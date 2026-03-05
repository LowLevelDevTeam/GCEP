#include "VulkanRHI.hpp"

// Externals
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>

// STL
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <Log/Log.hpp>
#include <Maths/Utils/vector3_convertor.hpp>

namespace gcep::rhi::vulkan
{

// ============================================================================
// Constructor
// ============================================================================

VulkanRHI::VulkanRHI(const SwapchainDesc& desc) : m_swapchainDesc(desc) {}

// ============================================================================
// gcep::RHI - public interface
// ============================================================================

static void framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/)
{
    auto* rhi = static_cast<VulkanRHI*>(glfwGetWindowUserPointer(window));
    if (rhi)
        rhi->setFramebufferResized(true);
}

void VulkanRHI::setWindow(GLFWwindow* window)
{
    m_swapchainDesc.nativeWindowHandle = window;
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void VulkanRHI::initRHI()
{
    m_device.createVulkanDevice(m_swapchainDesc);

    vk::CommandPoolCreateInfo createInfo{};
    createInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    createInfo.queueFamilyIndex = m_device.queueFamily();
    m_uploadCommandPool         = vk::raii::CommandPool(m_device.rawDevice(), createInfo);

    Log::info("Scene resources initialization started");
    initSceneResources();
    Log::info("Scene resources initialization completed successfully");
    Log::info("Renderer initialized successfully");
}

void VulkanRHI::drawFrame()
{
    processPendingOffscreenResize();
    if (m_framebufferResized)
        recreateSwapchain();

    if (!m_device.beginFrame()) return;

    perFrameUpdate();

    const bool offscreenReady =
        (*m_offscreenImage != VK_NULL_HANDLE) &&
        (*m_offscreenImageView != VK_NULL_HANDLE) &&
        (*m_offscreenDepthImage != VK_NULL_HANDLE) &&
        (*m_offscreenDepthImageView != VK_NULL_HANDLE);

    if (offscreenReady)
        recordOffscreenCommandBuffer();

    ImGui::Render();
    recordImGuiCommandBuffer();

    m_device.endFrame();
}

void VulkanRHI::cleanup()
{
    m_device.waitIdle();

    if (m_imguiTextureDescriptor != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(m_imguiTextureDescriptor);
        m_imguiTextureDescriptor = VK_NULL_HANDLE;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPoolImGui != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(*m_device.rawDevice(), m_descriptorPoolImGui, nullptr);
        m_descriptorPoolImGui = VK_NULL_HANDLE;
    }
    Log::info("Renderer cleaned up");
}

// ============================================================================
// Editor / window surface
// ============================================================================

void VulkanRHI::updateCameraUBO(UniformBufferObject ubo)
{
    m_prevViewProj = m_cameraUBO.proj * m_cameraUBO.view;

    auto jitter = haltonJitter(
        m_taaFrameIndex++,
        m_offscreenExtent.width,
        m_offscreenExtent.height
    );
    ubo.proj[2][0] += jitter.x;
    ubo.proj[2][1] += jitter.y;

    m_cameraUBO = ubo;
}

void VulkanRHI::updateSceneUBO(rhi::vulkan::SceneInfos* upstr, glm::vec3 cameraPos)
{
    m_clearColor            = upstr->clearColor;
    m_sceneUBO.lightDir     = upstr->lightDirection;
    m_sceneUBO.lightColor   = upstr->lightColor;
    m_sceneUBO.ambientColor = upstr->ambientColor;
    m_sceneUBO.cameraPos    = cameraPos;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        std::memcpy(m_uniformBuffersMapped[i], &m_sceneUBO, sizeof(m_sceneUBO));
}

void VulkanRHI::requestOffscreenResize(uint32_t width, uint32_t height)
{
    if (width <= 0 || height <= 0) return;
    if (width == m_offscreenExtent.width && height == m_offscreenExtent.height) return;

    m_offscreenResizePending = true;
    m_pendingOffscreenWidth  = width;
    m_pendingOffscreenHeight = height;
}

void VulkanRHI::setVSync(bool vsync)
{
    m_swapchainDesc.vsync = vsync;
    recreateSwapchain(true);
    Log::info(std::format("V-Sync mode set to {}", vsync));
}

bool VulkanRHI::isVSync()
{
    return m_swapchainDesc.vsync;
}

// ============================================================================
// Scene management
// ============================================================================

void VulkanRHI::spawnCube(glm::vec3 pos)
{
    assert(meshes.size() < MAX_MESHES && "Too many meshes");

    char* path    = (char*)"TestTextures/cube.obj";
    char* texPath = (char*)"TestTextures/white.png";
    spawnAsset(path, pos);
    addTexture(meshes.size() - 1, texPath, false);
}

void VulkanRHI::spawnAsset(char* filepath, glm::vec3 pos)
{
    if (!std::filesystem::exists(filepath))
    {
        return;
    }
    assert(meshes.size() < MAX_MESHES && "Too many meshes");

    const auto id    = m_ECSRegistry.createEntity();
    const auto index = static_cast<ECS::EntityID>(meshes.size());

    meshes.emplace_back();
    auto& mesh     = meshes.back();
    mesh.transform = m_ECSRegistry.addComponent<TransformComponent>(id);
    mesh.physics   = m_ECSRegistry.addComponent<PhysicsComponent>(id);
    mesh.id        = id;
    mesh.name      = std::filesystem::path(filepath).filename().string() + " / id = " + std::to_string(id);

    mesh.load(this, filepath);
    uploadSingleMesh(index);

    Vector3<float> halfExtents = { mesh.getAABBMax().x, mesh.getAABBMax().y, mesh.getAABBMax().z };

    m_ECSRegistry.getComponent<TransformComponent>(id).position = { pos.x, pos.y, pos.z };
    m_ECSRegistry.getComponent<TransformComponent>(id).scale    = halfExtents;
    m_ECSRegistry.getComponent<PhysicsComponent>(id).motionType = EMotionType::STATIC;
    m_ECSRegistry.getComponent<PhysicsComponent>(id).layers     = ELayers::NON_MOVING;

    mesh.transform = m_ECSRegistry.getComponent<TransformComponent>(id);
    mesh.physics   = m_ECSRegistry.getComponent<PhysicsComponent>(id);
}

void VulkanRHI::removeMesh(uint32_t meshIndex)
{
    assert(meshIndex < meshes.size() && "meshIndex out of range");

    auto& dying = meshes[meshIndex];
    bool shouldCompact = m_meshCache.release(dying.objPath());

    if (shouldCompact)
    {
        auto& deadCmd = m_indirectCommands[meshIndex];
        compactGeometry(
            static_cast<uint32_t>(deadCmd.vertexOffset),
            deadCmd.firstIndex,
            dying.getNumVertices(),
            dying.getNumIndices()
        );
    }

    m_ECSRegistry.destroyEntity(dying.id);
    m_ECSRegistry.update();

    meshes.erase(meshes.begin() + meshIndex);
    m_meshData.erase(m_meshData.begin() + meshIndex);
    m_indirectCommands.erase(m_indirectCommands.begin() + meshIndex);

    const uint32_t remaining = static_cast<uint32_t>(meshes.size());

    for (uint32_t i = meshIndex; i < remaining; ++i)
    {
        m_indirectCommands[i].firstInstance = i;
        static_cast<GPUMeshData*>(m_meshDataSSBOMapped)[i]                      = m_meshData[i];
        static_cast<vk::DrawIndexedIndirectCommand*>(m_indirectBufferMapped)[i] = m_indirectCommands[i];
    }

    const uint32_t deadSlot = remaining;
    static_cast<GPUMeshData*>(m_meshDataSSBOMapped)[deadSlot]                      = GPUMeshData{};
    static_cast<vk::DrawIndexedIndirectCommand*>(m_indirectBufferMapped)[deadSlot] = vk::DrawIndexedIndirectCommand{};
}

void VulkanRHI::addTexture(ECS::EntityID id, char* path, bool mipmaps)
{
    if (!std::filesystem::exists(path)) return;

    auto* mesh = findMesh(id);
    if (!mesh) return;

    mesh->texture()->loadTexture(this, path, mipmaps);
    Log::info(std::format("Loaded texture {}, mipmaps = {}", std::string(path), mipmaps));
}

void VulkanRHI::setGridPC(GridPushConstant* gridPC)
{
    m_gridPC = *gridPC;
}

void VulkanRHI::setSimulationStarted(bool started)
{
    simulationStarted = started;
}

// ============================================================================
// Queries
// ============================================================================

Mesh* VulkanRHI::findMesh(ECS::EntityID id)
{
    auto it = std::ranges::find_if(meshes, [id](const Mesh& m){ return m.id == id; });
    return it != meshes.end() ? &(*it) : nullptr;
}

InitInfos* VulkanRHI::getInitInfos()
{
    m_initInfos.instance = this;
    m_initInfos.meshData = &meshes;
    m_initInfos.registry = &m_ECSRegistry;
    m_initInfos.ds       = &m_imguiTextureDescriptor;
    return &m_initInfos;
}

uint32_t VulkanRHI::getDrawCount()
{
    return m_drawCount;
}

// ============================================================================
// ImGui integration
// ============================================================================

ImGui_ImplVulkan_InitInfo VulkanRHI::getUIInitInfo()
{
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 },
    };
    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCI.maxSets       = 1000;
    poolCI.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolCI.pPoolSizes    = poolSizes;

    if (vkCreateDescriptorPool(*m_device.rawDevice(), &poolCI, nullptr, &m_descriptorPoolImGui) != VK_SUCCESS)
        throw std::runtime_error("[VulkanRHI] Failed to create ImGui descriptor pool");

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance       = *m_device.rawInstance();
    initInfo.PhysicalDevice = *m_device.rawPhysDevice();
    initInfo.Device         = *m_device.rawDevice();
    initInfo.QueueFamily    = m_device.queueFamily();
    initInfo.Queue          = *m_device.rawQueue();
    initInfo.DescriptorPool = m_descriptorPoolImGui;
    initInfo.PipelineCache  = VK_NULL_HANDLE;
    initInfo.MinImageCount  = m_device.swapchainImageCount();
    initInfo.ImageCount     = m_device.swapchainImageCount();
    initInfo.Allocator      = nullptr;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount   = 1;
    m_swapchainFormat = m_device.swapchainFormat();
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = reinterpret_cast<VkFormat*>(&m_swapchainFormat);
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat   = static_cast<VkFormat>(findDepthFormat());
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    return initInfo;
}

