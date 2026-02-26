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

namespace gcep::rhi::vulkan
{

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto* rhi = static_cast<VulkanRHI*>(glfwGetWindowUserPointer(window));
    if (rhi)
    {
        rhi->setFramebufferResized(true);
    }
}

VulkanRHI::VulkanRHI(const gcep::rhi::SwapchainDesc& desc) : m_swapchainDesc(desc) {}

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

    initSceneResources();
}

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
    // Textures loading TODO: Move to manager class + use ECS
    meshes.reserve(MAX_MESHES);
    meshes.emplace_back();
    meshes[0].loadMesh(this, "TestTextures/viking_room.obj", "TestTextures/viking_room.png");
    meshes[0].name = "Viking Room";
    setMeshData();
    createMeshDataDescriptors();
    createGraphicsPipeline();
    createCullPipeline();
}

void VulkanRHI::setMeshData()
{
    vk::DeviceSize totalVertexBytes = 0;
    vk::DeviceSize totalIndexBytes  = 0;
    for (const auto& iMesh : meshes)
    {
        totalVertexBytes += iMesh.getVertexBufferSize();
        totalIndexBytes  += iMesh.getIndexBufferSize();
    }

    vk::raii::Buffer       stagingVB({});
    vk::raii::DeviceMemory stagingVBMem({});
    createBuffer(totalVertexBytes,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingVB, stagingVBMem
    );

    vk::raii::Buffer       stagingIB({});
    vk::raii::DeviceMemory stagingIBMem({});
    createBuffer(totalIndexBytes,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingIB, stagingIBMem
    );

    createBuffer(totalVertexBytes,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_globalVertexBuffer, m_globalVertexBufferMemory
    );

    createBuffer(totalIndexBytes,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_globalIndexBuffer, m_globalIndexBufferMemory
    );

    m_meshData.reserve(meshes.size());

    auto* vbPtr = static_cast<uint8_t*>(stagingVBMem.mapMemory(0, totalVertexBytes));
    auto* ibPtr = static_cast<uint8_t*>(stagingIBMem.mapMemory(0, totalIndexBytes));

    vk::DeviceSize vertexByteOffset = 0;
    vk::DeviceSize indexByteOffset  = 0;
    uint32_t       vertexCount      = 0;
    uint32_t       indexCount       = 0;

    for (uint32_t i = 0; i < meshes.size(); ++i)
    {
        auto& mesh = meshes[i];
        const vk::DeviceSize vbSize = mesh.getVertexBufferSize();
        const vk::DeviceSize ibSize = mesh.getIndexBufferSize();

        std::memcpy(vbPtr + vertexByteOffset, mesh.getVertices().data(), vbSize);
        std::memcpy(ibPtr + indexByteOffset, mesh.getIndices().data(), ibSize);
        mesh.clearVertices();
        mesh.clearIndices();

        GPUMeshData data{};
        data.firstIndex   = indexCount;
        data.indexCount   = mesh.getNumIndices();
        data.vertexOffset = vertexCount;
        data.textureIndex = mesh.texture()->getBindlessIndex();
        data.transform    = mesh.getTransform();
        data.aabbMin      = mesh.getAABBMin();
        data.aabbMax      = mesh.getAABBMax();
        m_meshData.emplace_back(data);

        vk::DrawIndexedIndirectCommand drawCmd{};
        drawCmd.indexCount    = data.indexCount;
        drawCmd.instanceCount = 1;
        drawCmd.firstIndex    = data.firstIndex;
        drawCmd.vertexOffset  = data.vertexOffset;
        drawCmd.firstInstance = i;
        m_indirectCommands.emplace_back(drawCmd);

        vertexByteOffset += vbSize;
        indexByteOffset  += ibSize;
        vertexCount      += mesh.getNumVertices();
        indexCount       += mesh.getNumIndices();
    }

    stagingVBMem.unmapMemory();
    stagingIBMem.unmapMemory();

    auto cmd = beginSingleTimeCommands();

    vk::BufferCopy vertexCopy{};
    vertexCopy.size = totalVertexBytes;
    cmd->copyBuffer(*stagingVB, *m_globalVertexBuffer, { vertexCopy });

    vk::BufferCopy indexCopy{};
    indexCopy.size = totalIndexBytes;
    cmd->copyBuffer(*stagingIB, *m_globalIndexBuffer, { indexCopy });

    endSingleTimeCommands(*cmd);

    const vk::DeviceSize size = sizeof(GPUMeshData) * m_meshData.size();
    createBuffer(size,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        m_meshDataSSBO, m_meshDataSSBOMemory
    );
    m_meshDataSSBOMapped = m_meshDataSSBOMemory.mapMemory(0, size);

    updateMeshDataSSBO();
    uploadIndirectBuffer();
}

