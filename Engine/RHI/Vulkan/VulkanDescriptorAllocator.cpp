#include "VulkanDescriptorAllocator.hpp"

// STL
#include <stdexcept>

namespace gcep::rhi::vulkan
{

VulkanDescriptorAllocator::VulkanDescriptorAllocator(const vk::raii::Device& device)
    : m_device(device)
{
    createPool();
}

vk::raii::DescriptorSet VulkanDescriptorAllocator::allocate(
    const vk::raii::DescriptorSetLayout& layout)
{
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool     = *m_currentPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &*layout;

    try
    {
        auto sets = m_device.allocateDescriptorSets(allocInfo);
        return std::move(sets.front());
    }
    catch (const vk::OutOfPoolMemoryError&)
    {
        m_fullPools.push_back(std::move(m_currentPool));
        createPool();

        allocInfo.descriptorPool = *m_currentPool;
        auto sets = m_device.allocateDescriptorSets(allocInfo);
        return std::move(sets.front());
    }
}

void VulkanDescriptorAllocator::reset()
{
    m_currentPool.reset(vk::DescriptorPoolResetFlags{});

    for (auto& pool : m_fullPools)
    {
        pool.reset(vk::DescriptorPoolResetFlags{});
    }

    m_fullPools.clear();
}

void VulkanDescriptorAllocator::createPool()
{
    vk::DescriptorPoolCreateInfo createInfo{};
    createInfo.maxSets       = MAX_SETS;
    createInfo.poolSizeCount = static_cast<uint32_t>(POOL_SIZES.size());
    createInfo.pPoolSizes    = POOL_SIZES.data();

    m_currentPool = m_device.createDescriptorPool(createInfo);
}

} // Namespace gcep::rhi::vulkan