void VulkanRHI::processPendingOffscreenResize()
{
    if (m_offscreenResizePending)
    {
        m_offscreenResizePending = false;
        resizeOffscreenResources(m_pendingOffscreenWidth, m_pendingOffscreenHeight);
    }
}

// ============================================================================
// Private - init helpers
// ============================================================================

void VulkanRHI::initSceneResources()
{
    // Scene UBO
    createSceneUBOLayout();
    createSceneUBOPool();
    createSceneUBOBuffers();
    createSceneUBOSets();
    // Bindless textures
    createBindlessLayout();
    createBindlessPool();
    createBindlessSet();
    initMeshBuffers();
    createMeshDataDescriptors();
    // Pipelines
    createGraphicsPipeline(); // Vertex / Fragment shader - general purpose
    createCullPipeline();     // Compute shader - Frustum culling
    createGridPipeline();     // Vertex / Fragment shader - editor grid
    // Base scene - TEST -- TODO: Move that
    spawnCube({0, 0, 10});
    spawnCube({0, 0, 0});
    meshes.at(1).setScale({10.0f, 10.0f, 1.0f});
    m_ECSRegistry.getComponent<TransformComponent>(1).scale = Vector3<float>(10.0f, 10.0f, 1.0f);
}

void VulkanRHI::recreateSwapchain(bool forVSync)
{
    m_framebufferResized = false;
    auto* win = static_cast<GLFWwindow*>(m_swapchainDesc.nativeWindowHandle);
    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    while (w == 0 || h == 0)
    {
        glfwGetFramebufferSize(win, &w, &h);
        glfwWaitEvents();
    }
    m_device.recreateSwapchain(static_cast<uint32_t>(w), static_cast<uint32_t>(h), m_swapchainDesc);
    if (!forVSync)
        Log::info(std::format("Window resized to {}x{}", w, h));
}

// ============================================================================
// Private - scene UBO
// ============================================================================

void VulkanRHI::createSceneUBOLayout()
{
    std::array bindings =
    {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
    };
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());
    m_UBOLayout = vk::raii::DescriptorSetLayout(m_device.rawDevice(), layoutInfo);
}

void VulkanRHI::createSceneUBOPool()
{
    std::array poolSizes =
    {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT)
    };
    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    m_UBOPool = vk::raii::DescriptorPool(m_device.rawDevice(), poolInfo);
}

void VulkanRHI::createSceneUBOBuffers()
{
    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();
    m_uniformBuffersMapped.clear();

    const vk::DeviceSize bufferSize = sizeof(SceneUBO);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vk::raii::Buffer       buffer       = nullptr;
        vk::raii::DeviceMemory bufferMemory = nullptr;

        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            buffer, bufferMemory
        );

        m_uniformBuffers.emplace_back(std::move(buffer));
        m_uniformBuffersMemory.emplace_back(std::move(bufferMemory));
        m_uniformBuffersMapped.emplace_back(m_uniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
}

void VulkanRHI::createSceneUBOSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *m_UBOLayout);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool     = *m_UBOPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts        = layouts.data();

    m_UBOSets.clear();
    m_UBOSets = m_device.rawDevice().allocateDescriptorSets(allocInfo);

    updateSceneUBOSets();
}

void VulkanRHI::updateSceneUBOSets()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = *m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(SceneUBO);

        vk::WriteDescriptorSet uboWrite{};
        uboWrite.dstSet          = m_UBOSets[i];
        uboWrite.dstBinding      = 0;
        uboWrite.descriptorCount = 1;
        uboWrite.descriptorType  = vk::DescriptorType::eUniformBuffer;
        uboWrite.pBufferInfo     = &bufferInfo;

        m_device.rawDevice().updateDescriptorSets({ uboWrite }, {});
    }
}

// ============================================================================
// Private - bindless textures
// ============================================================================

void VulkanRHI::createBindlessLayout()
{
    vk::DescriptorSetLayoutBinding binding{};
    binding.binding         = 0;
    binding.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    binding.descriptorCount = MAX_MESHES;
    binding.stageFlags      = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorBindingFlags bindingFlags =
        vk::DescriptorBindingFlagBits::ePartiallyBound |
        vk::DescriptorBindingFlagBits::eVariableDescriptorCount |
        vk::DescriptorBindingFlagBits::eUpdateAfterBind;

    vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
    flagsInfo.bindingCount  = 1;
    flagsInfo.pBindingFlags = &bindingFlags;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.pNext        = &flagsInfo;
    layoutInfo.flags        = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    m_bindlessLayout = vk::raii::DescriptorSetLayout(m_device.rawDevice(), layoutInfo);
}

void VulkanRHI::createBindlessPool()
{
    vk::DescriptorPoolSize poolSize{};
    poolSize.type            = vk::DescriptorType::eCombinedImageSampler;
    poolSize.descriptorCount = MAX_MESHES;

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags         = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;

    m_bindlessPool = vk::raii::DescriptorPool(m_device.rawDevice(), poolInfo);
}

void VulkanRHI::createBindlessSet()
{
    uint32_t variableCount = MAX_MESHES;

    vk::DescriptorSetVariableDescriptorCountAllocateInfo varCountInfo{};
    varCountInfo.descriptorSetCount = 1;
    varCountInfo.pDescriptorCounts  = &variableCount;

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.pNext              = &varCountInfo;
    allocInfo.descriptorPool     = *m_bindlessPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &(*m_bindlessLayout);

    m_bindlessSet = std::move(m_device.rawDevice().allocateDescriptorSets(allocInfo)[0]);
}

// ============================================================================
// Private - mesh / geometry buffers
// ============================================================================

void VulkanRHI::initMeshBuffers()
{
    createBuffer(MAX_VERTEX_BUFFER_BYTES,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_globalVertexBuffer, m_globalVertexBufferMemory
    );

    createBuffer(MAX_INDEX_BUFFER_BYTES,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_globalIndexBuffer, m_globalIndexBufferMemory
    );

    const vk::DeviceSize meshDataSize = sizeof(GPUMeshData) * MAX_MESHES;
    createBuffer(meshDataSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        m_meshDataSSBO, m_meshDataSSBOMemory
    );
    m_meshDataSSBOMapped = m_meshDataSSBOMemory.mapMemory(0, meshDataSize);

    m_indirectBufferCapacity = sizeof(vk::DrawIndexedIndirectCommand) * MAX_MESHES;
    createBuffer(m_indirectBufferCapacity,
        vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
        vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        m_indirectBuffer, m_indirectBufferMemory
    );
    m_indirectBufferMapped = m_indirectBufferMemory.mapMemory(0, m_indirectBufferCapacity);

    for (uint32_t i = 0; i < static_cast<uint32_t>(meshes.size()); ++i)
        uploadSingleMesh(i);
}

