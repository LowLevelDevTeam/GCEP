#include "vulkan_device.hpp"

// Internals
#include <Log/log.hpp>

// Externals
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// STL
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <ranges>
#include <stdexcept>
#include <string>

namespace gcep::rhi::vulkan
{
    void VulkanDevice::createVulkanDevice(const SwapchainDesc& desc)
    {
        m_swapchainDesc = desc;
        initInstance();
        initDebugMessenger();
        initSurface(desc);
        selectPhysicalDevice();
        initLogicalDevice();
        initSwapchain(desc);
        initFrameContexts();
        Log::info("Vulkan device creation completed successfully");
    }

    VulkanDevice::~VulkanDevice()
    {
        if (*m_device)
        {
            m_device.waitIdle();
            m_frames.clear();
            m_swapchain.reset();
        }
    }

    static std::vector<const char*> getRequiredInstanceExtensions()
    {
        uint32_t     count       = 0;
        const char** extensions  = glfwGetRequiredInstanceExtensions(&count);
        std::vector<const char*> result(extensions, extensions + count);

        if constexpr (enableValidationLayers)
        {
            result.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return result;
    }

    void VulkanDevice::initInstance()
    {
        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName   = "GC Engine Paris";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName        = "GC Engine Paris";
        appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion         = vk::ApiVersion14;

        std::vector<const char*> enabledLayers;
        if constexpr (enableValidationLayers)
        {
            auto available = m_context.enumerateInstanceLayerProperties();
            for (const char* req : validationLayers)
            {
                bool found = std::ranges::any_of(available, [req](const vk::LayerProperties& p)
                    { return std::strcmp(p.layerName, req) == 0; });

                if (!found)
                {
                    throw std::runtime_error(std::string("VulkanDevice: required layer missing: ") + req);
                }
            }
            enabledLayers = validationLayers;
        }

        auto requiredExtensions             = getRequiredInstanceExtensions();
        auto availableExtensions  = m_context.enumerateInstanceExtensionProperties();
        for (const char* required : requiredExtensions)
        {
            bool found = std::ranges::any_of(availableExtensions, [required](const vk::ExtensionProperties& p)
                { return std::strcmp(p.extensionName, required) == 0; });

            if (!found)
            {
                throw std::runtime_error(std::string("VulkanDevice: required instance extension unavailable: ") + required);
            }
        }

        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledLayerCount       = static_cast<uint32_t>(enabledLayers.size());
        createInfo.ppEnabledLayerNames     = enabledLayers.data();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        m_instance = vk::raii::Instance(m_context, createInfo);
    }

    void VulkanDevice::initDebugMessenger()
    {
        if constexpr (!enableValidationLayers) return;

        vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.messageSeverity =
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        createInfo.messageType =
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral    |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        createInfo.pfnUserCallback = &VulkanDevice::debugCallback;

        m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(createInfo);
    }

    VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDevice::debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT  severity,
        vk::DebugUtilsMessageTypeFlagsEXT          type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*)
    {
        std::cerr << "Validation Layer: \n"
                  << "Type : " << vk::to_string(type) << " Message : " << pCallbackData->pMessage
                  << std::endl;

        return vk::False;
    }

