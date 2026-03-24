#include "vulkan_rhi.hpp"

#include <Log/log.hpp>
#include <cstring> // memcpy

namespace gcep::rhi::vulkan
{
    // =========================================================================
    // createPhysicsDebugPipelines
    // =========================================================================
    // Creates two pipelines that share one pipeline layout:
    //   - m_debugLinePipeline  (eLineList)
    //   - m_debugTriPipeline   (eTriangleList)
    // Both use the same shader (Debug_Physics.spv), the same push constant block,
    // and the same vertex input format (DebugVertex: vec3 position + R8G8B8A8 color).
    // The only difference is the primitive topology.

    void VulkanRHI::createPhysicsDebugPipelines()
    {
        // ── Pipeline layout ────────────────────────────────────────────────────
        // Single push constant: DebugPushConstants (viewProj mat4 = 64 bytes).
        // Only the vertex shader needs it — no descriptor sets required.
        vk::PushConstantRange pcRange{};
        pcRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
        pcRange.offset     = 0;
        pcRange.size       = sizeof(DebugPushConstants);

        vk::PipelineLayoutCreateInfo layoutCI{};
        layoutCI.pushConstantRangeCount = 1;
        layoutCI.pPushConstantRanges    = &pcRange;
        m_debugPipelineLayout = vk::raii::PipelineLayout(m_device.rawDevice(), layoutCI);

        // ── Shader ─────────────────────────────────────────────────────────────
        auto spv    = readShader("Debug_Physics.spv");
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

        // ── Vertex input ───────────────────────────────────────────────────────
        // Unlike the cone/sprite gizmos which generate geometry procedurally in
        // the vertex shader, the debug renderer uploads real vertex data:
        //   binding 0, stride 16 bytes, per-vertex
        //   location 0: position  (vec3,  R32G32B32_SFLOAT,  offset 0)
        //   location 1: color     (uint,  R8G8B8A8_UNORM,    offset 12)
        auto bindingDesc   = DebugVertex::getBindingDescription();
        auto attributeDesc = DebugVertex::getAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &bindingDesc;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size());
        vertexInput.pVertexAttributeDescriptions    = attributeDesc.data();

        // ── Fixed states (shared by both pipelines) ────────────────────────────
        vk::PipelineRasterizationStateCreateInfo raster{};
        raster.polygonMode = vk::PolygonMode::eFill;
        raster.cullMode    = vk::CullModeFlagBits::eNone; // debug geometry viewed from all angles
        raster.frontFace   = vk::FrontFace::eCounterClockwise;
        raster.lineWidth   = 1.0f;

        // Depth test ON so debug draw respects occlusion.
        // Depth write OFF so debug draw doesn't corrupt the depth buffer.
        vk::PipelineDepthStencilStateCreateInfo depth{};
        depth.depthTestEnable  = vk::True;
        depth.depthWriteEnable = vk::False;
        depth.depthCompareOp   = vk::CompareOp::eLessOrEqual;

        // Alpha blend: debug shapes can be translucent (Jolt uses alpha for contacts etc.)
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

        // Two attachments: scene color + motion vectors (motion gets zeroed in the shader)
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

        // Dynamic rendering (Vulkan 1.3) — no render pass object needed
        const std::array<vk::Format, 2> colorFormats = { m_swapchainFormat, m_motionVectorsFormat };
        vk::PipelineRenderingCreateInfo renderingCI{};
        renderingCI.colorAttachmentCount    = 2;
        renderingCI.pColorAttachmentFormats = colorFormats.data();
        renderingCI.depthAttachmentFormat   = findDepthFormat();

        // ── Pipeline A: lines ──────────────────────────────────────────────────
        vk::PipelineInputAssemblyStateCreateInfo lineAssembly{ {}, vk::PrimitiveTopology::eLineList };

        vk::GraphicsPipelineCreateInfo lineCI{};
        lineCI.pNext               = &renderingCI;
        lineCI.stageCount          = static_cast<uint32_t>(stages.size());
        lineCI.pStages             = stages.data();
        lineCI.pVertexInputState   = &vertexInput;
        lineCI.pInputAssemblyState = &lineAssembly;
        lineCI.pViewportState      = &vpState;
        lineCI.pRasterizationState = &raster;
        lineCI.pMultisampleState   = &msaa;
        lineCI.pDepthStencilState  = &depth;
        lineCI.pColorBlendState    = &blend;
        lineCI.pDynamicState       = &dynState;
        lineCI.layout              = *m_debugPipelineLayout;

        m_debugLinePipeline = vk::raii::Pipeline(m_device.rawDevice(), nullptr, lineCI);

        // ── Pipeline B: triangles ──────────────────────────────────────────────
        vk::PipelineInputAssemblyStateCreateInfo triAssembly{ {}, vk::PrimitiveTopology::eTriangleList };

        vk::GraphicsPipelineCreateInfo triCI = lineCI; // copy — same everything
        triCI.pInputAssemblyState = &triAssembly;      // except topology