void VulkanRHI::uploadSingleMesh(uint32_t meshIndex)
{
    assert(meshIndex < MAX_MESHES && "Exceeded MAX_MESHES");

    auto& mesh = meshes[meshIndex];

    GeometryRegion* cached    = m_meshCache.find(mesh.objPath());
    bool geometryShared       = (cached != nullptr);
    uint32_t vertexOffset     = 0;
    uint32_t firstIndex       = 0;

    if (geometryShared)
    {
        GeometryRegion& r = m_meshCache.addRef(mesh.objPath());
        vertexOffset      = r.vertexOffset;
        firstIndex        = r.firstIndex;

        mesh.clearVertices();
        mesh.clearIndices();
    }
    else
    {
        const vk::DeviceSize vbSize = mesh.getVertexBufferSize();
        const vk::DeviceSize ibSize = mesh.getIndexBufferSize();

        assert(m_vertexBytesFilled + vbSize <= MAX_VERTEX_BUFFER_BYTES && "Vertex buffer overflow");
        assert(m_indexBytesFilled  + ibSize <= MAX_INDEX_BUFFER_BYTES  && "Index buffer overflow");

        {
            vk::raii::Buffer       staging({});
            vk::raii::DeviceMemory stagingMem({});
            createBuffer(vbSize,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging, stagingMem
            );

            void* ptr = stagingMem.mapMemory(0, vbSize);
            std::memcpy(ptr, mesh.getVertices().data(), vbSize);
            stagingMem.unmapMemory();

            auto cmd = beginSingleTimeCommands();
            vk::BufferCopy copy{};
            copy.srcOffset = 0;
            copy.dstOffset = m_vertexBytesFilled;
            copy.size      = vbSize;
            cmd->copyBuffer(*staging, *m_globalVertexBuffer, { copy });
            endSingleTimeCommands(*cmd);
        }

        {
            vk::raii::Buffer       staging({});
            vk::raii::DeviceMemory stagingMem({});
            createBuffer(ibSize,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging, stagingMem
            );

            void* ptr = stagingMem.mapMemory(0, ibSize);
            std::memcpy(ptr, mesh.getIndices().data(), ibSize);
            stagingMem.unmapMemory();

            auto cmd = beginSingleTimeCommands();
            vk::BufferCopy copy{};
            copy.srcOffset = 0;
            copy.dstOffset = m_indexBytesFilled;
            copy.size      = ibSize;
            cmd->copyBuffer(*staging, *m_globalIndexBuffer, { copy });
            endSingleTimeCommands(*cmd);
        }

        vertexOffset = m_totalVertexCount;
        firstIndex   = m_totalIndexCount;

        GeometryRegion region{};
        region.vertexOffset = vertexOffset;
        region.firstIndex   = firstIndex;
        region.vertexCount  = static_cast<uint32_t>(mesh.getNumVertices());
        region.indexCount   = static_cast<uint32_t>(mesh.getNumIndices());
        m_meshCache.insert(mesh.objPath(), region);

        m_vertexBytesFilled += vbSize;
        m_indexBytesFilled  += ibSize;
        m_totalVertexCount  += mesh.getNumVertices();
        m_totalIndexCount   += mesh.getNumIndices();

        mesh.clearVertices();
        mesh.clearIndices();
    }

    GPUMeshData data{};
    data.firstIndex   = firstIndex;
    data.indexCount   = mesh.getNumIndices();
    data.vertexOffset = vertexOffset;
    data.textureIndex = mesh.texture()->getBindlessIndex();
    data.transform    = mesh.getTransform();
    data.aabbMin      = mesh.getAABBMin();
    data.aabbMax      = mesh.getAABBMax();

    if (meshIndex < m_meshData.size())
        m_meshData[meshIndex] = data;
    else
        m_meshData.emplace_back(data);

    static_cast<GPUMeshData*>(m_meshDataSSBOMapped)[meshIndex] = data;

    vk::DrawIndexedIndirectCommand drawCmd{};
    drawCmd.indexCount    = data.indexCount;
    drawCmd.instanceCount = 1;
    drawCmd.firstIndex    = data.firstIndex;
    drawCmd.vertexOffset  = static_cast<int32_t>(data.vertexOffset);
    drawCmd.firstInstance = meshIndex;

    if (meshIndex < m_indirectCommands.size())
        m_indirectCommands[meshIndex] = drawCmd;
    else
        m_indirectCommands.emplace_back(drawCmd);

    static_cast<vk::DrawIndexedIndirectCommand*>(m_indirectBufferMapped)[meshIndex] = drawCmd;
}

void VulkanRHI::updateMeshMetadata(uint32_t meshIndex)
{
    assert(meshIndex < m_meshData.size() && "Mesh index out of range");

    auto& mesh = meshes[meshIndex];
    auto& data = m_meshData[meshIndex];

    data.textureIndex = mesh.texture()->getBindlessIndex();
    data.transform    = mesh.getTransform();
    data.aabbMin      = mesh.getAABBMin();
    data.aabbMax      = mesh.getAABBMax();

    static_cast<GPUMeshData*>(m_meshDataSSBOMapped)[meshIndex] = data;
}

void VulkanRHI::updateMeshDataSSBO()
{
    const vk::DeviceSize size = sizeof(GPUMeshData) * m_meshData.size();
    std::memcpy(m_meshDataSSBOMapped, m_meshData.data(), size);
}

void VulkanRHI::createMeshDataDescriptors()
{
    vk::DescriptorSetLayoutBinding binding{};
    binding.binding         = 0;
    binding.descriptorType  = vk::DescriptorType::eStorageBuffer;
    binding.descriptorCount = 1;
    binding.stageFlags      = vk::ShaderStageFlagBits::eVertex;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;
    m_meshDataDescriptorSetLayout = vk::raii::DescriptorSetLayout(m_device.rawDevice(), layoutInfo);

    vk::DescriptorPoolSize poolSize{};
    poolSize.type            = vk::DescriptorType::eStorageBuffer;
    poolSize.descriptorCount = 1;

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    m_meshDataDescriptorPool = vk::raii::DescriptorPool(m_device.rawDevice(), poolInfo);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool     = *m_meshDataDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &(*m_meshDataDescriptorSetLayout);
    m_meshDataDescriptorSet = std::move(m_device.rawDevice().allocateDescriptorSets(allocInfo)[0]);

    updateMeshDataSet();
}

void VulkanRHI::updateMeshDataSet()
{
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = *m_meshDataSSBO;
    bufferInfo.offset = 0;
    bufferInfo.range  = vk::WholeSize;

    vk::WriteDescriptorSet write{};
    write.dstSet          = *m_meshDataDescriptorSet;
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = vk::DescriptorType::eStorageBuffer;
    write.pBufferInfo     = &bufferInfo;

    m_device.rawDevice().updateDescriptorSets({ write }, {});
}

void VulkanRHI::refreshMeshData()
{
    for (uint32_t i = 0; i < meshes.size(); ++i)
        updateMeshMetadata(i);
    updateMeshDataSSBO();
}

// ============================================================================
// Private - GPU-driven culling compute pipeline
// ============================================================================

void VulkanRHI::createCullPipeline()
{
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings{};

    bindings[0].binding         = 0;
    bindings[0].descriptorType  = vk::DescriptorType::eStorageBuffer;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = vk::ShaderStageFlagBits::eCompute;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = vk::DescriptorType::eStorageBuffer;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = vk::ShaderStageFlagBits::eCompute;

    bindings[2].binding         = 2;
    bindings[2].descriptorType  = vk::DescriptorType::eStorageBuffer;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags      = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();
    m_cullSetLayout = vk::raii::DescriptorSetLayout(m_device.rawDevice(), layoutInfo);

    vk::DescriptorPoolSize poolSize{};
    poolSize.type            = vk::DescriptorType::eStorageBuffer;
    poolSize.descriptorCount = 3;

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    m_cullPool = vk::raii::DescriptorPool(m_device.rawDevice(), poolInfo);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool     = *m_cullPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &(*m_cullSetLayout);
    m_cullSet = std::move(m_device.rawDevice().allocateDescriptorSets(allocInfo)[0]);

    createBuffer(sizeof(uint32_t),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        m_drawCountBuffer, m_drawCountBufferMemory
    );
    m_drawCountMapped = m_drawCountBufferMemory.mapMemory(0, sizeof(uint32_t));

    vk::DescriptorBufferInfo objectsInfo{};
    objectsInfo.buffer = *m_meshDataSSBO;
    objectsInfo.offset = 0;
    objectsInfo.range  = vk::WholeSize;

    vk::DescriptorBufferInfo commandsInfo{};
    commandsInfo.buffer = *m_indirectBuffer;
    commandsInfo.offset = 0;
    commandsInfo.range  = vk::WholeSize;

    vk::DescriptorBufferInfo countInfo{};
    countInfo.buffer = *m_drawCountBuffer;
    countInfo.offset = 0;
    countInfo.range  = vk::WholeSize;

    std::array<vk::WriteDescriptorSet, 3> writes{};
    writes[0] = { *m_cullSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &objectsInfo };
    writes[1] = { *m_cullSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &commandsInfo };
    writes[2] = { *m_cullSet, 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &countInfo };
    m_device.rawDevice().updateDescriptorSets(writes, {});

    vk::PushConstantRange pushRange{};
    pushRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    pushRange.offset     = 0;
    pushRange.size       = sizeof(FrustumPlanes);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &(*m_cullSetLayout);
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushRange;
    m_cullPipelineLayout = vk::raii::PipelineLayout(m_device.rawDevice(), pipelineLayoutInfo);

    auto shaderCode = readShader("Compute_FrustumCulling.spv");
    vk::raii::ShaderModule cullModule = createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo stageInfo{};
    stageInfo.stage  = vk::ShaderStageFlagBits::eCompute;
    stageInfo.module = *cullModule;
    stageInfo.pName  = "compMain";

    vk::ComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = *m_cullPipelineLayout;

    m_cullPipeline = vk::raii::Pipeline(m_device.rawDevice(), nullptr, pipelineInfo);
}

