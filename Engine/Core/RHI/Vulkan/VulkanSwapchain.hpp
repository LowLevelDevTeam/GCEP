#pragma once

// Externals
#include <vulkan/vulkan_raii.hpp>

// STL
#include <vector>

namespace gcep::rhi {

/// @brief Creation parameters for the swapchain and surface.
struct SwapchainDesc
{
    void*    nativeWindowHandle = nullptr; ///< Platform window handle (e.g. @c GLFWwindow*).
    uint32_t width              = 0;       ///< Desired framebuffer width in pixels.
    uint32_t height             = 0;       ///< Desired framebuffer height in pixels.
    bool     vsync              = true;    ///< @c true -> FIFO (vsync); @c false -> Mailbox or Immediate.
};

} // namespace gcep::rhi

namespace gcep::rhi::vulkan
{

/// @brief Owns a @c VkSwapchainKHR, all swapchain @c VkImage handles, and their @c VkImageViews.
///
/// @par Ownership model
///   Swapchain images (@c VkImage) are owned by the driver; this class stores them as plain
///   @c vk::Image (non-owning). Image views (@c VkImageView) are owned here via
///   @c vk::raii::ImageView and destroyed when this object is destroyed.
///
/// @par Lifecycle
///   Constructed by @c VulkanDevice during @c initSwapchain(). On window resize,
///   @c VulkanDevice::recreateSwapchain() destroys the old instance (via @c unique_ptr::reset())
///   before constructing a new one, preventing @c ErrorNativeWindowInUseKHR.
///
/// @par Thread safety
///   Not thread-safe. Used exclusively on the render thread.
class VulkanSwapchain
{
public:
    /// @brief Constructs the swapchain, creates all swapchain images, and creates image views.
    ///
    /// Selects the optimal surface format (prefers @c B8G8R8A8_SRGB / @c eSrgbNonlinear),
    /// present mode (prefers Mailbox when @c !desc.vsync, falls back to FIFO), and extent
    /// (clamps @c desc.width / @c desc.height to the surface capabilities). Requests a minimum
    /// of 3 images, clamped to the surface's @c maxImageCount.
    ///
    /// @param device          The logical device used to create the swapchain and image views.
    /// @param physicalDevice  Used to query surface capabilities, formats, and present modes.
    /// @param surface         The @c VkSurfaceKHR to create the swapchain for.
    /// @param desc            Creation parameters (window size, vsync preference).
    /// @throws std::runtime_error if the surface reports no formats or present modes.
    VulkanSwapchain(const vk::raii::Device&         device,
                    const vk::raii::PhysicalDevice& physicalDevice,
                    const vk::raii::SurfaceKHR&     surface,
                    const SwapchainDesc&             desc);

    ~VulkanSwapchain() = default;

    VulkanSwapchain(const VulkanSwapchain&)            = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&)                 = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&)      = delete;

    /// @brief Acquires the next presentable swapchain image.
    ///
    /// Calls @c vkAcquireNextImageKHR with an infinite timeout. The semaphore is
    /// signaled by the presentation engine when the image is safe to write.
    ///
    /// @param signalSemaphore  Binary semaphore to signal when the image becomes available.
    ///                         Must be unsignaled when this call is made; use one semaphore
    ///                         per frame slot (not per image) to guarantee this.
    /// @returns A @c vk::ResultValue containing:
    ///   - @c eSuccess          - image acquired normally; @c value is the image index.
    ///   - @c eSuboptimalKHR    - still usable; caller may recreate swapchain after present.
    ///   - @c eErrorOutOfDateKHR - swapchain is stale; caller must recreate before presenting.
    [[nodiscard]] vk::ResultValue<uint32_t>
    acquireNextImage(const vk::raii::Semaphore& signalSemaphore) const;

    // ── Accessors ──────────────────────────────────────────────────────────

    /// @brief Returns a reference to the underlying @c vk::raii::SwapchainKHR.
    [[nodiscard]] const vk::raii::SwapchainKHR&           raw()        const noexcept { return m_swapchain;  }

    /// @brief Returns the driver-owned swapchain images (non-owning handles).
    [[nodiscard]] const std::vector<vk::Image>&            images()     const noexcept { return m_images;     }

    /// @brief Returns the image views, one per swapchain image, in the same order as @c images().
    [[nodiscard]] const std::vector<vk::raii::ImageView>&  imageViews() const noexcept { return m_imageViews; }

    /// @brief Returns the surface format selected at construction (e.g. @c eB8G8R8A8Srgb).
    [[nodiscard]] vk::Format    format()     const noexcept { return m_format; }

    /// @brief Returns the swapchain extent (framebuffer resolution in pixels).
    [[nodiscard]] vk::Extent2D  extent()     const noexcept { return m_extent; }

    /// @brief Returns the number of swapchain images created by the driver.
    [[nodiscard]] uint32_t      imageCount() const noexcept { return static_cast<uint32_t>(m_images.size()); }