        m_debugTriPipeline = vk::raii::Pipeline(m_device.rawDevice(), nullptr, triCI);
    }

    // =========================================================================
    // initPhysicsDebugBuffers
    // =========================================================================
    // Creates two host-visible, host-coherent vertex buffers per frame-in-flight
    // (one for lines, one for triangles) and maps them persistently.
    //
    // host-coherent means no explicit flush/invalidate is needed — writes are
    // visible to the GPU immediately without calling vkFlushMappedMemoryRanges.

    void VulkanRHI::initPhysicsDebugBuffers()
    {
        for (auto& fb : m_debugFrameBuffers)
        {
            // Line buffer
            createBuffer(
                kDebugLineBufferSize,
                vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                fb.lineBuffer, fb.lineMem
            );
            fb.lineMapped = fb.lineMem.mapMemory(0, kDebugLineBufferSize);

            // Triangle buffer
            createBuffer(
                kDebugTriBufferSize,
                vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                fb.triBuffer, fb.triMem
            );
            fb.triMapped = fb.triMem.mapMemory(0, kDebugTriBufferSize);
        }
    }

    // =========================================================================
    // flushPhysicsDebugData
    // =========================================================================
    // Copies raw vertex data (from PhysicsDebugRenderer) into the GPU-mapped
    // buffers for the current frame slot, then toggles the slot for next frame.
    //
    // Called by perFrameUpdate() BEFORE recording begins.
    // The data is produced by physicsSystem->DrawBodies() in the editor/game loop.
    //
    // inLineData / inTriData: pointer to arrays of DebugVertex (16 bytes each).
    // Layout is binary-compatible with PhysicsDebugRenderer::DebugVertex
    // (Vector3<float> 12 bytes + uint32_t 4 bytes).

    void VulkanRHI::flushPhysicsDebugData(const void* inLineData, uint32_t inLineVertCount,
                                          const void* inTriData,  uint32_t inTriVertCount)
    {
        auto& fb = m_debugFrameBuffers[m_debugFrameSlot];

        // Lines — clamp to buffer capacity
        const vk::DeviceSize lineBytes = static_cast<vk::DeviceSize>(inLineVertCount) * sizeof(DebugVertex);
        if (lineBytes > kDebugLineBufferSize)
        {
            Log::warn("Physics debug line buffer overflow — truncating");
            inLineVertCount = static_cast<uint32_t>(kDebugLineBufferSize / sizeof(DebugVertex));
        }
        if (inLineVertCount > 0)
            std::memcpy(fb.lineMapped, inLineData, inLineVertCount * sizeof(DebugVertex));
        m_debugLineVertCount = inLineVertCount;

        // Triangles — clamp to buffer capacity
        const vk::DeviceSize triBytes = static_cast<vk::DeviceSize>(inTriVertCount) * sizeof(DebugVertex);
        if (triBytes > kDebugTriBufferSize)
        {
            Log::warn("Physics debug triangle buffer overflow — truncating");
            inTriVertCount = static_cast<uint32_t>(kDebugTriBufferSize / sizeof(DebugVertex));
        }
        if (inTriVertCount > 0)
            std::memcpy(fb.triMapped, inTriData, inTriVertCount * sizeof(DebugVertex));
        m_debugTriVertCount = inTriVertCount;

        // Advance to the other slot for next frame
        m_debugFrameSlot ^= 1;
    }

    // =========================================================================
    // recordPhysicsDebugPass
    // =========================================================================
    // Issues at most 2 draw calls (one for lines, one for triangles).
    // Must be called inside an active dynamic rendering pass.
    // Uses the frame slot that was written by flushPhysicsDebugData() this frame,
    // which is (m_debugFrameSlot ^ 1) because the slot was already toggled.

    void VulkanRHI::recordPhysicsDebugPass()
    {
        if (m_debugLineVertCount == 0 && m_debugTriVertCount == 0) return;

        const vk::CommandBuffer cmd = m_device.currentCommandBuffer();

        // The slot that was written this frame is the one BEFORE the toggle
        const uint32_t readSlot = m_debugFrameSlot ^ 1;
        auto& fb = m_debugFrameBuffers[readSlot];

        cmd.setViewport(0, vk::Viewport(
            0.0f, 0.0f,
            static_cast<float>(m_offscreenExtent.width),
            static_cast<float>(m_offscreenExtent.height),
            0.0f, 1.0f));
        cmd.setScissor(0, vk::Rect2D({ 0, 0 }, m_offscreenExtent));

        DebugPushConstants dpc{ m_unjitteredViewProj };

        // ── Lines ────────────────────────────────────────────────────────────
        if (m_debugLineVertCount > 0)
        {
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_debugLinePipeline);
            cmd.bindVertexBuffers(0, { *fb.lineBuffer }, { vk::DeviceSize(0) });
            cmd.pushConstants<DebugPushConstants>(*m_debugPipelineLayout,
                vk::ShaderStageFlagBits::eVertex, 0, dpc);
            cmd.draw(m_debugLineVertCount, 1, 0, 0);
        }

        // ── Triangles ────────────────────────────────────────────────────────
        if (m_debugTriVertCount > 0)
        {
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_debugTriPipeline);
            cmd.bindVertexBuffers(0, { *fb.triBuffer }, { vk::DeviceSize(0) });
            cmd.pushConstants<DebugPushConstants>(*m_debugPipelineLayout,
                vk::ShaderStageFlagBits::eVertex, 0, dpc);
            cmd.draw(m_debugTriVertCount, 1, 0, 0);
        }
    }

} // namespace gcep::rhi::vulkan