void VulkanRHI::updateMeshDataSSBO()
{
    const vk::DeviceSize size = sizeof(GPUMeshData) * m_meshData.size();
    std::memcpy(m_meshDataSSBOMapped, m_meshData.data(), size);
}

void VulkanRHI::uploadIndirectBuffer()
{
    const vk::DeviceSize size = sizeof(vk::DrawIndexedIndirectCommand) * m_indirectCommands.size();

    vk::raii::Buffer       staging({});
    vk::raii::DeviceMemory stagingMem({});
    createBuffer(size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        staging, stagingMem
    );

    void* ptr = stagingMem.mapMemory(0, size);
    std::memcpy(ptr, m_indirectCommands.data(), size);
    stagingMem.unmapMemory();

    createBuffer(size,
        vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_indirectBuffer, m_indirectBufferMemory
    );

    auto cmd = beginSingleTimeCommands();

    vk::BufferCopy copy{};
    copy.size = size;
    cmd->copyBuffer(*staging, *m_indirectBuffer, { copy });

    endSingleTimeCommands(*cmd);
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

    // Pool
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
    {
        m_meshData[i].transform = meshes[i].getTransform();
    }
    updateMeshDataSSBO();
}

FrustumPlanes VulkanRHI::extractFrustum(const glm::mat4& viewProj) const
{
    FrustumPlanes f{};
    f.objectCount = static_cast<uint32_t>(m_meshData.size());

    const auto& m = viewProj;

    // Gribb-Hartmann extraction - each plane from a row combination of viewProj
    // See https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
    f.planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]); // left
    f.planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]); // right
    f.planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]); // bottom
    f.planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]); // top
    f.planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]); // near
    f.planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]); // far

    // Normalize
    for (auto& p : f.planes)
    {
        float len = glm::length(glm::vec3(p));
        p /= len;
    }

    return f;
}