private:
    /// @brief Creates the @c VkSwapchainKHR and populates @c m_images, @c m_format, @c m_extent.
    /// @param device          Logical device.
    /// @param physicalDevice  Used to query surface capabilities and supported formats.
    /// @param surface         Target surface.
    /// @param desc            Width, height, and vsync preference.
    void createSwapchain(const vk::raii::Device&         device,
                         const vk::raii::PhysicalDevice& physicalDevice,
                         const vk::raii::SurfaceKHR&     surface,
                         const SwapchainDesc&            desc);

    /// @brief Creates one @c VkImageView per swapchain image and stores them in @c m_imageViews.
    /// @param device  Logical device used to create the image views.
    void createImageViews(const vk::raii::Device& device);

    /// @brief Chooses the best available surface format.
    ///
    /// Prefers @c eB8G8R8A8Srgb with @c eSrgbNonlinear color space.
    /// Falls back to the first available format if the preferred one is not found.
    ///
    /// @param available  List of formats reported by @c vkGetPhysicalDeviceSurfaceFormatsKHR.
    /// @returns The selected @c vk::SurfaceFormatKHR.
    [[nodiscard]] static vk::SurfaceFormatKHR chooseSurfaceFormat(
            const std::vector<vk::SurfaceFormatKHR>& available);

    /// @brief Chooses the best available present mode.
    ///
    /// When @c vsync is @c false, prefers Mailbox (triple-buffering without tearing),
    /// then Immediate. Always falls back to FIFO, which is guaranteed by the Vulkan spec.
    ///
    /// @param available  List of present modes reported by @c vkGetPhysicalDeviceSurfacePresentModesKHR.
    /// @param vsync      @c true to force FIFO; @c false to prefer a low-latency mode.
    /// @returns The selected @c vk::PresentModeKHR.
    [[nodiscard]] static vk::PresentModeKHR choosePresentMode(
            const std::vector<vk::PresentModeKHR>& available,
            bool vsync);

    /// @brief Clamps the desired framebuffer size to what the surface supports.
    ///
    /// If the surface reports a fixed extent (@c currentExtent.width != UINT32_MAX),
    /// that value is returned directly. Otherwise, @p desiredWidth and @p desiredHeight
    /// are clamped to @c [minImageExtent, maxImageExtent].
    ///
    /// @param caps           Surface capabilities from @c vkGetPhysicalDeviceSurfaceCapabilitiesKHR.
    /// @param desiredWidth   Requested framebuffer width.
    /// @param desiredHeight  Requested framebuffer height.
    /// @returns The clamped @c vk::Extent2D to use for swapchain creation.
    [[nodiscard]] static vk::Extent2D chooseExtent(
            const vk::SurfaceCapabilitiesKHR& caps,
            uint32_t desiredWidth,
            uint32_t desiredHeight);

private:
    vk::raii::SwapchainKHR           m_swapchain = nullptr;
    std::vector<vk::Image>           m_images;
    std::vector<vk::raii::ImageView> m_imageViews;
    vk::Format                       m_format = vk::Format::eUndefined;
    vk::Extent2D                     m_extent{};
};

} // Namespace gcep::rhi::vulkan