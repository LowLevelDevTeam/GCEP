#include "VulkanTexture.hpp"

#include <Engine/RHI/Vulkan/VulkanRHI.hpp>

// Externals
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// STL
#include <cmath>

namespace gcep::rhi::vulkan
{

void VulkanTexture::loadTexture(VulkanRHI* instance,
                                const std::filesystem::path& filepath,
                                bool mipmaps)
{
    pRhi         = instance;
    m_hasMipmaps = mipmaps;

    const std::string key = std::filesystem::canonical(filepath).string();
    auto& cache           = pRhi->m_textureCache;

    if (auto it = cache.find(key); it != cache.end())
    {
        m_data          = it->second.data;
        m_bindlessIndex = m_data->bindlessIndex;
        m_hasTexture    = true;
        return;
    }

    m_data = std::make_shared<TextureData>();
    createTexture(filepath);
    createTextureView();
    createTextureSampler();
    m_data->bindlessIndex = allocateTextureSlot();
    m_bindlessIndex       = m_data->bindlessIndex;
    registerBindless(m_bindlessIndex);

    cache[key] = { m_data };
    m_hasTexture = true;
}

void VulkanTexture::setLodLevel(float lodLevel)
{
    if (!m_hasMipmaps || !m_hasTexture || !m_data) return;

    pRhi->m_device.waitIdle();

    createTextureSampler(lodLevel);
    registerBindless(m_bindlessIndex);
}

void VulkanTexture::createTexture(const std::filesystem::path& filepath)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filepath.string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        throw std::runtime_error(std::string("[VulkanRHI] Failed to load texture: " + filepath.string()));
    }

    m_data->width  = static_cast<uint32_t>(texWidth);
    m_data->height = static_cast<uint32_t>(texHeight);
    vk::DeviceSize imageSize = m_data->width * m_data->height * 4;

    if(m_hasMipmaps)
    {
        m_data->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_data->width, m_data->height)))) + 1;
    }

    vk::raii::Buffer       stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    createBuffer(
        imageSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory
    );

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    createImage(
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    transitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer);

    if(m_hasMipmaps)
    {
        generateMipmaps();
    }
}

void VulkanTexture::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                             vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufMem)
{
    vk::BufferCreateInfo createInfo{};
    createInfo.size        = size;
    createInfo.usage       = usage;
    createInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = vk::raii::Buffer(pRhi->m_device.rawDevice(), createInfo);
    const auto memoryRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo(memoryRequirements.size, pRhi->findMemoryType(memoryRequirements.memoryTypeBits, properties));
    bufMem = vk::raii::DeviceMemory(pRhi->m_device.rawDevice(), allocInfo);
    buffer.bindMemory(*bufMem, 0);
}

void VulkanTexture::createTextureView()
{
    m_data->imageView = createImageView(vk::ImageAspectFlagBits::eColor);
}

void VulkanTexture::createTextureSampler(float minLod)
{
    minLod = std::clamp(minLod, 0.0f, static_cast<float>(m_data->mipLevels));
    auto properties = pRhi->m_device.rawPhysDevice().getProperties();

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter        = vk::Filter::eLinear;
    samplerInfo.minFilter        = vk::Filter::eLinear;
    samplerInfo.mipmapMode       = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.anisotropyEnable = vk::True;
    samplerInfo.maxAnisotropy    = properties.limits.maxSamplerAnisotropy;
    samplerInfo.compareEnable    = vk::False;
    samplerInfo.compareOp        = vk::CompareOp::eAlways;
    samplerInfo.minLod           = minLod;
    samplerInfo.maxLod           = vk::LodClampNone;

    m_data->sampler = vk::raii::Sampler(pRhi->m_device.rawDevice(), samplerInfo);
}

void VulkanTexture::createImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties)
{
    vk::ImageCreateInfo createInfo{};
    createInfo.imageType     = vk::ImageType::e2D;
    createInfo.format        = m_format;
    createInfo.extent.width  = m_data->width;
    createInfo.extent.height = m_data->height;
    createInfo.extent.depth  = 1;
    createInfo.mipLevels     = m_data->mipLevels;
    createInfo.arrayLayers   = 1;
    createInfo.samples       = vk::SampleCountFlagBits::e1;
    createInfo.tiling        = tiling;
    createInfo.usage         = usage;
    createInfo.sharingMode   = vk::SharingMode::eExclusive;

    m_data->image = vk::raii::Image(pRhi->m_device.rawDevice(), createInfo);
    const auto memoryRequirements = m_data->image.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = pRhi->findMemoryType(memoryRequirements.memoryTypeBits, properties);

    m_data->imageMemory = vk::raii::DeviceMemory(pRhi->m_device.rawDevice(), allocInfo);
    m_data->image.bindMemory(m_data->imageMemory, 0);
}

