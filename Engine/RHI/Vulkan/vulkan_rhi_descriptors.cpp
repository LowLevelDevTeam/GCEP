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
    // ============================================================================
    // Private - scene UBO
    // ============================================================================

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

    // ============================================================================
    // Private - bindless textures
    // ============================================================================

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
} // namespace gcep::rhi::vulkan
