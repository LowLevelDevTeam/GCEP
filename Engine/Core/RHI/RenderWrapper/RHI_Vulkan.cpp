#include "RHI_Vulkan.hpp"

// STL
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <ranges>
#include <fstream>
#include <filesystem>

namespace gcep
{

void RHI_Vulkan::initRHI()
{
    createInstance();
    setupDebugMessenger();
    createWindowSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
}

void RHI_Vulkan::drawTriangle() {}

void RHI_Vulkan::setWindow(GLFWwindow* window)
{
    m_window = window;
}

std::vector<const char*> getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers)
    {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
}

void RHI_Vulkan::createInstance()
{
    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName   = "GC Engine";
    appInfo.applicationVersion = vk::makeVersion(0, 1, 0);
    appInfo.pEngineName        = "GC Engine";
    appInfo.engineVersion      = vk::makeVersion(0, 1, 0);
    appInfo.apiVersion         = vk::ApiVersion14;

    // Get the required layers
    std::vector<char const*> requiredLayers;

    if (enableValidationLayers)
    {
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // Check if the required layers are supported by Vulkan implementation.
    std::vector<vk::LayerProperties> layerProperties = m_context.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(
            requiredLayers,
            [&layerProperties](auto const& requiredLayer)
            {
                return std::ranges::none_of(
                    layerProperties, [requiredLayer](auto const& layerProperty)
                    { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
            }))
    {
        throw std::runtime_error("One or more required layers are not supported!");
    }

    // Get the required extensions.
    auto requiredExtensions = getRequiredExtensions();

    // Check if the required extensions are supported by the Vulkan implementation.
    auto extensionProperties = m_context.enumerateInstanceExtensionProperties();
    for (auto const& requiredExtension : requiredExtensions)
    {
        if (std::ranges::none_of(
                extensionProperties, [requiredExtension](auto const& extensionProperty)
                { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; }))
        {
            throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
        }
    }

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledLayerCount       = requiredLayers.size();
    createInfo.ppEnabledLayerNames     = requiredLayers.data();
    createInfo.enabledExtensionCount   = requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    m_instance = vk::raii::Instance(m_context, createInfo);
}

void RHI_Vulkan::setupDebugMessenger()
{
    if constexpr (!enableValidationLayers)
    {
        return;
    }

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{};
    debugUtilsMessengerCreateInfoEXT.messageSeverity = severityFlags;
    debugUtilsMessengerCreateInfoEXT.messageType     = messageTypeFlags;
    debugUtilsMessengerCreateInfoEXT.pfnUserCallback = &debugCallback;

    m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL RHI_Vulkan::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
{
    std::cerr << "Validation Layer: \n"
              << "Type : " << vk::to_string(type) << " Message : " << pCallbackData->pMessage
              << std::endl;

    return vk::False;
}

void RHI_Vulkan::pickPhysicalDevice()
{
    std::vector<vk::raii::PhysicalDevice> devices = m_instance.enumeratePhysicalDevices();
    if (devices.empty())
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::multimap<int, vk::raii::PhysicalDevice> candidates;
    for (auto& device : devices)
    {
        if (!isSuitable(device))
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
        throw std::runtime_error("Failed to find a suitable physical device!");
    }
    m_physicalDevice = candidates.rbegin()->second;
}

bool RHI_Vulkan::isSuitable(vk::raii::PhysicalDevice device)
{
    auto extensions = device.enumerateDeviceExtensionProperties();
    auto deviceFeatures = device.getFeatures();

    // Check required features :
    if (deviceFeatures.geometryShader == vk::True && deviceFeatures.samplerAnisotropy == vk::True)
    {
        return true;
    }

    return false;
}

uint32_t RHI_Vulkan::findQueueFamilies(vk::raii::PhysicalDevice device)
{
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = device.getQueueFamilyProperties();
    auto graphicsQueueFamilyProperty =
        std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
                     [](vk::QueueFamilyProperties const& qfp)
                     { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });

    if (graphicsQueueFamilyProperty == queueFamilyProperties.end())
    {
        throw std::runtime_error("Failed to find graphics queue family!");
    }

    // Index of graphicsQueueFamilyProperty inside queueFamilyProperties
    return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
}

void RHI_Vulkan::createLogicalDevice()
{
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();

    uint32_t queueIndex = ~0;

    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
    {
        if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
            m_physicalDevice.getSurfaceSupportKHR(qfpIndex, *m_surface))
        {
            queueIndex = qfpIndex;
            break;
        }
    }

    if (queueIndex == ~0)
    {
        throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
    }

    // Query for Vulkan 1.3 features
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT deviceExtendedDynamicStateFeatures{};
    deviceExtendedDynamicStateFeatures.extendedDynamicState = vk::True;

    vk::PhysicalDeviceVulkan13Features device13Features{};
    device13Features.dynamicRendering = vk::True;
    device13Features.pNext            = &deviceExtendedDynamicStateFeatures;

    vk::PhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.pNext = &device13Features;

    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{};
    deviceQueueCreateInfo.queueFamilyIndex = queueIndex;
    deviceQueueCreateInfo.queueCount       = 1;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

    vk::DeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.pNext                   = &deviceFeatures2;
    deviceCreateInfo.queueCreateInfoCount    = 1;
    deviceCreateInfo.pQueueCreateInfos       = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledExtensionCount   = m_deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    m_device        = vk::raii::Device(m_physicalDevice, deviceCreateInfo);
    m_graphicsQueue = vk::raii::Queue(m_device, queueIndex, 0);
}

void RHI_Vulkan::createWindowSurface()
{
    VkSurfaceKHR surface;

    if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface!");
    }

    m_surface = vk::raii::SurfaceKHR(m_instance, surface);
}

