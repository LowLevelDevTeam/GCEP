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

#include "ECS/Components/mesh_components.hpp"

namespace gcep::rhi::vulkan
{

// ============================================================================
// Constructor
// ============================================================================

VulkanRHI::VulkanRHI(const SwapchainDesc& desc) : m_swapchainDesc(desc) {}

// ============================================================================
// Destructor
// ============================================================================

VulkanRHI::~VulkanRHI()
{
    if (m_descriptorPoolImGui != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(*m_device.rawDevice(), m_descriptorPoolImGui, nullptr);
        m_descriptorPoolImGui = VK_NULL_HANDLE;
    }
    Log::info("Renderer cleaned up");
}

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
}

// ============================================================================
// Editor / window surface
// ============================================================================

void VulkanRHI::updateCameraUBO(UniformBufferObject ubo)
{
    m_prevViewProj = m_unjitteredViewProj;
    m_unjitteredViewProj = ubo.proj * ubo.view;

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
    m_sceneUBO.cellSize     = upstr->cellSize;
    m_sceneUBO.fadeDistance = upstr->fadeDistance;
    m_sceneUBO.thickEvery   = upstr->thickEvery;
    m_sceneUBO.lineWidth    = upstr->lineWidth;
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


void VulkanRHI::spawnAsset(char* filepath, ECS::EntityID id, glm::vec3 pos)
{
    if (!std::filesystem::exists(filepath))
    {
        return;
    }
    assert(meshes.size() < MAX_MESHES && "Too many meshes");

    const auto index = static_cast<ECS::EntityID>(meshes.size());

    meshes.emplace_back();
    auto& mesh     = meshes.back();
    mesh.transform = m_ECSRegistry->getComponent<ECS::Transform>(id);
    mesh.physics   = m_ECSRegistry->getComponent<ECS::PhysicsComponent>(id);
    mesh.id        = id;
    mesh.name      = std::filesystem::path(filepath).filename().string() + " / id = " + std::to_string(id);

    mesh.load(this, filepath);
    if(uploadSingleMesh(index) != 0)
    {
        meshes.pop_back();
        m_ECSRegistry->removeComponent<ECS::Transform>(id);
        m_ECSRegistry->removeComponent<ECS::PhysicsComponent>(id);
        m_ECSRegistry->update();
        return;
    }

    Vector3<float> halfExtents = { 1.0f, 1.0f, 1.0f };

    if (!m_ECSRegistry->hasComponent<ECS::MeshComponent>(id))
    {
        m_ECSRegistry->addComponent<ECS::MeshComponent>(id, filepath);
        m_ECSRegistry->getComponent<ECS::Transform>(id).position = { pos.x, pos.y, pos.z };
        m_ECSRegistry->getComponent<ECS::Transform>(id).scale    = halfExtents;
    }
    else
    {
        m_ECSRegistry->getComponent<ECS::MeshComponent>(id).filePath = filepath;
    }

    mesh.transform = m_ECSRegistry->getComponent<ECS::Transform>(id);
    mesh.physics   = m_ECSRegistry->getComponent<ECS::PhysicsComponent>(id);
}

void VulkanRHI::removeMesh(ECS::EntityID id)
{
    auto it = std::ranges::find_if(meshes, [id](const Mesh& m){ return m.id == id; });
    if (it == meshes.end()) return;

    const uint32_t meshIndex = static_cast<uint32_t>(std::distance(meshes.begin(), it));
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

    void VulkanRHI::uploadMesh(ECS::EntityID id, const char* objPath)
{
    if (!std::filesystem::exists(objPath)) return;
    assert(meshes.size() < MAX_MESHES && "Too many meshes");

    const auto index = static_cast<uint32_t>(meshes.size());

    meshes.emplace_back();
    auto& mesh = meshes.back();
    mesh.id    = id;
    mesh.name  = std::filesystem::path(objPath).filename().string()
               + " / id = " + std::to_string(id);

    mesh.transform = m_ECSRegistry->getComponent<ECS::Transform>(id);
    mesh.physics   = m_ECSRegistry->getComponent<ECS::PhysicsComponent>(id);

    mesh.load(this, objPath);

    if (uploadSingleMesh(index) != 0)
    {
        meshes.pop_back();
        return;
    }

    mesh.transform = m_ECSRegistry->getComponent<ECS::Transform>(id);
    mesh.physics   = m_ECSRegistry->getComponent<ECS::PhysicsComponent>(id);
}

void VulkanRHI::uploadTexture(ECS::EntityID id, const char* texPath, bool mipmaps)
{
    if (!std::filesystem::exists(texPath)) return;

    auto* mesh = findMesh(id);
    if (!mesh) return;

    mesh->texture()->loadTexture(this, texPath, mipmaps);
    Log::info(std::format("Loaded texture {}, mipmaps = {}", texPath, mipmaps));
}

void VulkanRHI::setSimulationStarted(bool started)
{
    simulationStarted = started;
}

void VulkanRHI::setRegistry(ECS::Registry *registry)
{
    m_ECSRegistry = registry;
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
    m_initInfos.registry = m_ECSRegistry;
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
    {
        auto err = std::runtime_error("[VulkanRHI] Failed to create ImGui descriptor pool");
        Log::handleException(err);
        throw err;
    }

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
    createGraphicsPipeline();     // Vertex / Fragment shader - general purpose
    createCullPipeline();         // Compute shader - Frustum culling
    createGridPipeline();         // Vertex / Fragment shader - editor grid
    createLightSpritePipeline();  // Vertex / Fragment shader - light billboard sprites
    createLightConePipeline();    // Vertex / Fragment shader - spot cone wireframe gizmo
    // Offscreen render target must exist before the light system so image views are valid.
    createOffscreenResources(m_swapchainDesc.width, m_swapchainDesc.height);
    // Light system - init after offscreen images are ready.
    m_lightSystem.init(
        m_device,
        m_offscreenExtent.width,
        m_offscreenExtent.height,
        *m_offscreenImageView,
        *m_offscreenDepthImageView,
        findDepthFormat(),
        m_device.swapchainFormat(),
        m_uploadCommandPool
    );
    createTAAResources(m_offscreenExtent.width,
        m_offscreenExtent.height);
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

} // Namespace gcep::rhi::vulkan
