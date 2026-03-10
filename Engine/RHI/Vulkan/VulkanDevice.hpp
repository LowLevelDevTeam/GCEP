#pragma once

#include <Engine/RHI/Vulkan/VulkanFrameContext.hpp>
#include <Engine/RHI/Vulkan/VulkanSwapchain.hpp>

// Externals
#include <vulkan/vulkan_raii.hpp>

// STL
#include <memory>
#include <vector>

namespace gcep::rhi::vulkan
{

#ifdef NDEBUG
    inline constexpr bool enableValidationLayers = false;
#else
    inline constexpr bool enableValidationLayers = true;
#endif

/// @brief Validation layers requested when @c enableValidationLayers is @c true.
inline const std::vector<const char*> validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

/// @brief Device extensions required by the engine, at minimum @c VK_KHR_swapchain.
inline const std::vector<const char*> deviceExtensions =
{
    vk::KHRSwapchainExtensionName
};

/// @brief Central Vulkan device manager.
///
/// Owns the Vulkan instance, debug messenger, surface, physical device, logical device,
/// queue, swapchain, and all per-frame-in-flight contexts. Coordinates the full
/// frame lifecycle (acquire -> record -> submit -> present).
///
/// @par Init sequence
///   @c createVulkanDevice() must be the first call after construction. It runs all
///   eight init stages in mandatory order:
///   @c initInstance -> @c initDebugMessenger -> @c initSurface -> @c selectPhysicalDevice
///   -> @c initLogicalDevice -> @c initSwapchain -> @c initFrameContexts.
///
/// @par Frame lifecycle
///   @code
///   if (!device.beginFrame()) return;   // fence wait + acquire
///   // ... record via currentCommandBuffer() ...
///   device.endFrame();                  // submit + present
///   @endcode
///
/// @par Thread safety
///   All public methods must be called from a single render thread.
class VulkanDevice
{
public:
    VulkanDevice() = default;

