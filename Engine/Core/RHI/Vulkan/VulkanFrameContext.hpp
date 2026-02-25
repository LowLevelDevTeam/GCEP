#pragma once

#include <RHI/Vulkan/Commands/VulkanCommandList.hpp>

// Externals
#include <vulkan/vulkan_raii.hpp>

// STL
#include <memory>
#include <stdexcept>

namespace gcep::rhi::vulkan
{

/// @brief Owns all per-frame-in-flight Vulkan objects for a single frame slot.
///
/// With @c MAX_FRAMES_IN_FLIGHT = 2 the CPU can record frame N+1 while the GPU
/// is still executing frame N, without ever stalling on the second frame.
///
/// @par Member roles
///   - @c commandPool              - Created with @c eTransient; reset wholesale each
///                                   frame via @c vkResetCommandPool, which is faster
///                                   than resetting individual buffers.
///   - @c commandBuffer            - Primary buffer allocated from the pool; re-recorded
///                                   every frame.
///   - @c commandList              - Typed wrapper around @c commandBuffer.
///   - @c inFlightFence            - Pre-signaled at construction so frame 0 does not
///                                   stall before the GPU has done any work. Waited on
///                                   and reset in @c waitAndReset() before each recording.
///   - @c presentCompleteSemaphore - Signaled by @c vkAcquireNextImageKHR when the
///                                   swapchain image is safe to write. The GPU waits
///                                   at @c eColorAttachmentOutput before writing color.
///
/// @par Destruction order
///   Member declaration order is also the reverse-destruction order enforced by C++.
///   @c commandList -> @c commandBuffer -> @c commandPool -> @c inFlightFence ->
///   @c presentCompleteSemaphore.
///
/// @par Thread safety
///   Not thread-safe. Owned and used exclusively by the render thread.
struct VulkanFrameContext
{
    /// @brief Constructs and initialises all per-frame Vulkan objects.
    ///
    /// Allocates a transient command pool for @p queueFamily, allocates a single
    /// primary command buffer from it, creates a pre-signaled in-flight fence, and
    /// creates the acquire semaphore. The @c VulkanCommandList is constructed last,
    /// wrapping the allocated command buffer.
    ///
    /// @param device       The logical device used to create all child objects.
    ///                     Must remain valid for the lifetime of this struct.
    /// @param queueFamily  Index of the graphics/present queue family.  All objects
    ///                     are created targeting this family.
    VulkanFrameContext(const vk::raii::Device& device,
                       uint32_t                 queueFamily);

    ~VulkanFrameContext() = default;

    VulkanFrameContext(const VulkanFrameContext&)            = delete;
    VulkanFrameContext& operator=(const VulkanFrameContext&) = delete;
    VulkanFrameContext(VulkanFrameContext&&)                 = delete;
    VulkanFrameContext& operator=(VulkanFrameContext&&)      = delete;

    /// @brief Blocks the CPU until the GPU finishes all work submitted with this
    ///        frame slot's fence, then resets the fence and command pool.
    ///
    /// This is the "begin of frame" CPU stall. Calling @c vkResetCommandPool
    /// in one shot is faster than resetting individual command buffers because
    /// the driver can reclaim all allocator memory at once.
    ///
    /// @param device  The logical device; must be the same one used at construction.
    /// @throws std::runtime_error if @c vkWaitForFences returns any result other
    ///         than @c VK_SUCCESS (e.g. @c VK_ERROR_DEVICE_LOST).
    void waitAndReset(const vk::raii::Device& device);

    // Members

    /// Transient command pool; reset wholesale each frame.
    vk::raii::CommandPool              commandPool;

    /// Primary command buffer re-recorded every frame.
    vk::raii::CommandBuffer            commandBuffer;

    /// Pre-signaled fence. Waited on before recording begins; reset immediately after.
    vk::raii::Fence                    inFlightFence;

    /// Binary semaphore signaled by @c vkAcquireNextImageKHR.
    /// Waited on by the GPU at @c eColorAttachmentOutput before any color writes.
    vk::raii::Semaphore                presentCompleteSemaphore;

    /// Typed command-recording wrapper around @c commandBuffer.
    std::unique_ptr<VulkanCommandList> commandList;
};

} // Namespace gcep::rhi::vulkan