void VulkanRHI::recordCullPass()
{
    const vk::CommandBuffer cmd = m_device.currentCommandBuffer();

    m_drawCount = *static_cast<uint32_t*>(m_drawCountMapped);
    uint32_t zero = 0;
    std::memcpy(m_drawCountMapped, &zero, sizeof(uint32_t));

    vk::BufferMemoryBarrier2 resetBarrier{};
    resetBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eHost;
    resetBarrier.srcAccessMask = vk::AccessFlagBits2::eHostWrite;
    resetBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
    resetBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
    resetBarrier.buffer        = *m_drawCountBuffer;
    resetBarrier.size          = vk::WholeSize;

    vk::BufferMemoryBarrier2 indirectWriteBarrier{};
    indirectWriteBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eDrawIndirect;
    indirectWriteBarrier.srcAccessMask = vk::AccessFlagBits2::eIndirectCommandRead;
    indirectWriteBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
    indirectWriteBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
    indirectWriteBarrier.buffer        = *m_indirectBuffer;
    indirectWriteBarrier.size          = vk::WholeSize;

    std::array preBarriers = { resetBarrier, indirectWriteBarrier };
    vk::DependencyInfo preDep{};
    preDep.bufferMemoryBarrierCount = static_cast<uint32_t>(preBarriers.size());
    preDep.pBufferMemoryBarriers    = preBarriers.data();
    cmd.pipelineBarrier2(preDep);

    constexpr int THREADS = 256; // Must match [numthreads(THREADS, 1, 1)] in FrustumCulling.slang
    FrustumPlanes frustum = extractFrustum(m_cameraUBO.proj * m_cameraUBO.view);

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *m_cullPipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_cullPipelineLayout, 0, { *m_cullSet }, {});
    cmd.pushConstants<FrustumPlanes>(*m_cullPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, frustum);

    const uint32_t groups = (static_cast<uint32_t>(m_meshData.size()) + (THREADS - 1)) / THREADS;
    cmd.dispatch(groups, 1, 1);

    vk::BufferMemoryBarrier2 indirectReadBarrier{};
    indirectReadBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
    indirectReadBarrier.srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
    indirectReadBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eDrawIndirect;
    indirectReadBarrier.dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead;
    indirectReadBarrier.buffer        = *m_indirectBuffer;
    indirectReadBarrier.size          = vk::WholeSize;

    vk::BufferMemoryBarrier2 countReadBarrier{};
    countReadBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
    countReadBarrier.srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
    countReadBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eDrawIndirect;
    countReadBarrier.dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead;
    countReadBarrier.buffer        = *m_drawCountBuffer;
    countReadBarrier.size          = vk::WholeSize;

    std::array postBarriers = { indirectReadBarrier, countReadBarrier };
    vk::DependencyInfo postDep{};
    postDep.bufferMemoryBarrierCount = static_cast<uint32_t>(postBarriers.size());
    postDep.pBufferMemoryBarriers    = postBarriers.data();
    cmd.pipelineBarrier2(postDep);
}

FrustumPlanes VulkanRHI::extractFrustum(const glm::mat4& viewProj) const
{
    FrustumPlanes f{};
    f.objectCount = static_cast<uint32_t>(m_meshData.size());

    const auto& m = viewProj;

    // Gribb-Hartmann extraction
    // See https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
    f.planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]); // left
    f.planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]); // right
    f.planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]); // bottom
    f.planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]); // top
    f.planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]); // near
    f.planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]); // far

    for (auto& p : f.planes)
    {
        float len = glm::length(glm::vec3(p));
        p /= len;
    }

    return f;
}

// ============================================================================
// Private - graphics pipeline
// ============================================================================

static struct PushConstant {
    glm::mat4 camViewProj;      // current (jittered) viewProj
    glm::mat4 prevCamViewProj;  // previous (unjittered) viewProj
} pc;

void VulkanRHI::createGraphicsPipeline()
{
    auto shaderCode = readShader("Base_Shader.spv");
    vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo{};
    vertexShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
    vertexShaderStageInfo.module = shaderModule;
    vertexShaderStageInfo.pName  = "vertMain";

    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo{};
    fragmentShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderStageInfo.module = shaderModule;
    fragmentShaderStageInfo.pName  = "fragMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageInfo, fragmentShaderStageInfo };

    std::vector dynStateList =
    {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynStateList.size());
    dynamicState.pDynamicStates    = dynStateList.data();

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable        = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode             = vk::PolygonMode::eFill;
    rasterizer.cullMode                = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace               = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable         = vk::False;
    rasterizer.depthBiasSlopeFactor    = 1.0f;
    rasterizer.lineWidth               = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable  = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable       = vk::True;
    depthStencil.depthWriteEnable      = vk::True;
    depthStencil.depthCompareOp        = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable     = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable    = vk::False;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                        | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;


    // The Vulkan spec states: [...] pColorBlendState->attachmentCount must be equal to
    // VkPipelineRenderingCreateInfo::colorAttachmentCount
    std::array<vk::PipelineColorBlendAttachmentState, 2> blendAttachments = { colorBlendAttachment, colorBlendAttachment };

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable   = vk::False;
    colorBlending.logicOp         = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 2;
    colorBlending.pAttachments    = blendAttachments.data();

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = static_cast<uint32_t>(sizeof(PushConstant));
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

    std::array<vk::DescriptorSetLayout, 3> layouts
    {
        *m_meshDataDescriptorSetLayout, // set = 0  per-mesh data
        *m_bindlessLayout,              // set = 1  bindless textures
        *m_UBOLayout                    // set = 2  scene UBO
    };
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount         = layouts.size();
    pipelineLayoutInfo.pSetLayouts            = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    m_pipelineLayout = vk::raii::PipelineLayout(m_device.rawDevice(), pipelineLayoutInfo);

    m_swapchainFormat = m_device.swapchainFormat();
    const std::array<vk::Format, 2> gfxColorFormats = {
        m_swapchainFormat,
        m_motionVectorsFormat
    };
    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount    = 2;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = gfxColorFormats.data();
    pipelineRenderingCreateInfo.depthAttachmentFormat   = findDepthFormat();

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pNext               = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = nullptr;

    m_graphicsPipeline = vk::raii::Pipeline(m_device.rawDevice(), nullptr, pipelineInfo);
}

// ============================================================================
// Private - grid pipeline
// ============================================================================

