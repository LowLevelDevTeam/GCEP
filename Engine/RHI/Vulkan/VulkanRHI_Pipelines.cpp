#include "VulkanRHI.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <Log/Log.hpp>

namespace gcep::rhi::vulkan
{

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
    FrustumPlanes frustum = extractFrustum(m_unjitteredViewProj);

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
    glm::mat4 camViewProj;
    glm::mat4 prevCamViewProj;
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

    cmd.pushConstants<GridPushConstant>(*m_gridPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, m_gridPC);

    cmd.draw(3, 1, 0, 0);
}

} // Namespace gcep::rhi::vulkan