    void VulkanDevice::initSurface(const SwapchainDesc& desc)
    {
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(*m_instance, static_cast<GLFWwindow*>(desc.nativeWindowHandle), nullptr, &surface) != VK_SUCCESS)
        {
            throw std::runtime_error("VulkanDevice: Failed to create window surface!");
        }
        m_surface = vk::raii::SurfaceKHR(m_instance, surface);
    }

    bool VulkanDevice::isDeviceSuitable(const vk::raii::PhysicalDevice& device) const
    {
        auto extensions = device.enumerateDeviceExtensionProperties();
        auto deviceFeatures = device.getFeatures();

        if (deviceFeatures.geometryShader == vk::True && deviceFeatures.samplerAnisotropy == vk::True)
        {
            return true;
        }

        return false;
    }

    uint32_t VulkanDevice::findQueueFamily(const vk::raii::PhysicalDevice& device) const
    {
        const auto props = device.getQueueFamilyProperties();
        for (uint32_t i = 0; i < static_cast<uint32_t>(props.size()); ++i)
        {
            const bool graphics = static_cast<bool>(props[i].queueFlags & vk::QueueFlagBits::eGraphics);
            const bool compute  = static_cast<bool>(props[i].queueFlags & vk::QueueFlagBits::eCompute);
            const bool present  = device.getSurfaceSupportKHR(i, *m_surface);
            if (graphics && compute && present) return i;
        }
        throw std::runtime_error("VulkanDevice: no graphics + compute + present queue family found");
    }

    void VulkanDevice::selectPhysicalDevice()
    {
        std::vector<vk::raii::PhysicalDevice> devices = m_instance.enumeratePhysicalDevices();
        if (devices.empty())
        {
            throw std::runtime_error("VulkanDevice : Failed to find GPUs with Vulkan support!");
        }

        std::multimap<int, vk::raii::PhysicalDevice> candidates;
        for (auto& device : devices)
        {
            if (!isDeviceSuitable(device))
            {
                continue;
            }
            auto deviceProperties = device.getProperties();
            uint32_t score = 0;

            if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                score += 1000;
            }

            score += deviceProperties.limits.maxImageDimension2D;
            candidates.insert(std::make_pair(score, device));
        }

        if (candidates.empty())
        {
            throw std::runtime_error("VulkanDevice : Failed to find a suitable physical device!");
        }
        m_physicalDevice = candidates.rbegin()->second;
        Log::setVulkanDevice(&m_physicalDevice);
    }

    void VulkanDevice::initLogicalDevice()
    {
        m_queueFamily = findQueueFamily(m_physicalDevice);

        vk::PhysicalDeviceVulkan11Features features11{};
        features11.shaderDrawParameters = vk::True;

        vk::PhysicalDeviceVulkan12Features features12{};
        features12.drawIndirectCount                            = vk::True;
        features12.descriptorBindingPartiallyBound              = vk::True;
        features12.descriptorBindingVariableDescriptorCount     = vk::True;
        features12.runtimeDescriptorArray                       = vk::True;
        features12.shaderSampledImageArrayNonUniformIndexing    = vk::True;
        features12.descriptorBindingSampledImageUpdateAfterBind = vk::True;
        features12.pNext                                        = &features11;

        vk::PhysicalDeviceVulkan13Features features13{};
        features13.dynamicRendering = vk::True;
        features13.synchronization2 = vk::True;
        features13.pNext            = &features12;

        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicState{};
        extendedDynamicState.extendedDynamicState = vk::True;
        extendedDynamicState.pNext                = &features13;

        vk::PhysicalDeviceFeatures2 features2{};
        features2.features.samplerAnisotropy = vk::True;
        features2.features.geometryShader    = vk::True;
        features2.features.multiDrawIndirect = vk::True;
        features2.pNext                      = &extendedDynamicState;

        constexpr float priority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = m_queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &priority;

        vk::DeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.pNext                   = &features2;
        deviceCreateInfo.queueCreateInfoCount    = 1;
        deviceCreateInfo.pQueueCreateInfos       = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);
        m_queue  = vk::raii::Queue(m_device, m_queueFamily, 0);
    }

    void VulkanDevice::initSwapchain(const SwapchainDesc& desc)
    {
        m_swapchain = std::make_unique<VulkanSwapchain>(m_device, m_physicalDevice, m_surface, desc);
    }

    void VulkanDevice::initFrameContexts()
    {
        m_frames.reserve(MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_frames.push_back(std::make_unique<VulkanFrameContext>(m_device, m_queueFamily));
        }

        m_renderFinishedSemaphores.clear();
        m_renderFinishedSemaphores.reserve(m_swapchain->imageCount());
        for (uint32_t i = 0; i < m_swapchain->imageCount(); ++i)
        {
            m_renderFinishedSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo{});
        }
    }

    bool VulkanDevice::beginFrame()
    {
        VulkanFrameContext& frame = *m_frames[m_frameIndex];
        frame.waitAndReset(m_device);

        auto [result, imageIndex] = m_swapchain->acquireNextImage(frame.presentCompleteSemaphore);

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            return false;
        }

        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("VulkanDevice::beginFrame: acquireNextImage failed: " + vk::to_string(result));
        }

        m_imageIndex = imageIndex;

        frame.commandList->beginRecording();
        return true;
    }

    void VulkanDevice::endFrame()
    {
        VulkanFrameContext& frame = *m_frames[m_frameIndex];
        frame.commandList->endRecording();

        const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo submitInfo{};
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &*frame.presentCompleteSemaphore;
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &*frame.commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &*m_renderFinishedSemaphores[m_imageIndex];

        m_queue.submit(submitInfo, *frame.inFlightFence);

        vk::PresentInfoKHR presentInfo{};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &*m_renderFinishedSemaphores[m_imageIndex];
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &*m_swapchain->raw();
        presentInfo.pImageIndices      = &m_imageIndex;

        const vk::Result presentResult = m_queue.presentKHR(presentInfo);
        if (presentResult != vk::Result::eSuccess && presentResult != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("VulkanDevice::endFrame: presentKHR failed: " + vk::to_string(presentResult));
        }

        m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VulkanDevice::waitIdle()
    {
        m_device.waitIdle();
    }

    void VulkanDevice::recreateSwapchain(uint32_t width, uint32_t height, SwapchainDesc& desc)
    {
        waitIdle();

        m_swapchain.reset();

        desc.width  = width;
        desc.height = height;

        m_swapchain = std::make_unique<VulkanSwapchain>(m_device, m_physicalDevice, m_surface, desc);
        m_renderFinishedSemaphores.clear();
        m_renderFinishedSemaphores.reserve(m_swapchain->imageCount());

        for (uint32_t i = 0; i < m_swapchain->imageCount(); ++i)
        {
            m_renderFinishedSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo{});
        }
    }
} // namespace gcep::rhi::vulkan
