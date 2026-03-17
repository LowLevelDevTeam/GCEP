#include "vulkan_rhi_swapchain.hpp"

// STL
#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace gcep::rhi::vulkan
{
    VulkanSwapchain::VulkanSwapchain(const vk::raii::Device&         device,
                                     const vk::raii::PhysicalDevice& physicalDevice,
                                     const vk::raii::SurfaceKHR&     surface,
                                     const SwapchainDesc&            desc)
    {
        createSwapchain(device, physicalDevice, surface, desc);
        createImageViews(device);
    }

    void VulkanSwapchain::createSwapchain(const vk::raii::Device&          device,
                                           const vk::raii::PhysicalDevice& physicalDevice,
                                           const vk::raii::SurfaceKHR&     surface,
                                           const SwapchainDesc&            desc)
    {
        const auto caps    = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        const auto formats = physicalDevice.getSurfaceFormatsKHR(*surface);
        const auto modes   = physicalDevice.getSurfacePresentModesKHR(*surface);

        if (formats.empty() || modes.empty())
        {
            throw std::runtime_error("VulkanSwapchain: surface has no formats or present modes");
        }

        const vk::SurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
        const vk::PresentModeKHR   presentMode   = choosePresentMode(modes, desc.vsync);
        const vk::Extent2D         extent        = chooseExtent(caps, desc.width, desc.height);

        // Image count: clamp preferred count to [min, max].
        uint32_t imageCount = std::max(3u, caps.minImageCount);
        if (caps.maxImageCount > 0)
        {
            imageCount = std::min(imageCount, caps.maxImageCount);
        }

        vk::SwapchainCreateInfoKHR createInfo{};
        createInfo.surface          = *surface;
        createInfo.minImageCount    = imageCount;
        createInfo.imageFormat      = surfaceFormat.format;
        createInfo.imageColorSpace  = surfaceFormat.colorSpace;
        createInfo.imageExtent      = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.preTransform     = caps.currentTransform;
        createInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode      = presentMode;
        createInfo.clipped          = vk::True;
        createInfo.oldSwapchain     = nullptr;

        m_swapchain = vk::raii::SwapchainKHR(device, createInfo);
        m_images    = m_swapchain.getImages();
        m_format    = surfaceFormat.format;
        m_extent    = extent;
    }

    void VulkanSwapchain::createImageViews(const vk::raii::Device& device)
    {
        m_imageViews.clear();
        m_imageViews.reserve(m_images.size());

        vk::ImageViewCreateInfo createInfo{};
        createInfo.viewType         = vk::ImageViewType::e2D;
        createInfo.format           = m_format;
        createInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        for (const vk::Image& img : m_images)
        {
            createInfo.image = img;
            m_imageViews.emplace_back(device, createInfo);
        }
    }

    vk::SurfaceFormatKHR VulkanSwapchain::chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available)
    {
        for (const auto& surfaceFormat : available)
        {
            if (surfaceFormat.format == vk::Format::eB8G8R8A8Srgb && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return surfaceFormat;
            }
        }

        return available.front();
    }

    vk::PresentModeKHR VulkanSwapchain::choosePresentMode(const std::vector<vk::PresentModeKHR>& available, bool vsync)
    {
        assert(std::ranges::any_of(available, [](auto m){ return m == vk::PresentModeKHR::eFifo; }) && "FIFO present mode needs to be available");

        if (!vsync)
        {
            for (const auto m : available)
            {
                if (m == vk::PresentModeKHR::eMailbox) return m;
            }
            for (const auto m : available)
            {
                if (m == vk::PresentModeKHR::eImmediate) return m;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D VulkanSwapchain::chooseExtent(const vk::SurfaceCapabilitiesKHR& caps, uint32_t desiredWidth, uint32_t desiredHeight)
    {
        if (caps.currentExtent.width != UINT32_MAX)
        {
            return caps.currentExtent;
        }

        return
        {
            std::clamp(desiredWidth,  caps.minImageExtent.width,  caps.maxImageExtent.width),
            std::clamp(desiredHeight, caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }

    // Fixes versions of Vulkan that use std::pair instead of vk::ResultValue
    template<typename T>
    concept HasFirstSecond = requires(T t)
    {
        t.first;
        t.second;
    };

    template<HasFirstSecond T>
    std::pair<vk::Result, uint32_t> extractResult(T& result)
    {
        return { result.first, result.second };
    }

    template<typename T>
    std::pair<vk::Result, uint32_t> extractResult(T& result)
    {
        return { result.result, result.value };
    }

    std::pair<vk::Result, uint32_t> VulkanSwapchain::acquireNextImage(const vk::raii::Semaphore& signalSemaphore) const
    {
        auto result = m_swapchain.acquireNextImage(UINT64_MAX, *signalSemaphore, nullptr);
        return extractResult(result);
    }
} // namespace gcep::rhi::vulkan