void VulkanRHI::createCullPipeline()
{
    // Descriptor set layout
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings{};

    // binding 0 - ObjectData SSBO (read)
    bindings[0].binding        = 0;
    bindings[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags     = vk::ShaderStageFlagBits::eCompute;

    // binding 1 - DrawIndexedIndirectCommand SSBO (write)
    bindings[1].binding        = 1;
    bindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags     = vk::ShaderStageFlagBits::eCompute;

    // binding 2 - draw count (atomic uint)
    bindings[2].binding        = 2;
    bindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags     = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();
    m_cullSetLayout = vk::raii::DescriptorSetLayout(m_device.rawDevice(), layoutInfo);

    // Descriptor pool
    vk::DescriptorPoolSize poolSize{};
    poolSize.type            = vk::DescriptorType::eStorageBuffer;
    poolSize.descriptorCount = 3;

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    m_cullPool = vk::raii::DescriptorPool(m_device.rawDevice(), poolInfo);

    // Descriptor set
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool     = *m_cullPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &(*m_cullSetLayout);
    m_cullSet = std::move(m_device.rawDevice().allocateDescriptorSets(allocInfo)[0]);

    // Draw count buffer
    createBuffer(sizeof(uint32_t),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        m_drawCountBuffer, m_drawCountBufferMemory
    );
    m_drawCountMapped = m_drawCountBufferMemory.mapMemory(0, sizeof(uint32_t));

    // Write descriptors
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

    // Push constant
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

    // Compute pipeline
    auto shaderCode = readShader("FrustumCulling.spv");
    vk::raii::ShaderModule cullModule = createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo stageInfo{};
    stageInfo.stage  = vk::ShaderStageFlagBits::eCompute;
    stageInfo.module = *cullModule;
    stageInfo.pName  = "cullMain";

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

    // Makes buffers visible and editable to compute shader
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

    // Dispatch cull compute
    constexpr int THREADS = 256;
    FrustumPlanes frustum = extractFrustum(m_cameraUBO.proj * m_cameraUBO.view);

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *m_cullPipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_cullPipelineLayout, 0, { *m_cullSet }, {});
    cmd.pushConstants<FrustumPlanes>(*m_cullPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, frustum);

    const uint32_t groups = (static_cast<uint32_t>(m_meshData.size()) + (THREADS - 1)) / THREADS;
    cmd.dispatch(groups, 1, 1);

    // Barrier - compute writes -> indirect draw reads
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

void VulkanRHI::updateSceneUBO(rhi::vulkan::SceneInfos* upstr, glm::vec3 cameraPos)
{
    m_clearColor            = upstr->clearColor;
    m_shininess             = upstr->shininess;
    m_sceneUBO.lightDir     = upstr->lightDirection;
    m_sceneUBO.lightColor   = upstr->lightColor;
    m_sceneUBO.ambientColor = upstr->ambientColor;
    m_sceneUBO.cameraPos    = cameraPos;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        std::memcpy(m_uniformBuffersMapped[i], &m_sceneUBO, sizeof(m_sceneUBO));
}

void VulkanRHI::recreateSwapchain()
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
    m_device.recreateSwapchain(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
}

ImGui_ImplVulkan_InitInfo VulkanRHI::getInitInfo()
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
        throw std::runtime_error("[VulkanRHI] Failed to create ImGui descriptor pool");
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
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    m_swapchainFormat = m_device.swapchainFormat();
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = reinterpret_cast<VkFormat*>(&m_swapchainFormat);
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat   = static_cast<VkFormat>(findDepthFormat());
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    return initInfo;
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
}

void VulkanRHI::drawFrame()
{
    if (m_framebufferResized)
    {
        recreateSwapchain();
    }

    if (!m_device.beginFrame()) return;

    perFrameUpdate();

    const bool offscreenReady =
        (*m_offscreenImage != VK_NULL_HANDLE) &&
        (*m_offscreenImageView != VK_NULL_HANDLE) &&
        (*m_offscreenDepthImage != VK_NULL_HANDLE) &&
        (*m_offscreenDepthImageView != VK_NULL_HANDLE);

    if (offscreenReady)
    {
        recordOffscreenCommandBuffer();
    }

    ImGui::Render();
    recordImGuiCommandBuffer();

    m_device.endFrame();
}

void VulkanRHI::updateCameraUBO(UniformBufferObject ubo)
{
    m_cameraUBO = ubo;
}

uint32_t VulkanRHI::getDrawCount()
{
    return m_drawCount;
}

std::vector<VulkanMesh>& VulkanRHI::getMeshData()
{
    return meshes;
}

void VulkanRHI::requestOffscreenResize(uint32_t width, uint32_t height)
{
    if (width <= 0 || height <= 0) return;
    if (width == m_offscreenExtent.width && height == m_offscreenExtent.height) return;

    m_offscreenResizePending  = true;
    m_pendingOffscreenWidth   = width;
    m_pendingOffscreenHeight  = height;
}

void VulkanRHI::processPendingOffscreenResize()
{
    if (m_offscreenResizePending)
    {
        m_offscreenResizePending = false;
        resizeOffscreenResources(m_pendingOffscreenWidth, m_pendingOffscreenHeight);
    }
}

void VulkanRHI::createOffscreenResources(uint32_t width, uint32_t height)
{
    m_offscreenExtent.width = width;
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

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter     = vk::Filter::eLinear;
    samplerInfo.minFilter     = vk::Filter::eLinear;
    samplerInfo.mipmapMode    = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU  = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV  = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW  = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.minLod        = 0.0f;
    samplerInfo.maxLod        = vk::LodClampNone;
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
        barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 };
        vk::DependencyInfo depInfo{};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers    = &barrier;
        cmd->pipelineBarrier2(depInfo);
        endSingleTimeCommands(*cmd);
    }

    m_imguiTextureDescriptor = ImGui_ImplVulkan_AddTexture(
        *m_offscreenSampler,
        *m_offscreenImageView,
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
    m_offscreenImageView        = nullptr;
    m_offscreenImage            = nullptr;
    m_offscreenImageMemory      = nullptr;
    m_offscreenSampler          = nullptr;
    m_offscreenDepthImageView   = nullptr;
    m_offscreenDepthImage       = nullptr;
    m_offscreenDepthImageMemory = nullptr;

    // Recreate with new size
    createOffscreenResources(width, height);
}

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