vk::SurfaceFormatKHR RHI_Vulkan::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

vk::PresentModeKHR RHI_Vulkan::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));

    // Top choice : Mailbox
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
        {
            return availablePresentMode;
        }
    }

    // Fallback : FIFO (V-Sync)
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D RHI_Vulkan::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    return
    {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}

void RHI_Vulkan::createSwapChain()
{
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(*m_surface);
    m_swapChainSurfaceFormat = chooseSwapSurfaceFormat(m_physicalDevice.getSurfaceFormatsKHR(*m_surface));
    m_swapChainExtent = chooseSwapExtent(surfaceCapabilities);

    uint32_t minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    minImageCount = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount) ? surfaceCapabilities.maxImageCount : minImageCount;

    vk::SwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.flags            = vk::SwapchainCreateFlagsKHR();
    swapChainCreateInfo.surface          = *m_surface;
    swapChainCreateInfo.minImageCount    = minImageCount;
    swapChainCreateInfo.imageFormat      = m_swapChainSurfaceFormat.format;
    swapChainCreateInfo.imageColorSpace  = m_swapChainSurfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent      = m_swapChainExtent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform     = surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode      = chooseSwapPresentMode(m_physicalDevice.getSurfacePresentModesKHR(*m_surface));
    swapChainCreateInfo.clipped          = vk::True;
    swapChainCreateInfo.oldSwapchain     = nullptr;

    m_swapChain       = vk::raii::SwapchainKHR(m_device, swapChainCreateInfo);
    m_swapChainImages = m_swapChain.getImages();
}

void RHI_Vulkan::createImageViews()
{
    m_swapChainImageViews.clear();

    vk::ImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.viewType         = vk::ImageViewType::e2D;
    imageViewCreateInfo.format           = m_swapChainSurfaceFormat.format;
    imageViewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    for (const vk::Image& image : m_swapChainImages)
    {
        imageViewCreateInfo.image = image;
        m_swapChainImageViews.emplace_back(m_device, imageViewCreateInfo);
    }
}


void RHI_Vulkan::createGraphicsPipeline()
{
    auto shaderCode = readShader("slang.spv");
    vk::raii::ShaderModule shaderModule= createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo {};
    vertexShaderStageInfo.stage     = vk::ShaderStageFlagBits::eVertex;
    vertexShaderStageInfo.module = shaderModule;
    vertexShaderStageInfo.pName     = "vertMain";

    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo {};
    fragmentShaderStageInfo.stage     = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderStageInfo.module = shaderModule;
    fragmentShaderStageInfo.pName     = "fragMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageInfo, fragmentShaderStageInfo};
}

std::vector<char> RHI_Vulkan::readShader(const std::string& fileName)
{
    const std::filesystem::path base = std::filesystem::current_path();
    const std::filesystem::path path = base /"Shaders"/ fileName;

    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        std::ostringstream oss;
        oss << "Cannot open file: " << path.string();
        throw std::runtime_error(oss.str());
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));


    std::cout << buffer.size() << std::endl;
    file.close();
    return buffer;
}

vk::raii::ShaderModule RHI_Vulkan::createShaderModule(const std::vector<char>& shaderCode) const
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = shaderCode.size() * sizeof(char);
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(shaderCode.data());

    vk::raii::ShaderModule shaderModule{m_device, createInfo};
    return shaderModule;
}

vk::raii::Context* RHI_Vulkan::getContext()
{
    return &m_context;
}

vk::raii::Instance* RHI_Vulkan::getInstance()
{
    return &m_instance;
}

vk::raii::PhysicalDevice* RHI_Vulkan::getPhysicalDevice()
{
    return &m_physicalDevice;
}

} // Namespace gcep