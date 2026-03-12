#include "VulkanRHI.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <Log/Log.hpp>

namespace gcep::rhi::vulkan
{

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
        vk::ImageUsageFlagBits::eDepthStencilAttachment |
        vk::ImageUsageFlagBits::eSampled,
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
    m_lightSystem.onResize(
        m_offscreenExtent.width,
        m_offscreenExtent.height,
        *m_offscreenImageView,
        *m_offscreenDepthImageView
    );
    createTAAResources(width, height);
    // Re-register the resolved image so ImGui displays the resized output.
    m_imguiTextureDescriptor = ImGui_ImplVulkan_AddTexture(
        *m_taaSampler,
        *m_taaResolvedImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}


} // Namespace gcep::rhi::vulkan