vk::raii::ImageView VulkanTexture::createImageView(vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image            = *m_data->image;
    viewInfo.viewType         = vk::ImageViewType::e2D;
    viewInfo.format           = m_format;
    viewInfo.subresourceRange = {aspectFlags, 0, m_data->mipLevels, 0, 1 };

    return vk::raii::ImageView(pRhi->m_device.rawDevice(), viewInfo);
}

void VulkanTexture::generateMipmaps()
{
    // Check linear filtering support
    const auto props = pRhi->m_device.rawPhysDevice().getFormatProperties(m_format);
    if (!(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
        throw std::runtime_error("[VulkanRHI] generateMipmaps: format does not support linear blitting");

    auto cmd = pRhi->beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.srcQueueFamilyIndex         = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex         = vk::QueueFamilyIgnored;
    barrier.image                       = *m_data->image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipW = m_data->width, mipH = m_data->height;
    for (uint32_t i = 1; i < m_data->mipLevels; ++i)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout     = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        cmd->pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                {}, {}, {}, barrier
        );

        vk::ImageBlit blit{};
        blit.srcSubresource = { vk::ImageAspectFlagBits::eColor, i - 1, 0, 1 };
        blit.srcOffsets[0]  = vk::Offset3D(0, 0, 0);
        blit.srcOffsets[1]  = vk::Offset3D(mipW, mipH, 1);
        blit.dstSubresource = { vk::ImageAspectFlagBits::eColor, i, 0, 1 };
        blit.dstOffsets[0]  = vk::Offset3D(0, 0, 0);
        blit.dstOffsets[1]  = vk::Offset3D(mipW > 1 ? mipW / 2 : 1, mipH > 1 ? mipH / 2 : 1, 1);
        cmd->blitImage(*m_data->image, vk::ImageLayout::eTransferSrcOptimal,
                       *m_data->image, vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);

        barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        cmd->pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                {}, {}, {}, barrier
        );

        if (mipW > 1) mipW /= 2;
        if (mipH > 1) mipH /= 2;
    }

    barrier.subresourceRange.baseMipLevel = m_data->mipLevels - 1;
    barrier.oldLayout     = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    cmd->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
            {}, {}, {}, barrier
    );

    pRhi->endSingleTimeCommands(*cmd);
}

void VulkanTexture::transitionImageLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    auto cmd = pRhi->beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout    = oldLayout;
    barrier.newLayout    = newLayout;
    barrier.image        = *m_data->image;
    barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, m_data->mipLevels, 0, 1 };

    vk::PipelineStageFlags srcStage, dstStage;
    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        srcStage = vk::PipelineStageFlagBits::eTransfer;
        dstStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else { throw std::invalid_argument("Vulkan_RHI::transitionImageLayout: unsupported transition"); }

    cmd->pipelineBarrier(srcStage, dstStage, {}, {}, {}, barrier);
    pRhi->endSingleTimeCommands(*cmd);
}

uint32_t VulkanTexture::registerBindless(uint32_t slot)
{
    m_bindlessIndex = slot;

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.sampler     = getSampler();
    imageInfo.imageView   = getImageView();
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::WriteDescriptorSet write{};
    write.dstSet          = pRhi->m_bindlessSet;
    write.dstBinding      = 0;
    write.dstArrayElement = slot;
    write.descriptorCount = 1;
    write.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo      = &imageInfo;

    pRhi->m_device.rawDevice().updateDescriptorSets({ write }, {});
    return slot;
}

void VulkanTexture::copyBufferToImage(const vk::raii::Buffer& buffer)
{
    auto cmd = pRhi->beginSingleTimeCommands();

    vk::BufferImageCopy region{};
    region.imageSubresource  = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    region.imageExtent.width  = m_data->width;
    region.imageExtent.height = m_data->height;
    region.imageExtent.depth  = 1;
    cmd->copyBufferToImage(*buffer, *m_data->image, vk::ImageLayout::eTransferDstOptimal, { region });

    pRhi->endSingleTimeCommands(*cmd);
}

}