void VulkanRHI::createGridPipeline()
{
    vk::PushConstantRange pcRange{
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0,
        sizeof(GridPushConstant)
    };

    std::array<vk::DescriptorSetLayout, 1> setLayouts = { *m_UBOLayout };

    vk::PipelineLayoutCreateInfo layoutCI{};
    layoutCI.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
    layoutCI.pSetLayouts            = setLayouts.data();
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges    = &pcRange;

    m_gridPipelineLayout = vk::raii::PipelineLayout(m_device.rawDevice(), layoutCI);

    auto shaderCode   = readShader("Grid.spv");
    auto shaderModule = createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo{};
    vertexShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
    vertexShaderStageInfo.module = shaderModule;
    vertexShaderStageInfo.pName  = "vertMain";

    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo{};
    fragmentShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderStageInfo.module = shaderModule;
    fragmentShaderStageInfo.pName  = "fragMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageInfo, fragmentShaderStageInfo };

    vk::PipelineVertexInputStateCreateInfo   vertexInput{};
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ {}, vk::PrimitiveTopology::eTriangleList };

    vk::PipelineRasterizationStateCreateInfo raster{};
    raster.polygonMode = vk::PolygonMode::eFill;
    raster.cullMode    = vk::CullModeFlagBits::eNone;
    raster.frontFace   = vk::FrontFace::eCounterClockwise;
    raster.lineWidth   = 1.0f;

    vk::PipelineDepthStencilStateCreateInfo depth{};
    depth.depthTestEnable  = true;
    depth.depthWriteEnable = false;
    depth.depthCompareOp   = vk::CompareOp::eLess;

    vk::PipelineColorBlendAttachmentState blendAtt{};
    blendAtt.blendEnable         = true;
    blendAtt.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    blendAtt.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blendAtt.colorBlendOp        = vk::BlendOp::eAdd;
    blendAtt.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    blendAtt.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blendAtt.alphaBlendOp        = vk::BlendOp::eAdd;
    blendAtt.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                 | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    // Slot 1 must be identical to slot 0 (independentBlend not required).
    std::array<vk::PipelineColorBlendAttachmentState, 2> blendAtts = { blendAtt, blendAtt };

    vk::PipelineColorBlendStateCreateInfo blend{};
    blend.attachmentCount = 2;
    blend.pAttachments    = blendAtts.data();

    std::array<vk::DynamicState, 2> dynStates{ vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynState{ {}, dynStates };

    vk::Viewport vp{};
    vk::Rect2D   sc{};
    vk::PipelineViewportStateCreateInfo vpState{ {}, 1, &vp, 1, &sc };

    vk::PipelineMultisampleStateCreateInfo msaa{};
    msaa.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // Two colour attachments: slot 0 = scene colour (RGBA16F / swapchain format),
    // slot 1 = motion vectors (R16G16_SFLOAT).  When TAA is off, the second
    // attachment is unused but declared so the pipeline is compatible.
    const std::array<vk::Format, 2> colorFormats = {
        m_swapchainFormat,
        m_motionVectorsFormat
    };

    vk::PipelineRenderingCreateInfo renderingCI{};
    renderingCI.colorAttachmentCount    = 2;
    renderingCI.pColorAttachmentFormats = colorFormats.data();
    renderingCI.depthAttachmentFormat   = findDepthFormat();

    vk::GraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.pNext               = &renderingCI;
    pipelineCI.stageCount          = 2;
    pipelineCI.pStages             = shaderStages;
    pipelineCI.pVertexInputState   = &vertexInput;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState      = &vpState;
    pipelineCI.pRasterizationState = &raster;
    pipelineCI.pMultisampleState   = &msaa;
    pipelineCI.pDepthStencilState  = &depth;
    pipelineCI.pColorBlendState    = &blend;
    pipelineCI.pDynamicState       = &dynState;
    pipelineCI.layout              = *m_gridPipelineLayout;

    m_gridPipeline = std::move(m_device.rawDevice().createGraphicsPipeline(nullptr, pipelineCI));
}

void VulkanRHI::recordGridPass()
{
    const vk::CommandBuffer cmd = m_device.currentCommandBuffer();

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_gridPipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_gridPipelineLayout, 0, { *m_UBOSets[0] }, {});

    m_gridPC.invView  = glm::inverse(m_cameraUBO.view);
    m_gridPC.invProj  = glm::inverse(m_cameraUBO.proj);
    m_gridPC.viewProj = m_cameraUBO.proj * m_cameraUBO.view;

    cmd.pushConstants<GridPushConstant>(*m_gridPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, m_gridPC);

    cmd.draw(3, 1, 0, 0);
}

// ============================================================================
// Private - offscreen render target
// ============================================================================

void VulkanRHI::createOffscreenResources(uint32_t width, uint32_t height)
{
    m_offscreenExtent.width  = width;
    m_offscreenExtent.height = height;

    const uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    // Color image
    createImage(
        width, height, mipLevels,
        m_device.swapchainFormat(),
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_offscreenImage, m_offscreenImageMemory
    );

    m_offscreenImageView = createImageView(m_offscreenImage, m_device.swapchainFormat(), vk::ImageAspectFlagBits::eColor, mipLevels);

    // Depth image
    const vk::Format depthFormat = findDepthFormat();
    createImage(
        width, height, 1,
        depthFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_offscreenDepthImage, m_offscreenDepthImageMemory
    );
    m_offscreenDepthImageView = createImageView(m_offscreenDepthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);

    createImage(
        width, height, 1,
        m_motionVectorsFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment |
        vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_motionImage, m_motionImageMemory
    );
    m_motionImageView = createImageView(
        m_motionImage,
        m_motionVectorsFormat,
        vk::ImageAspectFlagBits::eColor, 1
    );

    {
        auto cmd = beginSingleTimeCommands();
        vk::ImageMemoryBarrier2 b{};
        b.srcStageMask     = vk::PipelineStageFlagBits2::eTopOfPipe;
        b.srcAccessMask    = vk::AccessFlagBits2::eNone;
        b.dstStageMask     = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        b.dstAccessMask    = vk::AccessFlagBits2::eColorAttachmentWrite;
        b.oldLayout        = vk::ImageLayout::eUndefined;
        b.newLayout        = vk::ImageLayout::eColorAttachmentOptimal;
        b.image            = *m_motionImage;
        b.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        vk::DependencyInfo dep{};
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers    = &b;
        cmd->pipelineBarrier2(dep);
        endSingleTimeCommands(*cmd);
    }

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter    = vk::Filter::eLinear;
    samplerInfo.minFilter    = vk::Filter::eLinear;
    samplerInfo.mipmapMode   = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.minLod       = 0.0f;
    samplerInfo.maxLod       = vk::LodClampNone;
    samplerInfo.maxAnisotropy = 1.0f;

    m_offscreenSampler = vk::raii::Sampler(m_device.rawDevice(), samplerInfo);

    {
        auto cmd = beginSingleTimeCommands();
        vk::ImageMemoryBarrier2 barrier{};
        barrier.srcStageMask  = vk::PipelineStageFlagBits2::eTopOfPipe;
        barrier.srcAccessMask = vk::AccessFlagBits2::eNone;
        barrier.dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
        barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        barrier.oldLayout     = vk::ImageLayout::eUndefined;
        barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.image         = m_offscreenImage;
        barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 };
        vk::DependencyInfo depInfo{};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers    = &barrier;
        cmd->pipelineBarrier2(depInfo);
        endSingleTimeCommands(*cmd);
    }

    createTAAResources(width, height);

    m_imguiTextureDescriptor = ImGui_ImplVulkan_AddTexture(
        *m_taaSampler,
        *m_taaResolvedImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void VulkanRHI::resizeOffscreenResources(uint32_t width, uint32_t height)
{
    if (width <= 0 || height <= 0) return;
    if (width == m_offscreenExtent.width && height == m_offscreenExtent.height) return;

    m_device.waitIdle();

    if (m_imguiTextureDescriptor != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(m_imguiTextureDescriptor);
        m_imguiTextureDescriptor = VK_NULL_HANDLE;
    }

    // Clear old resources
    destroyTAAResources();

    m_offscreenImageView        = nullptr;
    m_offscreenImage            = nullptr;
    m_offscreenImageMemory      = nullptr;
    m_offscreenSampler          = nullptr;
    m_offscreenDepthImageView   = nullptr;
    m_offscreenDepthImage       = nullptr;
    m_offscreenDepthImageMemory = nullptr;
    m_motionImageView           = nullptr;
    m_motionImage               = nullptr;
    m_motionImageMemory         = nullptr;

    // Recreate with new size
    createOffscreenResources(width, height);
}

// ============================================================================
// Private - per-frame recording
// ============================================================================

void VulkanRHI::perFrameUpdate()
{
    refreshMeshData();
}

void VulkanRHI::recordOffscreenCommandBuffer()
{
    const vk::CommandBuffer cmd = m_device.currentCommandBuffer();

    const uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_offscreenExtent.width, m_offscreenExtent.height)))) + 1;

    vk::ImageMemoryBarrier2 colorBarrier{};
    colorBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
    colorBarrier.srcAccessMask = vk::AccessFlagBits2::eShaderRead;
    colorBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    colorBarrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    colorBarrier.oldLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    colorBarrier.newLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    colorBarrier.image         = m_offscreenImage;
    colorBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 };

    vk::ImageMemoryBarrier2 depthBarrier{};
    depthBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    depthBarrier.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    depthBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    depthBarrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    depthBarrier.oldLayout     = vk::ImageLayout::eUndefined;
    depthBarrier.newLayout     = vk::ImageLayout::eDepthAttachmentOptimal;
    depthBarrier.image         = m_offscreenDepthImage;
    depthBarrier.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 };

    std::array barriers = { colorBarrier, depthBarrier };
    vk::DependencyInfo depInfo{};
    depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    depInfo.pImageMemoryBarriers    = barriers.data();
    cmd.pipelineBarrier2(depInfo);

    const vk::ClearValue clearColor = vk::ClearColorValue(m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w);
    const vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.imageView   = m_offscreenImageView;
    colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachmentInfo.clearValue  = clearColor;

    vk::ImageMemoryBarrier2 motionBarrier{};
    motionBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    motionBarrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    motionBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    motionBarrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    motionBarrier.oldLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    motionBarrier.newLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    motionBarrier.image         = *m_motionImage;
    motionBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    vk::DependencyInfo motionDep{};
    motionDep.imageMemoryBarrierCount = 1;
    motionDep.pImageMemoryBarriers    = &motionBarrier;
    cmd.pipelineBarrier2(motionDep);

    vk::RenderingAttachmentInfo motionAttachmentInfo{};
    motionAttachmentInfo.imageView   = *m_motionImageView;
    motionAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    motionAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    motionAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eStore;
    motionAttachmentInfo.clearValue  = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);

    vk::RenderingAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.imageView   = m_offscreenDepthImageView;
    depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    depthAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eDontCare;
    depthAttachmentInfo.clearValue  = clearDepth;

    vk::Rect2D renderArea{};
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent   = m_offscreenExtent;

    std::array<vk::RenderingAttachmentInfo, 2> colorAttachments = { colorAttachmentInfo, motionAttachmentInfo };

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea           = renderArea;
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 2;
    renderingInfo.pColorAttachments    = colorAttachments.data();
    renderingInfo.pDepthAttachment     = &depthAttachmentInfo;

    recordCullPass();

    cmd.beginRendering(renderingInfo);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);
    cmd.setViewport(0, vk::Viewport(
        0.0f, 0.0f,
        static_cast<float>(m_offscreenExtent.width),
        static_cast<float>(m_offscreenExtent.height),
        0.0f, 1.0f)
    );
    cmd.setScissor(0, vk::Rect2D({ 0, 0 }, m_offscreenExtent));
    cmd.bindVertexBuffers(0, { *m_globalVertexBuffer }, { 0 });
    cmd.bindIndexBuffer(*m_globalIndexBuffer, 0, vk::IndexType::eUint32);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipelineLayout, 0, { *m_meshDataDescriptorSet, *m_bindlessSet, *m_UBOSets[0] }, {});

    pc.camViewProj     = m_cameraUBO.proj * m_cameraUBO.view;
    pc.prevCamViewProj = m_prevViewProj;
    cmd.pushConstants<PushConstant>(*m_pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pc);

    cmd.drawIndexedIndirectCount(
        *m_indirectBuffer,  0,
        *m_drawCountBuffer, 0,
        static_cast<uint32_t>(m_indirectCommands.size()),
        sizeof(vk::DrawIndexedIndirectCommand)
    );

    if(!simulationStarted)
    {
        recordGridPass();
    }

    cmd.endRendering();

    vk::ImageMemoryBarrier2 toShaderRead{};
    toShaderRead.srcStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    toShaderRead.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    toShaderRead.dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
    toShaderRead.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    toShaderRead.oldLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    toShaderRead.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    toShaderRead.image         = *m_offscreenImage;
    toShaderRead.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 };

    vk::DependencyInfo dep2{};
    dep2.imageMemoryBarrierCount = 1;
    dep2.pImageMemoryBarriers    = &toShaderRead;
    cmd.pipelineBarrier2(dep2);

    recordTAAPass();
}

