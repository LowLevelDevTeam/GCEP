#include "VulkanRHI.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace gcep::rhi::vulkan
{

// ============================================================================
// Private - light gizmo pipelines
// ============================================================================
//
// Two separate pipelines, both driven from recordLightSpritePass():
//
//  m_lightSpritePipeline  - Gizmo_LightSprite.spv
//      Triangle-list, additive blend (src=One, dst=One).
//      One billboard quad per point light and per spot light.
//
//  m_lightConePipeline    - Gizmo_SpotCone.spv
//      Line-list, alpha blend (src=SrcAlpha, dst=OneMinusSrcAlpha).
//      Wireframe cone for each spot light.
//      When SpotLight::showGizmo == false, visible=0 and the shader collapses
//      every vertex outside the clip cube so the rasterizer draws nothing.

// 4 edge lines + 32 outer-circle segs + 32 inner-circle segs + 1 axis line
static constexpr uint32_t kConeVertCount = 4*2 + 32*2 + 32*2 + 2; // = 138

// createLightSpritePipeline - additive billboard circles

void VulkanRHI::createLightSpritePipeline()
{
    vk::PushConstantRange pcRange{};
    pcRange.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(SpritePushConstants);

    vk::PipelineLayoutCreateInfo layoutCI{};
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges    = &pcRange;
    m_lightSpritePipelineLayout = vk::raii::PipelineLayout(m_device.rawDevice(), layoutCI);

    auto spv    = readShader("Gizmo_LightSprite.spv");
    auto module = createShaderModule(spv);

    vk::PipelineShaderStageCreateInfo vertStage{};
    vertStage.stage  = vk::ShaderStageFlagBits::eVertex;
    vertStage.module = *module;
    vertStage.pName  = "vertMain";

    vk::PipelineShaderStageCreateInfo fragStage{};
    fragStage.stage  = vk::ShaderStageFlagBits::eFragment;
    fragStage.module = *module;
    fragStage.pName  = "fragMain";

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = { vertStage, fragStage };

    vk::PipelineVertexInputStateCreateInfo   vertexInput{};
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ {}, vk::PrimitiveTopology::eTriangleList };

    vk::PipelineRasterizationStateCreateInfo raster{};
    raster.polygonMode = vk::PolygonMode::eFill;
    raster.cullMode    = vk::CullModeFlagBits::eNone;
    raster.frontFace   = vk::FrontFace::eCounterClockwise;
    raster.lineWidth   = 1.0f;

    vk::PipelineDepthStencilStateCreateInfo depth{};
    depth.depthTestEnable  = vk::True;
    depth.depthWriteEnable = vk::False;
    depth.depthCompareOp   = vk::CompareOp::eLess;

    vk::PipelineColorBlendAttachmentState blendAtt{};
    blendAtt.blendEnable         = vk::True;
    blendAtt.srcColorBlendFactor = vk::BlendFactor::eOne;
    blendAtt.dstColorBlendFactor = vk::BlendFactor::eOne;
    blendAtt.colorBlendOp        = vk::BlendOp::eAdd;
    blendAtt.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    blendAtt.dstAlphaBlendFactor = vk::BlendFactor::eOne;
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

    const std::array<vk::Format, 2> colorFormats = { m_swapchainFormat, m_motionVectorsFormat };
    vk::PipelineRenderingCreateInfo renderingCI{};
    renderingCI.colorAttachmentCount    = 2;
    renderingCI.pColorAttachmentFormats = colorFormats.data();
    renderingCI.depthAttachmentFormat   = findDepthFormat();

    vk::GraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.pNext               = &renderingCI;
    pipelineCI.stageCount          = static_cast<uint32_t>(stages.size());
    pipelineCI.pStages             = stages.data();
    pipelineCI.pVertexInputState   = &vertexInput;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState      = &vpState;
    pipelineCI.pRasterizationState = &raster;
    pipelineCI.pMultisampleState   = &msaa;
    pipelineCI.pDepthStencilState  = &depth;
    pipelineCI.pColorBlendState    = &blend;
    pipelineCI.pDynamicState       = &dynState;
    pipelineCI.layout              = *m_lightSpritePipelineLayout;

    m_lightSpritePipeline = vk::raii::Pipeline(m_device.rawDevice(), nullptr, pipelineCI);
}

