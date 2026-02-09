#include "RHI_Vulkan.hpp"

// STL
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <ranges>
#include <fstream>
#include <filesystem>
#include <sstream>

// Externals
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace gcep
{

static void resizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<RHI_Vulkan*>(glfwGetWindowUserPointer(window));
    app->setFramebufferResized(true);
}

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
    createCommandPool();
    createVertexBuffer();
    createCommandBuffer();
    createSyncObjects();
}

void RHI_Vulkan::drawFrame()
{
    auto fenceResult = m_device.waitForFences(*m_inFlightFences[m_frameIndex], vk::True, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to wait for fence!");
    }

    auto [result, imageIndex] = m_swapChain.acquireNextImage(UINT64_MAX, *m_presentCompleteSemaphores[m_frameIndex], nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        recreateSwapChain();
        return;
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    m_device.resetFences(*m_inFlightFences[m_frameIndex]);

    m_commandBuffers[m_frameIndex].reset();
    recordCommandBuffer(imageIndex);

    vk::PipelineStageFlags waitDestinationStageMask (vk::PipelineStageFlagBits::eColorAttachmentOutput);

    vk::SubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &*m_presentCompleteSemaphores[m_frameIndex];
    submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*m_commandBuffers[m_frameIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &*m_renderFinishedSemaphores[imageIndex];

    m_graphicsQueue.submit(submitInfo, *m_inFlightFences[m_frameIndex]);

    vk::PresentInfoKHR presentInfoKHR{};
    presentInfoKHR.waitSemaphoreCount = 1;
    presentInfoKHR.pWaitSemaphores = &*m_renderFinishedSemaphores[imageIndex];
    presentInfoKHR.swapchainCount = 1;
    presentInfoKHR.pSwapchains = &*m_swapChain;
    presentInfoKHR.pImageIndices = &imageIndex;

    result = m_graphicsQueue.presentKHR(presentInfoKHR);
    if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || m_framebufferResized)
    {
        m_framebufferResized = false;
        recreateSwapChain();
    }
    else
    {
        assert(result == vk::Result::eSuccess && "Hum excuse me wtf");
    }

    m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void RHI_Vulkan::setWindow(GLFWwindow* window)
{
    m_window = window;
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, resizeCallback);
}

void RHI_Vulkan::cleanup()
{
    m_device.waitIdle();

    cleanupSwapChain();
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

    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
    {
        if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
            m_physicalDevice.getSurfaceSupportKHR(qfpIndex, *m_surface))
        {
            m_graphicsIndex = qfpIndex;
            break;
        }
    }

    if (m_graphicsIndex == ~0)
    {
        throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
    }

    // Enable shaderDrawParameters (Vulkan 1.1 feature) required by SPIR-V DrawParameters capability
    vk::PhysicalDeviceVulkan11Features device11Features{};
    device11Features.shaderDrawParameters = vk::True;

    // Query for Vulkan 1.3 features
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT deviceExtendedDynamicStateFeatures{};
    deviceExtendedDynamicStateFeatures.extendedDynamicState = vk::True;
    deviceExtendedDynamicStateFeatures.pNext = &device11Features;

    vk::PhysicalDeviceVulkan13Features device13Features{};
    device13Features.dynamicRendering = vk::True;
    device13Features.synchronization2 = vk::True;
    device13Features.pNext            = &deviceExtendedDynamicStateFeatures;

    vk::PhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.pNext = &device13Features;

    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{};
    deviceQueueCreateInfo.queueFamilyIndex = m_graphicsIndex;
    deviceQueueCreateInfo.queueCount       = 1;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

    vk::DeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.pNext                   = &deviceFeatures2;
    deviceCreateInfo.queueCreateInfoCount    = 1;
    deviceCreateInfo.pQueueCreateInfos       = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledExtensionCount   = m_deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    m_device        = vk::raii::Device(m_physicalDevice, deviceCreateInfo);
    m_graphicsQueue = vk::raii::Queue(m_device, m_graphicsIndex, 0);
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
    auto shaderCode = readShader("Base_Shader.spv");
    vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo {};
    vertexShaderStageInfo.stage     = vk::ShaderStageFlagBits::eVertex;
    vertexShaderStageInfo.module    = shaderModule;
    vertexShaderStageInfo.pName     = "vertMain";

    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo {};
    fragmentShaderStageInfo.stage     = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderStageInfo.module    = shaderModule;
    fragmentShaderStageInfo.pName     = "fragMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageInfo, fragmentShaderStageInfo};

    std::vector dynamicStates =
    {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly {};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.depthClampEnable        = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode             = vk::PolygonMode::eFill;
    rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace               = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable         = vk::False;
    rasterizer.depthBiasSlopeFactor    = 1.0f;
    rasterizer.lineWidth               = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable  = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil {};

    vk::PipelineColorBlendAttachmentState colorBlendAttachment {};
    colorBlendAttachment.blendEnable    = vk::False;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable   = vk::False;
    colorBlending.logicOp         = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount         = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    m_pipelineLayout = vk::raii::PipelineLayout(m_device, pipelineLayoutInfo);

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapChainSurfaceFormat.format;

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pNext               = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = nullptr;

    m_graphicsPipeline = vk::raii::Pipeline(m_device, nullptr, pipelineInfo);
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

void RHI_Vulkan::createCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags         = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = m_graphicsIndex;

    m_commandPool = vk::raii::CommandPool(m_device, poolInfo);
}

void RHI_Vulkan::createVertexBuffer()
{
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size        = sizeof(vertices[0]) * vertices.size();
    bufferInfo.usage    = vk::BufferUsageFlagBits::eVertexBuffer;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    m_vertexBuffer = vk::raii::Buffer(m_device, bufferInfo);

    vk::MemoryRequirements memRequirements = m_vertexBuffer.getMemoryRequirements();

    vk::MemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.allocationSize = memRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    m_vertexBufferMemory = vk::raii::DeviceMemory(m_device, memoryAllocateInfo);
    m_vertexBuffer.bindMemory(*m_vertexBufferMemory, 0);

}

void RHI_Vulkan::createCommandBuffer()
{
    vk::CommandBufferAllocateInfo allocInfo{};

    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    m_commandBuffers = vk::raii::CommandBuffers(m_device, allocInfo);
}


void RHI_Vulkan::transitionImageLayout(uint32_t imageIndex,
                                       vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                       vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
                                       vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask)
{
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel   = 0;
    subresourceRange.levelCount     = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount     = 1;

    vk::ImageMemoryBarrier2 barrier {};
    barrier.srcStageMask        = srcStageMask;
    barrier.srcAccessMask       = srcAccessMask;
    barrier.dstStageMask        = dstStageMask;
    barrier.dstAccessMask       = dstAccessMask;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = m_swapChainImages[imageIndex];
    barrier.subresourceRange    = subresourceRange;

    vk::DependencyFlags dependencyFlags = {};

    vk::DependencyInfo dependencyInfo {};
    dependencyInfo.dependencyFlags         = dependencyFlags;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers    = &barrier;

    m_commandBuffers[m_frameIndex].pipelineBarrier2(dependencyInfo);
}

void RHI_Vulkan::recordCommandBuffer(uint32_t imageIndex)
{
    auto& m_commandBuffer = m_commandBuffers[m_frameIndex];

    m_commandBuffer.begin({});
    transitionImageLayout(imageIndex,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput ,
                          {},
                          vk::AccessFlagBits2::eColorAttachmentWrite
    );

    // TODO: Replace with ImGui ImVec4
    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.33f, 0.46f, 1.0f);
    vk::RenderingAttachmentInfo attachmentInfo{};
    attachmentInfo.imageView    = m_swapChainImageViews[imageIndex];
    attachmentInfo.imageLayout  = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfo.loadOp       = vk::AttachmentLoadOp::eClear;
    attachmentInfo.storeOp      = vk::AttachmentStoreOp::eStore;
    attachmentInfo.clearValue   = clearColor;

    vk::Rect2D renderArea{};
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent   = m_swapChainExtent;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea           = renderArea;
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &attachmentInfo;

    m_commandBuffer.beginRendering(renderingInfo);

    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
    m_commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(m_swapChainExtent.width), static_cast<float>(m_swapChainExtent.height), 0.0f, 1.0f));
    m_commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0,0), m_swapChainExtent));

    m_commandBuffer.draw(3,1,0,0);

    m_commandBuffer.endRendering();

    transitionImageLayout(imageIndex,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {}
    );

    m_commandBuffer.end();
}