    /// @brief Destroys all Vulkan objects in safe reverse-dependency order.
    ///
    /// Frame contexts are destroyed before the device; the device before the surface;
    /// the surface before the instance. All RAII members handle their own cleanup.
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&)            = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&)                 = delete;
    VulkanDevice& operator=(VulkanDevice&&)      = delete;

    // Lifecycle

    /// @brief Runs all init stages in the mandatory order.
    ///
    /// Must be called exactly once before any other method. Initialises the Vulkan
    /// instance, validation layers (if enabled), window surface, physical device,
    /// logical device, swapchain, and per-frame contexts.
    ///
    /// @param desc  Surface creation parameters (window handle, width, height, vsync).
    /// @throws std::runtime_error if any init stage fails.
    void createVulkanDevice(const SwapchainDesc& desc);

    /// @brief Begins a new frame: waits on the in-flight fence, acquires a swapchain
    ///        image, and opens the frame's command buffer for recording.
    ///
    /// Selects the next frame slot (round-robin over @c MAX_FRAMES_IN_FLIGHT),
    /// calls @c waitAndReset() on its @c VulkanFrameContext to stall the CPU until
    /// the GPU is done with that slot, then signals @c presentCompleteSemaphore via
    /// @c vkAcquireNextImageKHR and calls @c beginRecording() on the command list.
    ///
    /// @returns @c true if recording can proceed; @c false if the swapchain is
    ///          out-of-date (@c eErrorOutOfDateKHR), in which case the caller must
    ///          skip the rest of the frame.
    /// @throws std::runtime_error on any acquire result other than @c eSuccess,
    ///         @c eSuboptimalKHR, or @c eErrorOutOfDateKHR.
    [[nodiscard]] bool beginFrame();

    /// @brief Ends the current frame: closes the command buffer, submits it, and presents.
    ///
    /// Calls @c endRecording() on the command list, submits to the graphics queue
    /// with @c presentCompleteSemaphore as the wait semaphore and
    /// @c m_renderFinishedSemaphores[m_imageIndex] as the signal semaphore, then
    /// calls @c vkQueuePresentKHR. Advances @c m_frameIndex.
    ///
    /// @throws std::runtime_error if @c vkQueuePresentKHR returns a result other
    ///         than @c eSuccess or @c eSuboptimalKHR.
    void endFrame();

    /// @brief Blocks until the device has finished all submitted work.
    ///
    /// Wraps @c vkDeviceWaitIdle. Must be called before destroying any resource
    /// that may still be in use by the GPU (e.g., before swapchain recreation
    /// or application shutdown).
    void waitIdle();

    /// @brief Destroys and recreates the swapchain for a new framebuffer size.
    ///
    /// Calls @c waitIdle(), destroys the existing @c VulkanSwapchain (via
    /// @c unique_ptr::reset() to avoid @c ErrorNativeWindowInUseKHR), then
    /// constructs a new one at @p width × @p height. Also rebuilds the
    /// per-image acquire semaphores to match the new image count.
    ///
    /// @param width   New framebuffer width in pixels.
    /// @param height  New framebuffer height in pixels.
    void recreateSwapchain(uint32_t width, uint32_t height, SwapchainDesc& desc);

    // Per-frame accessors

    /// @brief Returns the swapchain image acquired in the current @c beginFrame() call.
    ///
    /// Valid only between a successful @c beginFrame() and the corresponding @c endFrame().
    /// Use this handle to record layout-transition barriers targeting the swapchain image.
    ///
    /// @returns A non-owning @c vk::Image handle. Do not destroy or cache across frames.
    [[nodiscard]] vk::Image currentSwapchainImage() const noexcept
    { return m_swapchain->images()[m_imageIndex]; }

    /// @brief Returns the primary command buffer open for recording in the current frame slot.
    ///
    /// Valid only between a successful @c beginFrame() and the corresponding @c endFrame().
    /// Prefer @c VulkanCommandList methods where possible; use this for direct Vulkan calls
    /// (e.g., @c pipelineBarrier2, @c beginRendering).
    ///
    /// @returns A non-owning @c vk::CommandBuffer handle.
    [[nodiscard]] vk::CommandBuffer currentCommandBuffer() const noexcept
    { return *m_frames[m_frameIndex]->commandBuffer; }

    /// @brief Returns the zero-based index of the swapchain image acquired this frame.
    ///
    /// Use this to index into @c swapchainImageViews() to obtain the correct image view
    /// for a render pass attachment targeting the swapchain.
    [[nodiscard]] uint32_t currentImageIndex() const noexcept { return m_imageIndex; }

    // Raw Vulkan handle accessors

    /// @brief Returns the RAII logical device.
    [[nodiscard]] const vk::raii::Device& rawDevice() const noexcept { return m_device; }

    /// @brief Returns the RAII physical device (GPU).
    [[nodiscard]] vk::raii::PhysicalDevice& rawPhysDevice() noexcept { return m_physicalDevice; }
    [[nodiscard]] const vk::raii::PhysicalDevice& constRawPhysDevice() const noexcept { return m_physicalDevice; }

    /// @brief Returns the RAII graphics/present queue.
    [[nodiscard]] const vk::raii::Queue& rawQueue() const noexcept { return m_queue; }

    /// @brief Returns the RAII Vulkan instance.
    [[nodiscard]] const vk::raii::Instance& rawInstance() const noexcept { return m_instance; }

    /// @brief Returns the queue family index used for both graphics and presentation.
    [[nodiscard]] uint32_t queueFamily() const noexcept { return m_queueFamily; }

    // Swapchain property accessors

    /// @brief Returns the surface format chosen at swapchain creation (e.g. @c eB8G8R8A8Srgb).
    [[nodiscard]] vk::Format swapchainFormat() const noexcept { return m_swapchain->format(); }

    /// @brief Returns the current swapchain resolution in pixels.
    [[nodiscard]] vk::Extent2D swapchainExtent() const noexcept { return m_swapchain->extent(); }

    /// @brief Returns the total number of swapchain images allocated by the driver.
    [[nodiscard]] uint32_t swapchainImageCount() const noexcept { return m_swapchain->imageCount(); }

    /// @brief Returns the image views for all swapchain images, in the same order as the images.
    ///
    /// Index with @c currentImageIndex() to obtain the view for the currently acquired image.
    [[nodiscard]] const std::vector<vk::raii::ImageView>& swapchainImageViews() const noexcept
    { return m_swapchain->imageViews(); }

