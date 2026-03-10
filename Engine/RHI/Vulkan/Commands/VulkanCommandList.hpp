#pragma once

// Externals
#include <vulkan/vulkan_raii.hpp>

namespace gcep::rhi::vulkan
{

/// @brief Thin RAII wrapper around a @c vk::CommandBuffer for render-thread recording.
///
/// @c VulkanCommandList does not own the command buffer it wraps - ownership remains
/// with the parent @c VulkanFrameContext. The class exists to provide a typed recording
/// interface and to expose the raw handle for direct Vulkan calls.
///
/// @par Lifetime
///   The @c vk::CommandBuffer passed to the constructor must remain valid for the entire
///   lifetime of this object. In practice this is guaranteed because
///   @c VulkanFrameContext constructs @c VulkanCommandList after allocating the buffer
///   and destroys it before freeing the command pool.
///
/// @par Thread safety
///   Not thread-safe. One instance is owned per frame slot and used exclusively
///   from the render thread.
class VulkanCommandList
{
public:
    /// @brief Constructs a command list wrapping the given command buffer.
    /// @param cmd  A non-owning handle to a command buffer allocated from a
    ///             @c VkCommandPool.  Must remain valid for the object's lifetime.
    explicit VulkanCommandList(vk::CommandBuffer cmd) : m_cmd(cmd) {}

    ~VulkanCommandList() = default;

    VulkanCommandList(const VulkanCommandList&)            = delete;
    VulkanCommandList& operator=(const VulkanCommandList&) = delete;
    VulkanCommandList(VulkanCommandList&&)                 = delete;
    VulkanCommandList& operator=(VulkanCommandList&&)      = delete;

    /// @brief Opens the command buffer for recording with @c eOneTimeSubmit semantics.
    ///
    /// Must be called once per frame before any recording commands are issued.
    /// Corresponds to @c vkBeginCommandBuffer with
    /// @c VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT.
    void beginRecording();

    /// @brief Closes the command buffer, making it ready for submission.
    ///
    /// Must be called after all commands have been recorded and before the buffer
    /// is passed to @c vkQueueSubmit. Corresponds to @c vkEndCommandBuffer.
    void endRecording();

    /// @brief Returns the underlying non-owning @c vk::CommandBuffer handle.
    ///
    /// Use this to issue Vulkan commands directly (barriers, draw calls, etc.)
    /// when the typed API does not cover a required operation.
    ///
    /// @returns A non-owning @c vk::CommandBuffer handle. Valid as long as the
    ///          parent @c VulkanFrameContext is alive.
    [[nodiscard]] vk::CommandBuffer raw() const noexcept { return m_cmd; }

private:
    /// Non-owning handle to the wrapped command buffer.
    vk::CommandBuffer m_cmd;
};

} // Namespace gcep::rhi::vulkan