void RHI_Vulkan::createSyncObjects()
{
    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        m_renderFinishedSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo());
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_presentCompleteSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo());
        m_inFlightFences.emplace_back(m_device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
    }
}

void RHI_Vulkan::cleanupSwapChain()
{
    m_swapChainImageViews.clear();
    m_swapChain = nullptr;
}

void RHI_Vulkan::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    m_device.waitIdle();

    cleanupSwapChain();
    createSwapChain();
    createImageViews();
}

uint32_t RHI_Vulkan::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
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

ImGui_ImplVulkan_InitInfo RHI_Vulkan::getInitInfo()
{
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance       = *m_instance;
    init_info.PhysicalDevice = *m_physicalDevice;
    init_info.Device         = *m_device;
    init_info.QueueFamily       = m_graphicsIndex;
    init_info.Queue          = *m_graphicsQueue;
    init_info.PipelineCache     = VK_NULL_HANDLE;
    // TODO Set DescriptorPool init_info.DescriptorPool    = VK_NULL_HANDLE;
    init_info.MinImageCount     = 3;
    init_info.ImageCount        = 3;
    init_info.Allocator         = nullptr;
    //init_info.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
    //init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    // TODO init_info.CheckVkResultFn = check_vk_result;

    return init_info;
};

void RHI_Vulkan::setFramebufferResized(bool resized)
{
    m_framebufferResized = resized;
}

} // Namespace gcep