private:
    // Init stages

    /// @brief Creates the @c VkInstance with required layers and extensions.
    /// @throws std::runtime_error if a required layer or extension is not available.
    void initInstance();

    /// @brief Creates the @c VkDebugUtilsMessengerEXT. No-op in release builds.
    void initDebugMessenger();

    /// @brief Creates the @c VkSurfaceKHR from the native window handle in @p desc.
    /// @param desc  Must contain a valid @c nativeWindowHandle (e.g. @c GLFWwindow*).
    void initSurface(const SwapchainDesc& desc);

    /// @brief Enumerates GPUs, scores each suitable one, and selects the highest scorer.
    ///
    /// Scoring: discrete GPU +10000, @c maxImageDimension2D added.
    /// Hard requirements: @c VK_KHR_swapchain, Vulkan 1.3 dynamic rendering +
    /// synchronization2, anisotropic sampling, graphics + present on one queue,
    /// at least one surface format and present mode.
    ///
    /// @throws std::runtime_error if no suitable GPU is found.
    void selectPhysicalDevice();

    /// @brief Creates the @c VkDevice and retrieves the graphics/present queue.
    ///
    /// Enables: @c shaderDrawParameters (Vulkan 1.1), @c extendedDynamicState,
    /// @c dynamicRendering, @c synchronization2 (Vulkan 1.3), @c samplerAnisotropy,
    /// @c geometryShader.
    ///
    /// @throws std::runtime_error if no queue family satisfies both graphics and present.
    void initLogicalDevice();

    /// @brief Constructs a @c VulkanSwapchain from the window surface and @p desc.
    /// @param desc  Width, height, and vsync preference.
    void initSwapchain(const SwapchainDesc& desc);

    /// @brief Allocates @c MAX_FRAMES_IN_FLIGHT @c VulkanFrameContext objects and
    ///        one @c vk::raii::Semaphore per swapchain image for render-finished signaling.
    void initFrameContexts();

    // Helpers

    /// @brief Returns @c true if @p device meets all hard requirements for rendering.
    ///
    /// Checks: required device extensions, Vulkan 1.3 features
    /// (@c dynamicRendering, @c synchronization2), @c samplerAnisotropy,
    /// a combined graphics+present queue, and at least one surface format
    /// and present mode.
    ///
    /// @param device  The physical device to evaluate.
    /// @returns @c true if the device is suitable; @c false otherwise.
    [[nodiscard]] bool isDeviceSuitable(const vk::raii::PhysicalDevice& device) const;

    /// @brief Finds the index of a queue family that supports graphics, compute and presentation.
    ///
    /// @param device  The physical device to query.
    /// @returns The queue family index.
    /// @throws std::runtime_error if no such family exists.
    [[nodiscard]] uint32_t findQueueFamily(const vk::raii::PhysicalDevice& device) const;

    /// @brief Vulkan debug messenger callback; prints validation messages to @c std::cerr.
    ///
    /// Registered with @c VkDebugUtilsMessengerEXT in @c initDebugMessenger().
    /// Always returns @c vk::False so the triggering call is not aborted.
    ///
    /// @param severity     Message severity (verbose / info / warning / error).
    /// @param type         Message type (general / validation / performance).
    /// @param pCallbackData  Struct containing the message string and object info.
    /// @param pUserData    User pointer set at messenger creation (unused; @c nullptr).
    /// @returns @c vk::False — the Vulkan spec requires the callback to return @c VK_FALSE.
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
            vk::DebugUtilsMessageTypeFlagsEXT             type,
            const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void*                                         pUserData);

private:
    vk::raii::Context                                m_context;
    vk::raii::Instance                               m_instance       = nullptr;
    vk::raii::DebugUtilsMessengerEXT                 m_debugMessenger = nullptr;
    vk::raii::SurfaceKHR                             m_surface        = nullptr;
    vk::raii::PhysicalDevice                         m_physicalDevice = nullptr;
    vk::raii::Device                                 m_device         = nullptr;
    vk::raii::Queue                                  m_queue          = nullptr;
    uint32_t                                         m_queueFamily    = ~0u;
    std::unique_ptr<VulkanSwapchain>                 m_swapchain      = nullptr;
    std::vector<std::unique_ptr<VulkanFrameContext>> m_frames;

    /// One semaphore per swapchain image; signaled by @c endFrame() and waited on
    /// by @c vkQueuePresentKHR to ensure rendering is complete before flip.
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;

    SwapchainDesc m_swapchainDesc;

    /// Index of the frame slot currently being recorded [0, MAX_FRAMES_IN_FLIGHT).
    uint32_t m_frameIndex = 0;

    /// Index of the swapchain image acquired in the current @c beginFrame() call.
    uint32_t m_imageIndex = 0;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
};

} // Namespace gcep::rhi::vulkan