void VulkanRHI::recordImGuiCommandBuffer()
{
    const vk::CommandBuffer cmd       = m_device.currentCommandBuffer();
    const vk::Image         swapImage = m_device.currentSwapchainImage();

    transitionImageInFrame(swapImage,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits2::eNone, vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor, 1
    );

    const uint32_t      imageIdx = m_device.currentImageIndex();
    const vk::ImageView swapView = *m_device.swapchainImageViews()[imageIdx];

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView   = swapView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue  = vk::ClearColorValue(m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea.offset.x      = 0;
    renderingInfo.renderArea.offset.y      = 0;
    renderingInfo.renderArea.extent.width  = m_device.swapchainExtent().width;
    renderingInfo.renderArea.extent.height = m_device.swapchainExtent().height;
    renderingInfo.layerCount               = 1;
    renderingInfo.colorAttachmentCount     = 1;
    renderingInfo.pColorAttachments        = &colorAttachment;

    cmd.beginRendering(renderingInfo);

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

    cmd.endRendering();

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    transitionImageInFrame(swapImage,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor, 1
    );
}

// ============================================================================
// Private - compaction
// ============================================================================

void VulkanRHI::compactBuffer(vk::raii::Buffer& buffer,
                              vk::DeviceSize    removeByteOffset,
                              vk::DeviceSize    removeByteSize,
                              vk::DeviceSize&   totalBytesFilled)
{
    const vk::DeviceSize tailSize = totalBytesFilled - removeByteOffset - removeByteSize;
    if (tailSize == 0)
    {
        totalBytesFilled -= removeByteSize;
        return;
    }

    vk::raii::Buffer       scratch({});
    vk::raii::DeviceMemory scratchMem({});
    createBuffer(tailSize,
        vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        scratch, scratchMem
    );

    {
        auto cmd = beginSingleTimeCommands();
        cmd->copyBuffer(*buffer, *scratch, vk::BufferCopy{ removeByteOffset + removeByteSize, 0, tailSize });
        endSingleTimeCommands(*cmd);
    }

    {
        auto cmd = beginSingleTimeCommands();
        cmd->copyBuffer(*scratch, *buffer, vk::BufferCopy{ 0, removeByteOffset, tailSize });
        endSingleTimeCommands(*cmd);
    }

    totalBytesFilled -= removeByteSize;
}

void VulkanRHI::compactGeometry(uint32_t removedVertexOffset, uint32_t removedFirstIndex,
                                uint32_t removedVertexCount,  uint32_t removedIndexCount)
{
    compactBuffer(m_globalVertexBuffer,
        removedVertexOffset * sizeof(Vertex),
        removedVertexCount  * sizeof(Vertex),
        m_vertexBytesFilled
    );

    compactBuffer(m_globalIndexBuffer,
        removedFirstIndex  * sizeof(uint32_t),
        removedIndexCount  * sizeof(uint32_t),
        m_indexBytesFilled
    );

    m_totalVertexCount -= removedVertexCount;
    m_totalIndexCount  -= removedIndexCount;

    m_meshCache.patchAfterCompaction(
        removedVertexOffset, removedFirstIndex,
        removedVertexCount,  removedIndexCount
    );

    for (uint32_t i = 0; i < static_cast<uint32_t>(meshes.size()); ++i)
    {
        auto& data   = m_meshData[i];
        auto& cmd    = m_indirectCommands[i];
        bool patched = false;

        if (data.vertexOffset >= removedVertexOffset + removedVertexCount)
        {
            data.vertexOffset -= removedVertexCount;
            cmd.vertexOffset   = static_cast<int32_t>(data.vertexOffset);
            patched            = true;
        }
        if (data.firstIndex >= removedFirstIndex + removedIndexCount)
        {
            data.firstIndex -= removedIndexCount;
            cmd.firstIndex   = data.firstIndex;
            patched          = true;
        }

        if (patched)
        {
            static_cast<GPUMeshData*>(m_meshDataSSBOMapped)[i]                      = data;
            static_cast<vk::DrawIndexedIndirectCommand*>(m_indirectBufferMapped)[i] = cmd;
        }
    }
}

// ============================================================================
// Private - low-level Vulkan helpers
// ============================================================================

void VulkanRHI::createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                            vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                            vk::MemoryPropertyFlags properties,
                            vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
{
    vk::ImageCreateInfo createInfo{};
    createInfo.imageType     = vk::ImageType::e2D;
    createInfo.format        = format;
    createInfo.extent.width  = width;
    createInfo.extent.height = height;
    createInfo.extent.depth  = 1;
    createInfo.mipLevels     = mipLevels;
    createInfo.arrayLayers   = 1;
    createInfo.samples       = vk::SampleCountFlagBits::e1;
    createInfo.tiling        = tiling;
    createInfo.usage         = usage;
    createInfo.sharingMode   = vk::SharingMode::eExclusive;

    image = vk::raii::Image(m_device.rawDevice(), createInfo);
    const auto memoryRequirements = image.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize  = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

    imageMemory = vk::raii::DeviceMemory(m_device.rawDevice(), allocInfo);
    image.bindMemory(imageMemory, 0);
}

vk::raii::ImageView VulkanRHI::createImageView(vk::raii::Image& image, vk::Format format,
                                               vk::ImageAspectFlags aspectFlags, uint32_t mipLevels)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image            = *image;
    viewInfo.viewType         = vk::ImageViewType::e2D;
    viewInfo.format           = format;
    viewInfo.subresourceRange = { aspectFlags, 0, mipLevels, 0, 1 };

    return vk::raii::ImageView(m_device.rawDevice(), viewInfo);
}

void VulkanRHI::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                             vk::MemoryPropertyFlags properties,
                             vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufMem)
{
    vk::BufferCreateInfo createInfo{};
    createInfo.size        = size;
    createInfo.usage       = usage;
    createInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = vk::raii::Buffer(m_device.rawDevice(), createInfo);
    const auto memoryRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo(memoryRequirements.size, findMemoryType(memoryRequirements.memoryTypeBits, properties));
    bufMem = vk::raii::DeviceMemory(m_device.rawDevice(), allocInfo);
    buffer.bindMemory(*bufMem, 0);
}

void VulkanRHI::copyBuffer(vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size)
{
    auto cmd = beginSingleTimeCommands();
    cmd->copyBuffer(*src, *dst, vk::BufferCopy(0, 0, size));
    endSingleTimeCommands(*cmd);
}

