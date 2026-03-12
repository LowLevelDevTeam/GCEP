#include "VulkanRHI.hpp"

// STL
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <Log/Log.hpp>

namespace gcep::rhi::vulkan
{

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
    currentInfo.imageView   = m_lightSystem.litImageView();
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
    static constexpr float halton2[16] = {
        0.500000f, 0.250000f, 0.750000f, 0.125000f,
        0.625000f, 0.375000f, 0.875000f, 0.062500f,
        0.562500f, 0.312500f, 0.812500f, 0.187500f,
        0.687500f, 0.437500f, 0.937500f, 0.031250f
    };

    static constexpr float halton3[16] = {
        0.333333f, 0.666667f, 0.111111f, 0.444444f,
        0.777778f, 0.222222f, 0.555556f, 0.888889f,
        0.037037f, 0.370370f, 0.703704f, 0.148148f,
        0.481481f, 0.814815f, 0.259259f, 0.592593f
    };

    const uint32_t idx = frameIndex % 16u;

    const float jitterX = (halton2[idx] - 0.5f) / float(width);
    const float jitterY = (halton3[idx] - 0.5f) / float(height);

    return { jitterX, jitterY };
}


} // Namespace gcep::rhi::vulkan
