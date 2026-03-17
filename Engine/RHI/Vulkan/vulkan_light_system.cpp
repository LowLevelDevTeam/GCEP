/// @file VulkanLightSystem.cpp
/// @brief Implementation of @c gcep::rhi::vulkan::LightSystem.
///
/// All Vulkan resources follow the same conventions as the rest of @c VulkanRHI:
///   - @c vulkan_raii.hpp RAII handles - destruction is automatic and in
///     reverse declaration order.
///   - @c pipelineBarrier2 / @c VkImageMemoryBarrier2 (Vulkan 1.3 / KHR_sync2).
///   - Dynamic rendering (no render passes or framebuffers).
///   - Persistently-mapped host-visible coherent buffers for CPU->GPU uploads.

#include "vulkan_light_system.hpp"

// Internals
#include <Log/log.hpp>

// STL
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace gcep::rhi::vulkan
{
    // ============================================================================
    // Public - init
    // ============================================================================

    void LightSystem::init(VulkanDevice&         device,
                           uint32_t              width,
                           uint32_t              height,
                           vk::ImageView         colorView,
                           vk::ImageView         depthView,
                           vk::raii::CommandPool& uploadPool)
    {
        m_device      = &device;
        m_uploadPool  = &uploadPool;
        m_colorFormat = vk::Format::eR16G16B16A16Sfloat;  // must support eStorage; swapchain format does not

        createLightBuffers();
        createLitOutputImage(width, height);
        createDescriptors(colorView, depthView);
        createLightingPipeline();

        m_initialised = true;
        Log::info("[LightSystem] Initialized successfully");
    }

    // ============================================================================
    // Public - onResize
    // ============================================================================

    void LightSystem::onResize(uint32_t      width,
                               uint32_t      height,
                               vk::ImageView colorView,
                               vk::ImageView depthView)
    {
        if (!m_initialised) return;
        m_device->waitIdle();

        destroyLitOutputImage();
        createLitOutputImage(width, height);
        updateDescriptors(colorView, depthView);
    }

    // ============================================================================
    // Public - updateLights
    // ============================================================================

    void LightSystem::updateLights()
    {
        assert(m_lightSSBOMapped  && "LightSystem not initialised");
        assert(m_lightMetaMapped  && "LightSystem not initialised");
        if (!m_initialised) return;

        // ---- Pack CPU descriptors into GPULight entries -------------------------

        std::vector<GPULight> gpuLights;
        gpuLights.reserve(m_pointLights.size() + m_spotLights.size());

        for (const auto& pl : m_pointLights)
        {
            if (gpuLights.size() >= MAX_LIGHTS) break;

            GPULight g{};
            g.position  = { pl.position.x, pl.position.y, pl.position.z };
            g.type      = static_cast<uint32_t>(LightType::Point);
            g.color     = { pl.color.x, pl.color.y, pl.color.z };
            g.intensity = pl.intensity;
            g.direction = glm::vec3(0.0f);  // unused
            g.radius    = pl.radius;
            g.innerCos  = 0.0f;             // unused
            g.outerCos  = 0.0f;             // unused
            gpuLights.push_back(g);
        }

        for (const auto& sl : m_spotLights)
        {
            if (gpuLights.size() >= MAX_LIGHTS) break;

            GPULight g{};
            g.position  = { sl.position.x, sl.position.y, sl.position.z };
            g.type      = static_cast<uint32_t>(LightType::Spot);
            g.color     = { sl.color.x, sl.color.y, sl.color.z };
            g.intensity = sl.intensity;
            // Normalise direction defensively; avoid NaN if caller passes zero-vector
            g.direction = { sl.direction.x, sl.direction.y, sl.direction.z };
            g.direction = glm::length(g.direction) > 1e-6f
                            ? glm::normalize(g.direction)
                            : glm::vec3(0.0f, -1.0f, 0.0f);
            g.radius    = sl.radius;
            g.innerCos  = std::cos(glm::radians(sl.innerCutoffDeg));
            g.outerCos  = std::cos(glm::radians(sl.outerCutoffDeg));
            gpuLights.push_back(g);
        }

        m_activeLightCount = static_cast<uint32_t>(gpuLights.size());

        // ---- Upload SSBO --------------------------------------------------------
        // Zero unused slots so stale data never leaks through.
        const vk::DeviceSize ssboSize = sizeof(GPULight) * MAX_LIGHTS;
        std::memset(m_lightSSBOMapped, 0, ssboSize);

        if (!gpuLights.empty())
        {
            std::memcpy(m_lightSSBOMapped,
                        gpuLights.data(),
                        sizeof(GPULight) * gpuLights.size());
        }

        // ---- Upload meta UBO ----------------------------------------------------
        LightMetaUBO meta{};
        meta.lightCount = m_activeLightCount;
        std::memcpy(m_lightMetaMapped, &meta, sizeof(LightMetaUBO));
    }

    void LightSystem::addPointLight(Vector3<float> pos)
    {
        m_pointLights.push_back({
            .position  = pos,
            .color     = { 1.0f, 1.0f, 1.0f },
            .intensity = 5.0f,
            .radius    = 2.0f
        });
    }

    void LightSystem::addSpotLight(Vector3<float> pos)
    {
        m_spotLights.push_back({
            .position       = pos,
            .direction      = { 0.0f, -1.0f, 0.0f },
            .color          = { 1.0f,  1.0f, 1.0f },
            .intensity      = 1.0f,
            .radius         = 20.0f,
            .innerCutoffDeg = 15.0f,
            .outerCutoffDeg = 30.0f,
            .showGizmo      = true
        });
    }

    void LightSystem::removePointLight(ECS::EntityID id)
    {
        auto it = std::ranges::find(m_pointLights, id, &PointLight::id);
        if (it != m_pointLights.end())
            m_pointLights.erase(it);
    }

    void LightSystem::removeSpotLight(ECS::EntityID id)
    {
        auto it = std::ranges::find(m_spotLights, id, &SpotLight::id);
        if (it != m_spotLights.end())
            m_spotLights.erase(it);
    }

    // ============================================================================
    // Public - recordLightingPass
    // ============================================================================

    void LightSystem::recordLightingPass(vk::CommandBuffer cmd,
                                          const glm::mat4&  invViewProj,
                                          const glm::vec3&  cameraPos,
                                          uint32_t          width,
                                          uint32_t          height)
    {
        if (!m_initialised) return;
        // -------------------------------------------------------------------------
        // Pre-dispatch barriers
        //
        // - offscreen color: eShaderReadOnlyOptimal  (already there after scene pass,
        //   but we declare the dependency explicitly for correctness)
        // - lit output:       eGeneral (storage write)
        // The SSBO/UBO are host-coherent; no barrier needed for them.
        // -------------------------------------------------------------------------

        vk::ImageMemoryBarrier2 litBarrier{};
        litBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
        litBarrier.srcAccessMask = vk::AccessFlagBits2::eShaderRead;
        litBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
        litBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
        litBarrier.oldLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        litBarrier.newLayout     = vk::ImageLayout::eGeneral;
        litBarrier.image         = *m_litImage;
        litBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        vk::DependencyInfo preBarrierInfo{};
        preBarrierInfo.imageMemoryBarrierCount = 1;
        preBarrierInfo.pImageMemoryBarriers    = &litBarrier;
        cmd.pipelineBarrier2(preBarrierInfo);

        // -------------------------------------------------------------------------
        // Host-write -> compute-read barrier on the light SSBO and meta UBO.
        // Both are host-visible coherent, so we only need a pipeline-stage
        // ordering barrier (no cache flush required for eHostCoherent memory).
        // -------------------------------------------------------------------------

        std::array<vk::BufferMemoryBarrier2, 2> bufBarriers{};

        bufBarriers[0].srcStageMask  = vk::PipelineStageFlagBits2::eHost;
        bufBarriers[0].srcAccessMask = vk::AccessFlagBits2::eHostWrite;
        bufBarriers[0].dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
        bufBarriers[0].dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        bufBarriers[0].buffer        = *m_lightSSBO;
        bufBarriers[0].size          = vk::WholeSize;

        bufBarriers[1].srcStageMask  = vk::PipelineStageFlagBits2::eHost;
        bufBarriers[1].srcAccessMask = vk::AccessFlagBits2::eHostWrite;
        bufBarriers[1].dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
        bufBarriers[1].dstAccessMask = vk::AccessFlagBits2::eUniformRead;
        bufBarriers[1].buffer        = *m_lightMetaUBO;
        bufBarriers[1].size          = vk::WholeSize;

        vk::DependencyInfo bufDep{};
        bufDep.bufferMemoryBarrierCount = static_cast<uint32_t>(bufBarriers.size());
        bufDep.pBufferMemoryBarriers    = bufBarriers.data();
        cmd.pipelineBarrier2(bufDep);

        // -------------------------------------------------------------------------
        // Dispatch
        // -------------------------------------------------------------------------

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *m_pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                               *m_pipelineLayout, 0, { *m_set }, {});

        LightingPushConstants lpc{};
        lpc.invViewProj = invViewProj;
        lpc.cameraPos   = cameraPos;
        lpc._pad0       = 0.0f;
        lpc.screenSize  = { static_cast<float>(width), static_cast<float>(height) };

        cmd.pushConstants<LightingPushConstants>(
            *m_pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            0,
            lpc
        );

        const uint32_t groupX = (width  + 7u) / 8u;
        const uint32_t groupY = (height + 7u) / 8u;
        cmd.dispatch(groupX, groupY, 1);

        // Post-dispatch barrier: lit output -> eShaderReadOnlyOptimal for TAA.

        vk::ImageMemoryBarrier2 litToRead{};
        litToRead.srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader;
        litToRead.srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
        litToRead.dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader
                                | vk::PipelineStageFlagBits2::eFragmentShader;
        litToRead.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        litToRead.oldLayout     = vk::ImageLayout::eGeneral;
        litToRead.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        litToRead.image         = *m_litImage;
        litToRead.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        vk::DependencyInfo postBarrierInfo{};
        postBarrierInfo.imageMemoryBarrierCount = 1;
        postBarrierInfo.pImageMemoryBarriers    = &litToRead;
        cmd.pipelineBarrier2(postBarrierInfo);
    }

    PointLight* LightSystem::findPointLight(ECS::EntityID id)
    {
        for(auto& point : m_pointLights)
        {
            if(id == point.id)
            {
                return &point;
            }
        }
        return nullptr;
    }

    SpotLight* LightSystem::findSpotLight(ECS::EntityID id)
    {
        for(auto& spot : m_spotLights)
        {
            if(id == spot.id)
            {
                return &spot;
            }
        }
        return nullptr;
    }

    // Private - createBuffer

    void LightSystem::createBuffer(vk::DeviceSize          size,
                                    vk::BufferUsageFlags    usage,
                                    vk::MemoryPropertyFlags props,
                                    vk::raii::Buffer&       outBuffer,
                                    vk::raii::DeviceMemory& outMemory)
    {
        assert(m_device);

        vk::BufferCreateInfo ci{};
        ci.size        = size;
        ci.usage       = usage;
        ci.sharingMode = vk::SharingMode::eExclusive;

        outBuffer = vk::raii::Buffer(m_device->rawDevice(), ci);
        const auto req = outBuffer.getMemoryRequirements();

        vk::MemoryAllocateInfo ai{};
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);

        outMemory = vk::raii::DeviceMemory(m_device->rawDevice(), ai);
        outBuffer.bindMemory(*outMemory, 0);
    }

    // Private - createImage

    void LightSystem::createImage(uint32_t                width,
                                   uint32_t                height,
                                   vk::Format              format,
                                   vk::ImageUsageFlags     usage,
                                   vk::raii::Image&        outImage,
                                   vk::raii::DeviceMemory& outMemory)
    {
        assert(m_device);

        vk::ImageCreateInfo ci{};
        ci.imageType     = vk::ImageType::e2D;
        ci.format        = format;
        ci.extent        = vk::Extent3D{ width, height, 1u };
        ci.mipLevels     = 1u;
        ci.arrayLayers   = 1u;
        ci.samples       = vk::SampleCountFlagBits::e1;
        ci.tiling        = vk::ImageTiling::eOptimal;
        ci.usage         = usage;
        ci.sharingMode   = vk::SharingMode::eExclusive;
        ci.initialLayout = vk::ImageLayout::eUndefined;

        outImage = vk::raii::Image(m_device->rawDevice(), ci);
        const auto req = outImage.getMemoryRequirements();

        vk::MemoryAllocateInfo ai{};
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits,
                                            vk::MemoryPropertyFlagBits::eDeviceLocal);

        outMemory = vk::raii::DeviceMemory(m_device->rawDevice(), ai);
        outImage.bindMemory(*outMemory, 0);
    }

    // Private - createImageView

    vk::raii::ImageView LightSystem::createImageView(vk::raii::Image&     image,
                                                       vk::Format           format,
                                                       vk::ImageAspectFlags aspect)
    {
        vk::ImageViewCreateInfo ci{};
        ci.image            = *image;
        ci.viewType         = vk::ImageViewType::e2D;
        ci.format           = format;
        ci.subresourceRange = { aspect, 0, 1, 0, 1 };

        return vk::raii::ImageView(m_device->rawDevice(), ci);
    }

    // Private - findMemoryType

    uint32_t LightSystem::findMemoryType(uint32_t                typeFilter,
                                          vk::MemoryPropertyFlags props) const
    {
        const auto memProps = m_device->constRawPhysDevice().getMemoryProperties();
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        }

        auto err = std::runtime_error("[LightSystem] findMemoryType: no suitable type");
        Log::handleException(err);
        throw err;
    }

    // Private - single-time command helpers

    std::unique_ptr<vk::raii::CommandBuffer> LightSystem::beginSingleTimeCommands()
    {
        vk::CommandBufferAllocateInfo ai{};
        ai.commandPool        = **m_uploadPool;
        ai.level              = vk::CommandBufferLevel::ePrimary;
        ai.commandBufferCount = 1;

        auto cmd = std::make_unique<vk::raii::CommandBuffer>(
            std::move(vk::raii::CommandBuffers(m_device->rawDevice(), ai).front())
        );

        vk::CommandBufferBeginInfo bi{};
        bi.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd->begin(bi);
        return cmd;
    }

    void LightSystem::endSingleTimeCommands(vk::raii::CommandBuffer& cmd)
    {
        cmd.end();
        vk::SubmitInfo si{};
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &*cmd;
        m_device->rawQueue().submit(si, nullptr);
        m_device->rawQueue().waitIdle();
    }

    // Private - createLightBuffers

    void LightSystem::createLightBuffers()
    {
        // SSBO - host-visible, host-coherent so we can memcpy every frame without
        // staging.  Size is fixed at MAX_LIGHTS slots; unused slots remain zeroed.
        const vk::DeviceSize ssboSize = sizeof(GPULight) * MAX_LIGHTS;
        createBuffer(
            ssboSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent,
            m_lightSSBO, m_lightSSBOMemory
        );
        m_lightSSBOMapped = m_lightSSBOMemory.mapMemory(0, ssboSize);
        std::memset(m_lightSSBOMapped, 0, ssboSize);

        // Meta UBO - single uint + padding
        createBuffer(
            sizeof(LightMetaUBO),
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent,
            m_lightMetaUBO, m_lightMetaUBOMemory
        );
        m_lightMetaMapped = m_lightMetaUBOMemory.mapMemory(0, sizeof(LightMetaUBO));
        LightMetaUBO zero{};
        std::memcpy(m_lightMetaMapped, &zero, sizeof(LightMetaUBO));
    }

    // Private - createLitOutputImage / destroyLitOutputImage

    void LightSystem::createLitOutputImage(uint32_t width, uint32_t height)
    {
        createImage(
            width, height, m_colorFormat,
            vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eTransferSrc,
            m_litImage, m_litImageMemory
        );

        m_litImageView = createImageView(m_litImage,
                                         m_colorFormat,
                                         vk::ImageAspectFlagBits::eColor);

        // Transition to eShaderReadOnlyOptimal once so the first TAA read finds it
        // in a well-defined layout before any lighting compute has run.
        {
            auto cmd = beginSingleTimeCommands();

            vk::ImageMemoryBarrier2 b{};
            b.srcStageMask     = vk::PipelineStageFlagBits2::eTopOfPipe;
            b.srcAccessMask    = vk::AccessFlagBits2::eNone;
            b.dstStageMask     = vk::PipelineStageFlagBits2::eComputeShader |
                                 vk::PipelineStageFlagBits2::eFragmentShader;
            b.dstAccessMask    = vk::AccessFlagBits2::eShaderRead;
            b.oldLayout        = vk::ImageLayout::eUndefined;
            b.newLayout        = vk::ImageLayout::eShaderReadOnlyOptimal;
            b.image            = *m_litImage;
            b.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

            vk::DependencyInfo dep{};
            dep.imageMemoryBarrierCount = 1;
            dep.pImageMemoryBarriers    = &b;
            cmd->pipelineBarrier2(dep);

            endSingleTimeCommands(*cmd);
        }

        // Nearest-clamp sampler; the lit image is read at exact texel coordinates
        // in the TAA and ImGui stages.
        vk::SamplerCreateInfo si{};
        si.magFilter    = vk::Filter::eNearest;
        si.minFilter    = vk::Filter::eNearest;
        si.mipmapMode   = vk::SamplerMipmapMode::eNearest;
        si.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        si.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        si.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        si.maxAnisotropy = 1.0f;
        si.minLod        = 0.0f;
        si.maxLod        = vk::LodClampNone;

        m_litSampler = vk::raii::Sampler(m_device->rawDevice(), si);
    }

    void LightSystem::destroyLitOutputImage()
    {
        m_litSampler    = nullptr;
        m_litImageView  = nullptr;
        m_litImage      = nullptr;
        m_litImageMemory = nullptr;
    }

    // Private - createDescriptors

    void LightSystem::createDescriptors(vk::ImageView colorView, vk::ImageView depthView)
    {
        vk::SamplerCreateInfo si{};
        si.magFilter    = vk::Filter::eNearest;
        si.minFilter    = vk::Filter::eNearest;
        si.mipmapMode   = vk::SamplerMipmapMode::eNearest;
        si.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        si.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        si.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        si.maxAnisotropy = 1.0f;
        si.minLod        = 0.0f;
        si.maxLod        = vk::LodClampNone;
        m_inputSampler = vk::raii::Sampler(m_device->rawDevice(), si);

        // Layout
        std::array<vk::DescriptorSetLayoutBinding, 5> bindings{};

        // binding 0 - GPULight SSBO
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = vk::DescriptorType::eStorageBuffer;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = vk::ShaderStageFlagBits::eCompute;

        // binding 1 - LightMetaUBO
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = vk::DescriptorType::eUniformBuffer;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = vk::ShaderStageFlagBits::eCompute;

        // binding 2 - scene color (sampled)
        bindings[2].binding         = 2;
        bindings[2].descriptorType  = vk::DescriptorType::eCombinedImageSampler;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = vk::ShaderStageFlagBits::eCompute;

        // binding 3 - scene depth (sampled)
        bindings[3].binding         = 3;
        bindings[3].descriptorType  = vk::DescriptorType::eCombinedImageSampler;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags      = vk::ShaderStageFlagBits::eCompute;

        // binding 4 - lit output (storage image, write)
        bindings[4].binding         = 4;
        bindings[4].descriptorType  = vk::DescriptorType::eStorageImage;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags      = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutCreateInfo layoutCI{};
        layoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutCI.pBindings    = bindings.data();
        m_setLayout = vk::raii::DescriptorSetLayout(m_device->rawDevice(), layoutCI);

        // Pool
        std::array<vk::DescriptorPoolSize, 4> poolSizes{};
        poolSizes[0] = { vk::DescriptorType::eStorageBuffer,        1u };
        poolSizes[1] = { vk::DescriptorType::eUniformBuffer,        1u };
        poolSizes[2] = { vk::DescriptorType::eCombinedImageSampler, 2u };
        poolSizes[3] = { vk::DescriptorType::eStorageImage,         1u };

        vk::DescriptorPoolCreateInfo poolCI{};
        poolCI.maxSets       = 1u;
        poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolCI.pPoolSizes    = poolSizes.data();
        poolCI.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        m_pool = vk::raii::DescriptorPool(m_device->rawDevice(), poolCI);

        // Set
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool     = *m_pool;
        allocInfo.descriptorSetCount = 1u;
        allocInfo.pSetLayouts        = &*m_setLayout;
        m_set = std::move(vk::raii::DescriptorSets(m_device->rawDevice(), allocInfo).front());

        // Initial write
        updateDescriptors(colorView, depthView);
    }

    // Private - updateDescriptors

    void LightSystem::updateDescriptors(vk::ImageView colorView, vk::ImageView depthView)
    {
        // Binding 0 - light SSBO
        vk::DescriptorBufferInfo ssboInfo{};
        ssboInfo.buffer = *m_lightSSBO;
        ssboInfo.offset = 0;
        ssboInfo.range  = vk::WholeSize;

        // Binding 1 - meta UBO
        vk::DescriptorBufferInfo uboInfo{};
        uboInfo.buffer = *m_lightMetaUBO;
        uboInfo.offset = 0;
        uboInfo.range  = sizeof(LightMetaUBO);

        // Binding 2 - scene color sampled
        vk::DescriptorImageInfo colorInfo{};
        colorInfo.sampler     = *m_inputSampler;
        colorInfo.imageView   = colorView;
        colorInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        // Binding 3 - scene depth sampled.
        // The image is transitioned to eDepthStencilReadOnlyOptimal before the
        // lighting dispatch, so the layout here must match that.
        vk::DescriptorImageInfo depthInfo{};
        depthInfo.sampler     = *m_inputSampler;
        depthInfo.imageView   = depthView;
        depthInfo.imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;

        // Binding 4 - lit output (storage, written by compute)
        vk::DescriptorImageInfo litInfo{};
        litInfo.imageView   = *m_litImageView;
        litInfo.imageLayout = vk::ImageLayout::eGeneral;

        std::array<vk::WriteDescriptorSet, 5> writes{};
        writes[0] = { *m_set, 0, 0, 1, vk::DescriptorType::eStorageBuffer,        nullptr,    &ssboInfo  };
        writes[1] = { *m_set, 1, 0, 1, vk::DescriptorType::eUniformBuffer,        nullptr,    &uboInfo   };
        writes[2] = { *m_set, 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &colorInfo, nullptr    };
        writes[3] = { *m_set, 3, 0, 1, vk::DescriptorType::eCombinedImageSampler, &depthInfo, nullptr    };
        writes[4] = { *m_set, 4, 0, 1, vk::DescriptorType::eStorageImage,         &litInfo,   nullptr    };

        m_device->rawDevice().updateDescriptorSets(writes, {});
    }

    // Private - createLightingPipeline

    void LightSystem::createLightingPipeline()
    {
        vk::PushConstantRange pcRange{};
        pcRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
        pcRange.offset     = 0;
        pcRange.size       = sizeof(LightingPushConstants);

        vk::PipelineLayoutCreateInfo plCI{};
        plCI.setLayoutCount         = 1;
        plCI.pSetLayouts            = &*m_setLayout;
        plCI.pushConstantRangeCount = 1;
        plCI.pPushConstantRanges    = &pcRange;

        m_pipelineLayout = vk::raii::PipelineLayout(m_device->rawDevice(), plCI);

        auto spv    = readShader("Compute_Lighting.spv");
        auto spvSz  = spv.size();

        vk::ShaderModuleCreateInfo smCI{};
        smCI.codeSize = spvSz;
        smCI.pCode    = reinterpret_cast<const uint32_t*>(spv.data());
        vk::raii::ShaderModule shaderModule(m_device->rawDevice(), smCI);

        vk::PipelineShaderStageCreateInfo stageCI{};
        stageCI.stage  = vk::ShaderStageFlagBits::eCompute;
        stageCI.module = *shaderModule;
        stageCI.pName  = "compMain";

        vk::ComputePipelineCreateInfo cpCI{};
        cpCI.stage  = stageCI;
        cpCI.layout = *m_pipelineLayout;

        m_pipeline = vk::raii::Pipeline(m_device->rawDevice(), nullptr, cpCI);
    }

    // Private - readShader

    std::vector<char> LightSystem::readShader(const std::string& fileName)
    {
        const auto path = std::filesystem::current_path() / "Assets" / "Shaders" / fileName;
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            auto err = std::runtime_error("[LightSystem] Cannot open shader: " + path.string());
            Log::handleException(err);
            throw err;
        }

        std::vector<char> buffer(static_cast<size_t>(file.tellg()));
        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        return buffer;
    }
} // namespace gcep::rhi::vulkan
