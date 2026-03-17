#include "vulkan_rhi.hpp"

// Internals
#include <Log/log.hpp>

// STL
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace gcep::rhi::vulkan
{
    // Private - per-frame recording

    static struct PushConstant {
        glm::mat4 camViewProj;      // current (jittered) viewProj
        glm::mat4 prevCamViewProj;  // previous (unjittered) viewProj
    } pc;

    void VulkanRHI::perFrameUpdate()
    {
        m_lightSystem.updateLights();
        m_lastPointLights = m_lightSystem.getPointLights();
        m_lastSpotLights  = m_lightSystem.getSpotLights();
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
            m_gridPC.viewProj    = pc.camViewProj; // has jitter
            m_gridPC.invViewProj = glm::inverse(m_unjitteredViewProj); // well, read the name
            recordGridPass();
        }

        recordLightSpritePass(m_lastPointLights, m_lastSpotLights);

        cmd.endRendering();

        vk::ImageMemoryBarrier2 toShaderRead{};
        toShaderRead.srcStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        toShaderRead.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        toShaderRead.dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
        toShaderRead.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        toShaderRead.oldLayout     = vk::ImageLayout::eColorAttachmentOptimal;
        toShaderRead.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        toShaderRead.image         = *m_offscreenImage;
        toShaderRead.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 };

        vk::ImageMemoryBarrier2 depthToShaderRead{};
        depthToShaderRead.srcStageMask  = vk::PipelineStageFlagBits2::eLateFragmentTests;
        depthToShaderRead.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
        depthToShaderRead.dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
        depthToShaderRead.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        depthToShaderRead.oldLayout     = vk::ImageLayout::eDepthAttachmentOptimal;
        depthToShaderRead.newLayout     = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        depthToShaderRead.image         = *m_offscreenDepthImage;
        depthToShaderRead.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 };

        std::array<vk::ImageMemoryBarrier2, 2> postRenderBarriers = { toShaderRead, depthToShaderRead };
        vk::DependencyInfo dep2{};
        dep2.imageMemoryBarrierCount = static_cast<uint32_t>(postRenderBarriers.size());
        dep2.pImageMemoryBarriers    = postRenderBarriers.data();
        cmd.pipelineBarrier2(dep2);

        const glm::mat4 invViewProj = glm::inverse(m_unjitteredViewProj);

        m_lightSystem.recordLightingPass(
            cmd,
            invViewProj,
            glm::vec3(m_sceneUBO.cameraPos),
            m_offscreenExtent.width,
            m_offscreenExtent.height
        );

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
} // namespace gcep::rhi::vulkan