void VulkanRHI::transitionImageInFrame(vk::Image image,
                                       vk::ImageLayout oldLayout,        vk::ImageLayout newLayout,
                                       vk::AccessFlags2 srcAccess,       vk::AccessFlags2 dstAccess,
                                       vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage,
                                       vk::ImageAspectFlags aspect,      uint32_t mipLevels)
{
    vk::ImageMemoryBarrier2 barrier{};
    barrier.srcStageMask        = srcStage;
    barrier.srcAccessMask       = srcAccess;
    barrier.dstStageMask        = dstStage;
    barrier.dstAccessMask       = dstAccess;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image               = image;
    barrier.subresourceRange    = { aspect, 0, mipLevels, 0, 1 };

    vk::DependencyInfo dependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers    = &barrier;

    m_device.currentCommandBuffer().pipelineBarrier2(dependencyInfo);
}

std::unique_ptr<vk::raii::CommandBuffer> VulkanRHI::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool        = *m_uploadCommandPool;
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    auto cmd = std::make_unique<vk::raii::CommandBuffer>(
        std::move(vk::raii::CommandBuffers(m_device.rawDevice(), allocInfo).front())
    );

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd->begin(beginInfo);
    return cmd;
}

void VulkanRHI::endSingleTimeCommands(vk::raii::CommandBuffer& cmd)
{
    cmd.end();
    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &*cmd;
    m_device.rawQueue().submit(submitInfo, nullptr);
    m_device.rawQueue().waitIdle();
}

uint32_t VulkanRHI::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags props) const
{
    const auto memProps = m_device.constRawPhysDevice().getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("[VulkanRHI] findMemoryType: no suitable type found");
}