void VulkanRHI::perFrameUpdate()
{
    refreshMeshData();
}

void VulkanRHI::createGraphicsPipeline()
{
    auto shaderCode = readShader("Base_Shader.spv");
    vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo{};
    vertexShaderStageInfo.stage     = vk::ShaderStageFlagBits::eVertex;
    vertexShaderStageInfo.module    = shaderModule;
    vertexShaderStageInfo.pName     = "vertMain";

    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo{};
    fragmentShaderStageInfo.stage     = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderStageInfo.module    = shaderModule;
    fragmentShaderStageInfo.pName     = "fragMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageInfo, fragmentShaderStageInfo};

    std::vector dynamicStates =
    {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer {};
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
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable   = vk::False;
    colorBlending.logicOp         = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // Push constant, camera UBO (viewProj)
    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = static_cast<uint32_t>(sizeof(glm::mat4) + sizeof(float) * 4);
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

    std::array<vk::DescriptorSetLayout, 3> layouts
    {
        *m_meshDataDescriptorSetLayout, // set = 0 Per mesh data
        *m_bindlessLayout,              // set = 1 Bindless textures
        *m_UBOLayout                    // set = 2 Scene UBO
    };
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount         = layouts.size();
    pipelineLayoutInfo.pSetLayouts            = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    m_pipelineLayout = vk::raii::PipelineLayout(m_device.rawDevice(), pipelineLayoutInfo);

    m_swapchainFormat = m_device.swapchainFormat();
    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapchainFormat;
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
static struct PushConstant {
    glm::mat4 transform;
    float shininess;
    float _pad[3];
} pc;
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

    // Begin dynamic rendering
    const vk::ClearValue clearColor = vk::ClearColorValue(m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w);
    const vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.imageView   = m_offscreenImageView;
    colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachmentInfo.clearValue  = clearColor;

    vk::RenderingAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.imageView   = m_offscreenDepthImageView;
    depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    depthAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eDontCare;
    depthAttachmentInfo.clearValue  = clearDepth;

    vk::Rect2D renderArea{};
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent = m_offscreenExtent;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea               = renderArea;
    renderingInfo.layerCount               = 1;
    renderingInfo.colorAttachmentCount     = 1;
    renderingInfo.pColorAttachments        = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment         = &depthAttachmentInfo;

    recordCullPass();

    cmd.beginRendering(renderingInfo);

    // Draw scene
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
    pc.transform = m_cameraUBO.proj * m_cameraUBO.view;
    pc.shininess = m_shininess;
    cmd.pushConstants<PushConstant>(*m_pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pc);

    // Entire scene - one call
    cmd.drawIndexedIndirectCount(
        *m_indirectBuffer,   0,
        *m_drawCountBuffer,  0,
        static_cast<uint32_t>(m_meshData.size()),
        sizeof(vk::DrawIndexedIndirectCommand)
    );

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

void VulkanRHI::createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                            vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                            vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
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
    allocInfo.allocationSize = memoryRequirements.size;
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
    viewInfo.subresourceRange = {aspectFlags, 0, mipLevels, 0, 1 };

    return vk::raii::ImageView(m_device.rawDevice(), viewInfo);
}

void VulkanRHI::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                             vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufMem)
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
                                       vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                       vk::AccessFlags2 srcAccess, vk::AccessFlags2 dstAccess,
                                       vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage,
                                       vk::ImageAspectFlags aspect, uint32_t mipLevels)
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
    barrier.subresourceRange    = {aspect, 0, mipLevels, 0, 1 };

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
    const auto memProps = m_device.rawPhysDevice().getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
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
        const auto fmtProps = m_device.rawPhysDevice().getFormatProperties(fmt);
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

} // Namespace gcep::rhi::vulkan