void VulkanRHI::createLightConePipeline()
{
    vk::PushConstantRange pcRange{};
    pcRange.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(ConePushConstants);

    vk::PipelineLayoutCreateInfo layoutCI{};
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges    = &pcRange;
    m_lightConePipelineLayout = vk::raii::PipelineLayout(m_device.rawDevice(), layoutCI);

    auto spv    = readShader("Gizmo_SpotCone.spv");
    auto module = createShaderModule(spv);

    vk::PipelineShaderStageCreateInfo vertStage{};
    vertStage.stage  = vk::ShaderStageFlagBits::eVertex;
    vertStage.module = *module;
    vertStage.pName  = "vertMain";

    vk::PipelineShaderStageCreateInfo fragStage{};
    fragStage.stage  = vk::ShaderStageFlagBits::eFragment;
    fragStage.module = *module;
    fragStage.pName  = "fragMain";

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = { vertStage, fragStage };

    vk::PipelineVertexInputStateCreateInfo   vertexInput{};
    // Line-list: each consecutive pair of verts = one line segment
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ {}, vk::PrimitiveTopology::eLineList };

    vk::PipelineRasterizationStateCreateInfo raster{};
    raster.polygonMode = vk::PolygonMode::eFill;
    raster.cullMode    = vk::CullModeFlagBits::eNone;
    raster.frontFace   = vk::FrontFace::eCounterClockwise;
    raster.lineWidth   = 1.0f;

    vk::PipelineDepthStencilStateCreateInfo depth{};
    depth.depthTestEnable  = vk::True;
    depth.depthWriteEnable = vk::False;
    depth.depthCompareOp   = vk::CompareOp::eLessOrEqual;

    vk::PipelineColorBlendAttachmentState blendAtt{};
    blendAtt.blendEnable         = vk::True;
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

    const std::array<vk::Format, 2> colorFormats = { m_swapchainFormat, m_motionVectorsFormat };
    vk::PipelineRenderingCreateInfo renderingCI{};
    renderingCI.colorAttachmentCount    = 2;
    renderingCI.pColorAttachmentFormats = colorFormats.data();
    renderingCI.depthAttachmentFormat   = findDepthFormat();

    vk::GraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.pNext               = &renderingCI;
    pipelineCI.stageCount          = static_cast<uint32_t>(stages.size());
    pipelineCI.pStages             = stages.data();
    pipelineCI.pVertexInputState   = &vertexInput;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState      = &vpState;
    pipelineCI.pRasterizationState = &raster;
    pipelineCI.pMultisampleState   = &msaa;
    pipelineCI.pDepthStencilState  = &depth;
    pipelineCI.pColorBlendState    = &blend;
    pipelineCI.pDynamicState       = &dynState;
    pipelineCI.layout              = *m_lightConePipelineLayout;

    m_lightConePipeline = vk::raii::Pipeline(m_device.rawDevice(), nullptr, pipelineCI);
}

// recordLightSpritePass

void VulkanRHI::recordLightSpritePass(const std::vector<PointLight>& points,
                                       const std::vector<SpotLight>&  spots)
{
    if (points.empty() && spots.empty()) return;

    const vk::CommandBuffer cmd = m_device.currentCommandBuffer();

    cmd.setViewport(0, vk::Viewport(
        0.0f, 0.0f,
        static_cast<float>(m_offscreenExtent.width),
        static_cast<float>(m_offscreenExtent.height),
        0.0f, 1.0f));
    cmd.setScissor(0, vk::Rect2D({ 0, 0 }, m_offscreenExtent));

    const glm::mat4 view     = m_cameraUBO.view;
    const glm::mat4 viewProj = m_unjitteredViewProj;  // no TAA jitter on gizmos
    // Camera basis from view matrix rows (transposed columns of view inverse)
    const glm::vec3 camRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
    const glm::vec3 camUp    = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));

    // Sprite billboard (point lights + spot light centres)
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_lightSpritePipeline);
    {
        SpritePushConstants spc{};
        spc.viewProj    = viewProj;
        spc.cameraRight = camRight;
        spc.cameraUp    = camUp;

        for (const auto& pl : points)
        {
            glm::vec3 pos = {pl.position.x, pl.position.y, pl.position.z};
            spc.worldPos = pos;
            spc.halfSize = 0.25f;
            glm::vec3 col = {pl.color.x, pl.color.y, pl.color.z};
            spc.color    = col * pl.intensity * 0.15f;
            cmd.pushConstants<SpritePushConstants>(
                *m_lightSpritePipelineLayout,
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                0, spc);
            cmd.draw(6, 1, 0, 0);
        }

        for (const auto& sl : spots)
        {
            glm::vec3 pos = {sl.position.x, sl.position.y, sl.position.z};
            spc.worldPos = pos;
            spc.halfSize = 0.30f;
            glm::vec3 col = {sl.color.x, sl.color.y, sl.color.z};
            spc.color    = col * sl.intensity * 0.12f;
            cmd.pushConstants<SpritePushConstants>(
                *m_lightSpritePipelineLayout,
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                0, spc);
            cmd.draw(6, 1, 0, 0);
        }
    }

    // Cone wireframe
    if (!spots.empty())
    {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_lightConePipeline);

        for (const auto& sl : spots)
        {
            ConePushConstants cpc{};
            cpc.viewProj  = viewProj;
            glm::vec3 pos = {sl.position.x, sl.position.y, sl.position.z};
            cpc.apex      = pos;
            cpc.length    = sl.radius * 0.85f;
            glm::vec3 dir = {sl.direction.x, sl.direction.y, sl.direction.z};
            cpc.direction = glm::length(dir) > 1e-6f
                                ? glm::normalize(dir)
                                : glm::vec3(0.0f, -1.0f, 0.0f);
            cpc.outerCos  = std::cos(glm::radians(sl.outerCutoffDeg));
            cpc.innerCos  = std::cos(glm::radians(sl.innerCutoffDeg));
            glm::vec3 col = {sl.color.x, sl.color.y, sl.color.z};
            cpc.color     = col;
            cpc.alpha     = 0.75f;
            cpc.visible   = sl.showGizmo ? 1u : 0u;

            cmd.pushConstants<ConePushConstants>(
                *m_lightConePipelineLayout,
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                0, cpc);
            cmd.draw(kConeVertCount, 1, 0, 0);
        }
    }
}

} // Namespace gcep::rhi::vulkan