vk::Format VulkanRHI::findDepthFormat() const
{
    return findSupportedFormat(
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

vk::Format VulkanRHI::findSupportedFormat(const std::vector<vk::Format>& candidates,
                                          vk::ImageTiling tiling, vk::FormatFeatureFlags features) const
{
    for (const auto fmt : candidates)
    {
        const auto fmtProps = m_device.constRawPhysDevice().getFormatProperties(fmt);
        if (tiling == vk::ImageTiling::eLinear  && (fmtProps.linearTilingFeatures  & features) == features) return fmt;
        if (tiling == vk::ImageTiling::eOptimal && (fmtProps.optimalTilingFeatures & features) == features) return fmt;
    }
    throw std::runtime_error("[VulkanRHI] findSupportedFormat: no suitable format found");
}

std::vector<char> VulkanRHI::readShader(const std::string& fileName)
{
    const auto path = std::filesystem::current_path() / "Shaders" / fileName;
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("[VulkanRHI] Cannot open shader: " + path.string());
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
}

vk::raii::ShaderModule VulkanRHI::createShaderModule(const std::vector<char>& code) const
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    return vk::raii::ShaderModule(m_device.rawDevice(), createInfo);
}

// ============================================================================
// TAA – createTAAResources
// ============================================================================

static struct TAAPushConstants
{
    glm::vec2 texelSize;
    float blendAlpha;
    float varianceGamma;
} taaPC;

void VulkanRHI::createTAAResources(uint32_t width, uint32_t height)
{
    // Previous resolved frame
    createImage(
        width, height, 1,
        vk::Format::eR16G16B16A16Sfloat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_taaHistoryImage, m_taaHistoryImageMemory
    );
    m_taaHistoryImageView = createImageView(m_taaHistoryImage,
                                             vk::Format::eR16G16B16A16Sfloat,
                                             vk::ImageAspectFlagBits::eColor, 1);

    // Resolved output
    createImage(
        width, height, 1,
        vk::Format::eR16G16B16A16Sfloat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_taaResolvedImage, m_taaResolvedImageMemory
    );
    m_taaResolvedImageView = createImageView(m_taaResolvedImage,
                                              vk::Format::eR16G16B16A16Sfloat,
                                              vk::ImageAspectFlagBits::eColor, 1);

    // Bilinear sampler for history
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter        = vk::Filter::eLinear;
    samplerInfo.minFilter        = vk::Filter::eLinear;
    samplerInfo.mipmapMode       = vk::SamplerMipmapMode::eNearest;
    samplerInfo.addressModeU     = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV     = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW     = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.maxAnisotropy    = 1.0f;
    samplerInfo.minLod           = 0.0f;
    samplerInfo.maxLod           = vk::LodClampNone;
    m_taaSampler = vk::raii::Sampler(m_device.rawDevice(), samplerInfo);

    {
        auto cmd = beginSingleTimeCommands();

        auto transitionSimple = [&](vk::Image img, vk::ImageLayout newLayout)
        {
            vk::ImageMemoryBarrier2 b{};
            b.srcStageMask        = vk::PipelineStageFlagBits2::eTopOfPipe;
            b.srcAccessMask       = vk::AccessFlagBits2::eNone;
            b.dstStageMask        = vk::PipelineStageFlagBits2::eComputeShader |
                                    vk::PipelineStageFlagBits2::eFragmentShader |
                                    vk::PipelineStageFlagBits2::eColorAttachmentOutput;
            b.dstAccessMask       = vk::AccessFlagBits2::eShaderRead |
                                    vk::AccessFlagBits2::eShaderWrite |
                                    vk::AccessFlagBits2::eColorAttachmentWrite;
            b.oldLayout           = vk::ImageLayout::eUndefined;
            b.newLayout           = newLayout;
            b.image               = img;
            b.subresourceRange    = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

            vk::DependencyInfo dep{};
            dep.imageMemoryBarrierCount = 1;
            dep.pImageMemoryBarriers    = &b;
            cmd->pipelineBarrier2(dep);
        };

        transitionSimple(*m_taaHistoryImage,  vk::ImageLayout::eShaderReadOnlyOptimal);
        transitionSimple(*m_taaResolvedImage, vk::ImageLayout::eShaderReadOnlyOptimal);

        endSingleTimeCommands(*cmd);
    }

    // Descriptor set layout (bindings 0-4 match TAA.slang)
    std::array<vk::DescriptorSetLayoutBinding, 5> bindings{};

    // binding 0 – currentColor   (Texture2D sampled)
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = vk::ShaderStageFlagBits::eCompute;

    // binding 1 – historyColor   (Texture2D sampled)
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = vk::ShaderStageFlagBits::eCompute;

    // binding 2 – motionVectors  (Texture2D sampled)
    bindings[2].binding         = 2;
    bindings[2].descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags      = vk::ShaderStageFlagBits::eCompute;

    // binding 3 – resolvedOutput (RWTexture2D storage)
    bindings[3].binding         = 3;
    bindings[3].descriptorType  = vk::DescriptorType::eStorageImage;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags      = vk::ShaderStageFlagBits::eCompute;

    // binding 4 – linearSampler  (immutable sampler)
    bindings[4].binding            = 4;
    bindings[4].descriptorType     = vk::DescriptorType::eSampler;
    bindings[4].descriptorCount    = 1;
    bindings[4].stageFlags         = vk::ShaderStageFlagBits::eCompute;
    const vk::Sampler immSampler   = *m_taaSampler;
    bindings[4].pImmutableSamplers = &immSampler;

    vk::DescriptorSetLayoutCreateInfo layoutCI{};
    layoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutCI.pBindings    = bindings.data();
    m_taaSetLayout = vk::raii::DescriptorSetLayout(m_device.rawDevice(), layoutCI);

    // Descriptor pool
    std::array<vk::DescriptorPoolSize, 3> poolSizes{};
    poolSizes[0] = { vk::DescriptorType::eCombinedImageSampler, 3 };
    poolSizes[1] = { vk::DescriptorType::eStorageImage,         1 };
    poolSizes[2] = { vk::DescriptorType::eSampler,              1 };

    vk::DescriptorPoolCreateInfo poolCI{};
    poolCI.maxSets       = 1;
    poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolCI.pPoolSizes    = poolSizes.data();
    poolCI.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    m_taaPool = vk::raii::DescriptorPool(m_device.rawDevice(), poolCI);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool     = *m_taaPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &*m_taaSetLayout;

    m_taaSet = std::move(vk::raii::DescriptorSets(m_device.rawDevice(), allocInfo).front());

    // Write descriptors
    vk::DescriptorImageInfo currentInfo{};
    currentInfo.sampler     = *m_taaSampler;
    currentInfo.imageView   = *m_offscreenImageView;
    currentInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::DescriptorImageInfo historyInfo{};
    historyInfo.sampler     = *m_taaSampler;
    historyInfo.imageView   = *m_taaHistoryImageView;
    historyInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::DescriptorImageInfo motionInfo{};
    motionInfo.sampler     = *m_taaSampler;
    motionInfo.imageView   = *m_motionImageView;
    motionInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::DescriptorImageInfo resolvedInfo{};
    resolvedInfo.imageView   = *m_taaResolvedImageView;
    resolvedInfo.imageLayout = vk::ImageLayout::eGeneral;

    std::array<vk::WriteDescriptorSet, 4> writes{};
    writes[0] = { *m_taaSet, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &currentInfo  };
    writes[1] = { *m_taaSet, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &historyInfo  };
    writes[2] = { *m_taaSet, 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &motionInfo   };
    writes[3] = { *m_taaSet, 3, 0, 1, vk::DescriptorType::eStorageImage,         &resolvedInfo };

    m_device.rawDevice().updateDescriptorSets(writes, {});

    // Push-constant layout + compute pipeline

    vk::PushConstantRange pcRange{};
    pcRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(TAAPushConstants);

    vk::PipelineLayoutCreateInfo plCI{};
    plCI.setLayoutCount         = 1;
    plCI.pSetLayouts            = &*m_taaSetLayout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &pcRange;
    m_taaPipelineLayout = vk::raii::PipelineLayout(m_device.rawDevice(), plCI);

    auto spv = readShader("Compute_TAA.spv");
    auto module = createShaderModule(spv);

    vk::PipelineShaderStageCreateInfo stageCI{};
    stageCI.stage  = vk::ShaderStageFlagBits::eCompute;
    stageCI.module = *module;
    stageCI.pName  = "compMain";

    vk::ComputePipelineCreateInfo cpCI{};
    cpCI.stage  = stageCI;
    cpCI.layout = *m_taaPipelineLayout;
    m_taaPipeline = std::move(m_device.rawDevice().createComputePipeline(nullptr, cpCI));

    Log::info("TAA resources created.");
}

// ============================================================================
// TAA – destroyTAAResources
// ============================================================================

void VulkanRHI::destroyTAAResources()
{
    m_device.waitIdle();

    m_taaPipeline       = nullptr;
    m_taaPipelineLayout = nullptr;
    m_taaSet            = nullptr;
    m_taaPool           = nullptr;
    m_taaSetLayout      = nullptr;
    m_taaSampler        = nullptr;

    m_taaResolvedImageView   = nullptr;
    m_taaResolvedImage       = nullptr;
    m_taaResolvedImageMemory = nullptr;

    m_taaHistoryImageView   = nullptr;
    m_taaHistoryImage       = nullptr;
    m_taaHistoryImageMemory = nullptr;
}

// ============================================================================
// TAA – recordTAAPass
// ============================================================================

void VulkanRHI::recordTAAPass()
{
    const vk::CommandBuffer cmd = m_device.currentCommandBuffer();

    std::array<vk::ImageMemoryBarrier2, 2> preBarriers{};

    preBarriers[0].srcStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    preBarriers[0].srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    preBarriers[0].dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
    preBarriers[0].dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    preBarriers[0].oldLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    preBarriers[0].newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    preBarriers[0].image         = *m_motionImage;
    preBarriers[0].subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    preBarriers[1].srcStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
    preBarriers[1].srcAccessMask = vk::AccessFlagBits2::eShaderRead;
    preBarriers[1].dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
    preBarriers[1].dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
    preBarriers[1].oldLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    preBarriers[1].newLayout     = vk::ImageLayout::eGeneral;
    preBarriers[1].image         = *m_taaResolvedImage;
    preBarriers[1].subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    vk::DependencyInfo preDep{};
    preDep.imageMemoryBarrierCount = static_cast<uint32_t>(preBarriers.size());
    preDep.pImageMemoryBarriers    = preBarriers.data();
    cmd.pipelineBarrier2(preDep);

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *m_taaPipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_taaPipelineLayout, 0, { *m_taaSet }, {});

    taaPC.texelSize     = { 1.0f / static_cast<float>(m_offscreenExtent.width),
                            1.0f / static_cast<float>(m_offscreenExtent.height) };
    taaPC.blendAlpha    = m_taaBlendAlpha;
    taaPC.varianceGamma = 1.0f;

    cmd.pushConstants<TAAPushConstants>(*m_taaPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, taaPC);

    const uint32_t groupX = (m_offscreenExtent.width  + 7u) / 8u;
    const uint32_t groupY = (m_offscreenExtent.height + 7u) / 8u;
    cmd.dispatch(groupX, groupY, 1);

    std::array<vk::ImageMemoryBarrier2, 2> copyBarriers{};

    copyBarriers[0].srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
    copyBarriers[0].srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
    copyBarriers[0].dstStageMask  = vk::PipelineStageFlagBits2::eTransfer;
    copyBarriers[0].dstAccessMask = vk::AccessFlagBits2::eTransferRead;
    copyBarriers[0].oldLayout     = vk::ImageLayout::eGeneral;
    copyBarriers[0].newLayout     = vk::ImageLayout::eTransferSrcOptimal;
    copyBarriers[0].image         = *m_taaResolvedImage;
    copyBarriers[0].subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    copyBarriers[1].srcStageMask  = vk::PipelineStageFlagBits2::eFragmentShader |
                                    vk::PipelineStageFlagBits2::eComputeShader;
    copyBarriers[1].srcAccessMask = vk::AccessFlagBits2::eShaderRead;
    copyBarriers[1].dstStageMask  = vk::PipelineStageFlagBits2::eTransfer;
    copyBarriers[1].dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
    copyBarriers[1].oldLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    copyBarriers[1].newLayout     = vk::ImageLayout::eTransferDstOptimal;
    copyBarriers[1].image         = *m_taaHistoryImage;
    copyBarriers[1].subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    vk::DependencyInfo copyDep{};
    copyDep.imageMemoryBarrierCount = static_cast<uint32_t>(copyBarriers.size());
    copyDep.pImageMemoryBarriers    = copyBarriers.data();
    cmd.pipelineBarrier2(copyDep);

    vk::ImageCopy copyRegion{};
    copyRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    copyRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    copyRegion.extent.width   = m_offscreenExtent.width;
    copyRegion.extent.height  = m_offscreenExtent.height;
    copyRegion.extent.depth   = 1;

    cmd.copyImage(*m_taaResolvedImage, vk::ImageLayout::eTransferSrcOptimal,
                  *m_taaHistoryImage,  vk::ImageLayout::eTransferDstOptimal,
                  copyRegion);

    std::array<vk::ImageMemoryBarrier2, 3> finalBarriers{};

    finalBarriers[0].srcStageMask  = vk::PipelineStageFlagBits2::eTransfer;
    finalBarriers[0].srcAccessMask = vk::AccessFlagBits2::eTransferRead;
    finalBarriers[0].dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
    finalBarriers[0].dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    finalBarriers[0].oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
    finalBarriers[0].newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    finalBarriers[0].image         = *m_taaResolvedImage;
    finalBarriers[0].subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    finalBarriers[1].srcStageMask  = vk::PipelineStageFlagBits2::eTransfer;
    finalBarriers[1].srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
    finalBarriers[1].dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader |
                                     vk::PipelineStageFlagBits2::eFragmentShader;
    finalBarriers[1].dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    finalBarriers[1].oldLayout     = vk::ImageLayout::eTransferDstOptimal;
    finalBarriers[1].newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    finalBarriers[1].image         = *m_taaHistoryImage;
    finalBarriers[1].subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    finalBarriers[2].srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
    finalBarriers[2].srcAccessMask = vk::AccessFlagBits2::eShaderRead;
    finalBarriers[2].dstStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    finalBarriers[2].dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    finalBarriers[2].oldLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    finalBarriers[2].newLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    finalBarriers[2].image         = *m_motionImage;
    finalBarriers[2].subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    vk::DependencyInfo finalDep{};
    finalDep.imageMemoryBarrierCount = static_cast<uint32_t>(finalBarriers.size());
    finalDep.pImageMemoryBarriers    = finalBarriers.data();
    cmd.pipelineBarrier2(finalDep);
}

// ============================================================================
// TAA – haltonJitter
// ============================================================================

glm::vec2 VulkanRHI::haltonJitter(uint32_t frameIndex, uint32_t width, uint32_t height)
{
    // Halton(2,3) pre-shifted to [-0.5, +0.5] in pixel space.
    static constexpr float halton2[16] = {
        -0.000000f,  -0.250000f,  0.250000f, -0.375000f,
         0.125000f,  -0.125000f,  0.375000f, -0.437500f,
         0.062500f,  -0.187500f,  0.312500f, -0.312500f,
         0.187500f,  -0.062500f,  0.437500f, -0.468750f,
    };
    static constexpr float halton3[16] = {
        -0.166667f,   0.166667f, -0.388889f,  -0.055556f,
         0.277778f,  -0.277778f,  0.055556f,   0.388889f,
        -0.462963f,  -0.129630f,  0.203704f,  -0.351852f,
        -0.018519f,   0.314815f, -0.240741f,   0.092593f,
    };

    const uint32_t idx = frameIndex % 16u;

    const float jitterX = halton2[idx] * 2.0f / float(width);
    const float jitterY = halton3[idx] * 2.0f / float(height);

    return { jitterX, jitterY };
}

} // Namespace gcep::rhi::vulkan
