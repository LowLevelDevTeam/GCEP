#pragma once

// Externals
#include <vulkan/vulkan_raii.hpp>

// STL
#include <vector>

namespace gcep::rhi::vulkan
{

/// @brief Per-frame Vulkan descriptor set allocator.
///
/// Manages a pool of @c VkDescriptorPool objects using a grow-or-reset strategy:
/// - A single "current" pool satisfies allocations until it is exhausted.
/// - When exhausted, a new pool is created and the full one is retired.
/// - At the end of each frame, @c reset() reclaims all pools in one call,
///   which is faster than freeing individual descriptor sets.
///
/// @par Thread safety
///   Not thread-safe. Must be called exclusively from the render thread.
class VulkanDescriptorAllocator
{
public:
    /// @brief Constructs the allocator and creates an initial descriptor pool.
    /// @param device  The logical device used to create and manage pools.
    ///                The reference must remain valid for the lifetime of this object.
    explicit VulkanDescriptorAllocator(const vk::raii::Device& device);

    ~VulkanDescriptorAllocator() = default;

    VulkanDescriptorAllocator(const VulkanDescriptorAllocator&)            = delete;
    VulkanDescriptorAllocator& operator=(const VulkanDescriptorAllocator&) = delete;
    VulkanDescriptorAllocator(VulkanDescriptorAllocator&&)                 = delete;
    VulkanDescriptorAllocator& operator=(VulkanDescriptorAllocator&&)      = delete;

    /// @brief Allocates a single descriptor set matching the given layout.
    ///
    /// Tries to allocate from the current pool. If the pool is exhausted
    /// (@c vk::Result::eErrorOutOfPoolMemory or @c eErrorFragmentedPool),
    /// the current pool is retired to @c m_fullPools, a new pool is created,
    /// and allocation is retried from the new pool.
    ///
    /// @param layout  The descriptor set layout that the allocated set must conform to.
    /// @returns A newly allocated @c vk::raii::DescriptorSet.
    /// @throws std::runtime_error if allocation fails for any reason other than pool exhaustion.
    vk::raii::DescriptorSet allocate(const vk::raii::DescriptorSetLayout& layout);

    /// @brief Resets all pools, reclaiming every descriptor set allocated this frame.
    ///
    /// Calls @c vkResetDescriptorPool on the current pool and all retired full pools,
    /// then clears the full-pool list so they are available for reuse next frame.
    /// Must be called once per frame before any @c allocate() calls for that frame.
    void reset();

private:
    /// @brief Creates a new @c VkDescriptorPool sized by @c MAX_SETS and @c POOL_SIZES
    ///        and assigns it to @c m_currentPool.
    void createPool();

    /// The logical device used to create pools and allocate sets.
    const vk::raii::Device&              m_device;

    /// The pool currently accepting allocations.
    vk::raii::DescriptorPool             m_currentPool = nullptr;

    /// Pools that have been exhausted this frame; reset and recycled each frame.
    std::vector<vk::raii::DescriptorPool> m_fullPools;

    /// Maximum number of descriptor sets per pool.
    static constexpr uint32_t MAX_SETS = 256;

    /// Descriptor type capacities per pool. Each pool supports up to 128
    /// uniform-buffer descriptors and 128 combined-image-sampler descriptors.
    static constexpr std::array<vk::DescriptorPoolSize, 2> POOL_SIZES =
    {{
        { vk::DescriptorType::eUniformBuffer,         128 },
        { vk::DescriptorType::eCombinedImageSampler,  128 },
    }};
};

} // Namespace gcep::rhi::vulkan
