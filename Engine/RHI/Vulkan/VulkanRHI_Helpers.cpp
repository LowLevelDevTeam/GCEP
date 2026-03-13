#include "VulkanRHI.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <Log/Log.hpp>

namespace gcep::rhi::vulkan
{

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
    auto err = std::runtime_error("[VulkanRHI] findMemoryType: no suitable type found");
    Log::handleException(err);
    throw err;
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
    auto err = std::runtime_error("[VulkanRHI] findSupportedFormat: no suitable format found");
    Log::handleException(err);
    throw err;
}

std::vector<char> VulkanRHI::readShader(const std::string& fileName)
{
    const auto path = std::filesystem::current_path() / "Assets" / "Shaders" / fileName;
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        auto err = std::runtime_error("[VulkanRHI] Cannot open shader: " + path.string());
        Log::handleException(err);
        throw err;
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
