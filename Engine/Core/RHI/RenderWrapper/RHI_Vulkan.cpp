#include "RHI_Vulkan.hpp"

// STL
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <ranges>
#include <sstream>

// Externals
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Engine/Core/RHI/ObjParser/ObjParser.hpp"

namespace gcep
{

static void resizeCallback(GLFWwindow* window, int width, int height)
{
    auto* app = static_cast<RHI_Vulkan*>(glfwGetWindowUserPointer(window));
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
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthResources();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
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

    updateUniformBuffer(m_frameIndex);
    m_device.resetFences(*m_inFlightFences[m_frameIndex]);

    m_commandBuffers[m_frameIndex].reset();
    recordCommandBuffer(imageIndex);

    vk::PipelineStageFlags waitDestinationStageMask (vk::PipelineStageFlagBits::eColorAttachmentOutput);

    vk::SubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &*m_presentCompleteSemaphores[m_frameIndex];
    submitInfo.pWaitDstStageMask    = &waitDestinationStageMask;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &*m_commandBuffers[m_frameIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &*m_renderFinishedSemaphores[imageIndex];

    m_graphicsQueue.submit(submitInfo, *m_inFlightFences[m_frameIndex]);

    vk::PresentInfoKHR presentInfoKHR{};
    presentInfoKHR.waitSemaphoreCount = 1;
    presentInfoKHR.pWaitSemaphores    = &*m_renderFinishedSemaphores[imageIndex];
    presentInfoKHR.swapchainCount     = 1;
    presentInfoKHR.pSwapchains        = &*m_swapChain;
    presentInfoKHR.pImageIndices      = &imageIndex;

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
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(*m_device, m_descriptorPoolImGui, nullptr);
}

void RHI_Vulkan::updateEditorInfo(ImVec4& clearColor)
{
    m_clearColor = clearColor;
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
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.pEngineName        = "GC Engine";
    appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0);
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
    device13Features.pNext            = &deviceExtendedDynamicStateFeatures;
    device13Features.dynamicRendering = vk::True;
    device13Features.synchronization2 = vk::True;

    vk::PhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.features.samplerAnisotropy = vk::True;
    deviceFeatures2.pNext                      = &device13Features;

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

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo{};
    vertexShaderStageInfo.stage     = vk::ShaderStageFlagBits::eVertex;
    vertexShaderStageInfo.module    = shaderModule;
    vertexShaderStageInfo.pName     = "vertMain";

    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo{};
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

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.depthClampEnable        = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode             = vk::PolygonMode::eFill;
    rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace               = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable         = vk::False;
    rasterizer.depthBiasSlopeFactor    = 1.0f;
    rasterizer.lineWidth               = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable  = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable       = vk::True;
    depthStencil.depthWriteEnable      = vk::True;
    depthStencil.depthCompareOp        = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable     = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable    = vk::False;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable   = vk::False;
    colorBlending.logicOp         = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &*m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    m_pipelineLayout = vk::raii::PipelineLayout(m_device, pipelineLayoutInfo);

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapChainSurfaceFormat.format;
    pipelineRenderingCreateInfo.depthAttachmentFormat = findDepthFormat();

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pNext               = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
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
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    vk::raii::Buffer stagingBuffer             = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;

    createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, vertices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();
    vertices.clear();

    createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_vertexBuffer, m_vertexBufferMemory);

    copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
}

void RHI_Vulkan::createIndexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    vk::raii::Buffer stagingBuffer             = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;
    createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, indices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();
    numIndices = indices.size();
    indices.clear();

    createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_indexBuffer, m_indexBufferMemory);

    copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
}

void RHI_Vulkan::createCommandBuffer()
{
    vk::CommandBufferAllocateInfo allocInfo{};

    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    m_commandBuffers = vk::raii::CommandBuffers(m_device, allocInfo);
}

void RHI_Vulkan::transitionImageLayoutOld(
    vk::Image image,
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
    vk::ImageAspectFlags imageAspectFlags)
{
    vk::ImageMemoryBarrier2 barrier{};
    barrier.srcStageMask        = srcStageMask,
    barrier.srcAccessMask       = srcAccessMask,
    barrier.dstStageMask        = dstStageMask,
    barrier.dstAccessMask       = dstAccessMask,
    barrier.oldLayout           = oldLayout,
    barrier.newLayout           = newLayout,
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    barrier.image               = image,
    barrier.subresourceRange.aspectMask     = imageAspectFlags;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    vk::DependencyInfo dependency_info{};
    dependency_info.dependencyFlags = {};
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers    = &barrier;

    m_commandBuffers[m_frameIndex].pipelineBarrier2(dependency_info);
}

void RHI_Vulkan::transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    auto commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.image               = image;
    barrier.subresourceRange    = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage      = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition!");
    }
    commandBuffer->pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
    endSingleTimeCommands(*commandBuffer);
}

void RHI_Vulkan::recordCommandBuffer(uint32_t imageIndex)
{
    auto& m_commandBuffer = m_commandBuffers[m_frameIndex];

    m_commandBuffer.begin({});

    // Render scene to offscreen image (only if resources are ready)
    bool offscreenReady = (*m_offscreenImage != VK_NULL_HANDLE) &&
                          (*m_offscreenImageView != VK_NULL_HANDLE) &&
                          (*m_offscreenDepthImage != VK_NULL_HANDLE) &&
                          (*m_offscreenDepthImageView != VK_NULL_HANDLE);

    if (offscreenReady)
    {
        recordOffscreenCommandBuffer();
    }

    // Render ImGui
    ImGui::Render();

    // Render ImGui to swapchain
    recordImGuiCommandBuffer(imageIndex);

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
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    m_device.waitIdle();

    cleanupSwapChain();
    createSwapChain();
    createImageViews();
    createDepthResources();
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

void RHI_Vulkan::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory)
{
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = vk::raii::Buffer(m_device, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.allocationSize  = memRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    bufferMemory = vk::raii::DeviceMemory(m_device, memoryAllocateInfo);
    buffer.bindMemory(*bufferMemory, 0);
}

void RHI_Vulkan::copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    vk::raii::CommandBuffer commandCopyBuffer = std::move(m_device.allocateCommandBuffers(allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandCopyBuffer.begin(beginInfo);
    commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
    commandCopyBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &*commandCopyBuffer;

    m_graphicsQueue.submit(submitInfo, nullptr);
    m_graphicsQueue.waitIdle();
}

void RHI_Vulkan::createUniformBuffers()
{
    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();
    m_uniformBuffersMapped.clear();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
        vk::raii::Buffer buffer({});
        vk::raii::DeviceMemory bufferMem({});

        createBuffer(bufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            buffer, bufferMem);

        m_uniformBuffers.emplace_back(std::move(buffer));
        m_uniformBuffersMemory.emplace_back(std::move(bufferMem));
        m_uniformBuffersMapped.emplace_back(m_uniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
}

void RHI_Vulkan::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto  currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime).count();
    float aspectRatio = (m_offscreenExtent.width > 0 && m_offscreenExtent.height > 0)
                        ? static_cast<float>(m_offscreenExtent.width) / static_cast<float>(m_offscreenExtent.height)
                        : 1.0f;

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(0.5f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view  = glm::lookAt(glm::vec3(10.0f, 10.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj  = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    ubo.proj[1][1] *= -1;

    memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void RHI_Vulkan::createDescriptorPool()
{
    std::array poolSize
    {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
    };
    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSize.size());
    poolInfo.pPoolSizes    = poolSize.data();

    m_descriptorPool = vk::raii::DescriptorPool(m_device, poolInfo);
}

void RHI_Vulkan::createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *m_descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool     = *m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts        = layouts.data();

    m_descriptorSets.clear();
    m_descriptorSets = m_device.allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(UniformBufferObject);

        vk::DescriptorImageInfo imageInfo{};
        imageInfo.sampler     = m_textureSampler;
        imageInfo.imageView   = m_textureImageView;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::WriteDescriptorSet uboSet{};
        uboSet.dstSet          = m_descriptorSets[i];
        uboSet.dstBinding      = 0;
        uboSet.dstArrayElement = 0;
        uboSet.descriptorCount = 1;
        uboSet.descriptorType  = vk::DescriptorType::eUniformBuffer;
        uboSet.pBufferInfo     = &bufferInfo;
        vk::WriteDescriptorSet textureSet{};
        textureSet.dstSet          = m_descriptorSets[i];
        textureSet.dstBinding      = 1;
        textureSet.dstArrayElement = 0;
        textureSet.descriptorCount = 1;
        textureSet.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
        textureSet.pImageInfo      = &imageInfo;

        std::array descriptorWrites
        {
            uboSet,
            textureSet
        };

        m_device.updateDescriptorSets(descriptorWrites, {});
    }
}

void RHI_Vulkan::createDescriptorSetLayout()
{
    std::array bindings = {
        vk::DescriptorSetLayoutBinding( 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
        vk::DescriptorSetLayoutBinding( 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());

    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(m_device, layoutInfo);
}

void RHI_Vulkan::createTextureImage()
{
    int            texWidth, texHeight, texChannels;
    stbi_uc       *pixels    = stbi_load("TestTextures/black.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels)
    {
        throw std::runtime_error("Failed to load texture image!");
    }

    vk::raii::Buffer       stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void *data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, m_textureImage, m_textureImageMemory);

    transitionImageLayout(m_textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, m_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    transitionImageLayout(m_textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void RHI_Vulkan::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory)
{
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType     = vk::ImageType::e2D;
    imageInfo.format        = format;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = vk::SampleCountFlagBits::e1;
    imageInfo.tiling        = tiling;
    imageInfo.usage         = usage;
    imageInfo.sharingMode   = vk::SharingMode::eExclusive;

    image = vk::raii::Image(m_device, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    imageMemory = vk::raii::DeviceMemory(m_device, allocInfo);
    image.bindMemory(imageMemory, 0);
}

void RHI_Vulkan::createTextureImageView()
{
    m_textureImageView = createImageView(m_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

void RHI_Vulkan::createTextureSampler()
{
    vk::PhysicalDeviceProperties properties = m_physicalDevice.getProperties();

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter        = vk::Filter::eLinear;
    samplerInfo.minFilter        = vk::Filter::eLinear;
    samplerInfo.mipmapMode       = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.anisotropyEnable = vk::True;
    samplerInfo.maxAnisotropy    = properties.limits.maxSamplerAnisotropy;
    samplerInfo.compareEnable    = vk::False;
    samplerInfo.compareOp        = vk::CompareOp::eAlways;

    m_textureSampler = vk::raii::Sampler(m_device, samplerInfo);
}

vk::raii::ImageView RHI_Vulkan::createImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image            = *image;
    viewInfo.viewType         = vk::ImageViewType::e2D;
    viewInfo.format           = format;
    viewInfo.subresourceRange = {aspectFlags, 0, 1, 0, 1};

    return vk::raii::ImageView(m_device, viewInfo);
}

void RHI_Vulkan::copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height)
{
    std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = beginSingleTimeCommands();

    vk::BufferImageCopy region{};
    region.bufferOffset        = 0;
    region.bufferRowLength     = 0;
    region.bufferImageHeight   = 0;
    region.imageSubresource    = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.imageOffset.x       = 0;
    region.imageOffset.y       = 0;
    region.imageOffset.z       = 0;
    region.imageExtent.width   = width;
    region.imageExtent.height  = height;
    region.imageExtent.depth   = 1;

    commandBuffer->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
    endSingleTimeCommands(*commandBuffer);
}

std::unique_ptr<vk::raii::CommandBuffer> RHI_Vulkan::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;
    std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = std::make_unique<vk::raii::CommandBuffer>(std::move(vk::raii::CommandBuffers(m_device, allocInfo).front()));

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    commandBuffer->begin(beginInfo);

    return commandBuffer;
}

void RHI_Vulkan::endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer)
{
    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &*commandBuffer;

    m_graphicsQueue.submit(submitInfo, nullptr);
    m_graphicsQueue.waitIdle();
}

void RHI_Vulkan::createDepthResources()
{
    vk::Format depthFormat = findDepthFormat();

    createImage(
        m_swapChainExtent.width, m_swapChainExtent.height,
        depthFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_depthImage, m_depthImageMemory
    );

    m_depthImageView = createImageView(m_depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

vk::Format RHI_Vulkan::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
    for (const auto format : candidates)
    {
        vk::FormatProperties props = m_physicalDevice.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format!");
}

vk::Format RHI_Vulkan::findDepthFormat()
{
    return findSupportedFormat
    (
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

bool RHI_Vulkan::hasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

void RHI_Vulkan::loadModel()
{
    std::filesystem::path filepath = "TestTextures/bugatti.obj";
    auto [attrib, modelIndices] = objParser::loadObj(filepath);

    vertices.resize(modelIndices.size());
    indices.resize(modelIndices.size());

    const float* verts = attrib.vertices.data();
    const float* texcoords = attrib.texcoords.data();
    const float* normals = attrib.normals.data();

    for (size_t i = 0; i < modelIndices.size(); ++i)
    {
        const auto& index = modelIndices[i];

        const float* v = verts + 3 * index.vertex_index;
        vertices[i].pos = {v[0], v[1], v[2]};

        if (index.texcoord_index != UINT32_MAX)
        {
            const float* tc = texcoords + 2 * index.texcoord_index;
            vertices[i].texCoord = {tc[0], 1.0f - tc[1]};
        }
        else
        {
            vertices[i].texCoord = {0.0f, 0.0f};
        }

        if (index.normal_index != UINT32_MAX)
        {
            const float* n = normals + 3 * index.normal_index;
            vertices[i].color = {n[0], n[1], n[2]};
        }
        else
        {
            vertices[i].color = {1.0f, 1.0f, 1.0f};
        }

        indices[i] = static_cast<uint32_t>(i);
    }
}

void RHI_Vulkan::createImGuiImage()
{
    createImage(
        m_swapChainExtent.width, m_swapChainExtent.height,
        vk::Format::eR8G8B8A8Unorm,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_image[m_frameIndex], m_imageMemory[m_frameIndex]
    );
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
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    auto result = vkCreateDescriptorPool(*m_device, &pool_info, nullptr, &m_descriptorPoolImGui);

    if (result != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool! (ImGui failure)");

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance          = *m_instance;
    init_info.PhysicalDevice    = *m_physicalDevice;
    init_info.Device            = *m_device;
    init_info.QueueFamily       = m_graphicsIndex;
    init_info.Queue             = *m_graphicsQueue;
    init_info.DescriptorPool    = m_descriptorPoolImGui;
    init_info.PipelineCache     = VK_NULL_HANDLE;
    init_info.MinImageCount     = 3;
    init_info.ImageCount        = 3;
    init_info.Allocator         = nullptr;
    init_info.UseDynamicRendering = true;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = reinterpret_cast<VkFormat*>(&m_swapChainSurfaceFormat.format);
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = static_cast<VkFormat>(findDepthFormat());
    //init_info.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
    //init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    return init_info;
}

void RHI_Vulkan::setFramebufferResized(bool resized)
{
    m_framebufferResized = resized;
}

void RHI_Vulkan::createOffscreenResources(uint32_t width, uint32_t height)
{
    m_offscreenExtent.width = width;
    m_offscreenExtent.height = height;

    // Create offscreen color image
    createImage(
        width, height,
        m_swapChainSurfaceFormat.format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_offscreenImage, m_offscreenImageMemory
    );

    m_offscreenImageView = createImageView(m_offscreenImage, m_swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor);

    // Create offscreen depth image
    vk::Format depthFormat = findDepthFormat();
    createImage(
        width, height,
        depthFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_offscreenDepthImage, m_offscreenDepthImageMemory
    );
    m_offscreenDepthImageView = createImageView(m_offscreenDepthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);

    // Create sampler for the offscreen image
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter     = vk::Filter::eLinear;
    samplerInfo.minFilter     = vk::Filter::eLinear;
    samplerInfo.mipmapMode    = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU  = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV  = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW  = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.mipLodBias    = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod        = 0.0f;
    samplerInfo.maxLod        = 1.0f;

    m_offscreenSampler = vk::raii::Sampler(m_device, samplerInfo);

    // Transition offscreen image to shader read layout for initial state
    std::unique_ptr<vk::raii::CommandBuffer> cmdBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier2 barrier{};
    barrier.srcStageMask  = vk::PipelineStageFlagBits2::eTopOfPipe;
    barrier.srcAccessMask = vk::AccessFlagBits2::eNone;
    barrier.dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
    barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    barrier.oldLayout     = vk::ImageLayout::eUndefined;
    barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.image         = *m_offscreenImage;
    barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    vk::DependencyInfo depInfo{};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers    = &barrier;
    cmdBuffer->pipelineBarrier2(depInfo);

    endSingleTimeCommands(*cmdBuffer);

    // Create ImGui texture descriptor
    m_imguiTextureDescriptor = ImGui_ImplVulkan_AddTexture(
        *m_offscreenSampler,
        *m_offscreenImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void RHI_Vulkan::resizeOffscreenResources(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return;
    if (width == m_offscreenExtent.width && height == m_offscreenExtent.height) return;

    m_device.waitIdle();

    // Remove old ImGui descriptor
    if (m_imguiTextureDescriptor != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(m_imguiTextureDescriptor);
        m_imguiTextureDescriptor = VK_NULL_HANDLE;
    }

    // Clear old resources
    m_offscreenImageView = nullptr;
    m_offscreenImage = nullptr;
    m_offscreenImageMemory = nullptr;
    m_offscreenSampler = nullptr;
    m_offscreenDepthImageView = nullptr;
    m_offscreenDepthImage = nullptr;
    m_offscreenDepthImageMemory = nullptr;

    // Recreate with new size
    createOffscreenResources(width, height);
}

void RHI_Vulkan::requestOffscreenResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return;
    if (width == m_offscreenExtent.width && height == m_offscreenExtent.height) return;

    m_offscreenResizePending = true;
    m_pendingOffscreenWidth = width;
    m_pendingOffscreenHeight = height;
}

void RHI_Vulkan::processPendingOffscreenResize()
{
    if (m_offscreenResizePending)
    {
        m_offscreenResizePending = false;
        resizeOffscreenResources(m_pendingOffscreenWidth, m_pendingOffscreenHeight);
    }
}

void RHI_Vulkan::recordOffscreenCommandBuffer()
{
    // Verify descriptor sets are valid
    if (m_descriptorSets.empty())
    {
        std::cerr << "ERROR: m_descriptorSets is empty!" << std::endl;
        return;
    }

    if (m_frameIndex >= m_descriptorSets.size())
    {
        std::cerr << "ERROR: m_frameIndex (" << m_frameIndex << ") >= m_descriptorSets.size() (" << m_descriptorSets.size() << ")" << std::endl;
        return;
    }

    // Also verify the descriptor set handle is valid
    VkDescriptorSet ds = *m_descriptorSets[m_frameIndex];
    if (ds == VK_NULL_HANDLE)
    {
        std::cerr << "ERROR: descriptor set at frame " << m_frameIndex << " is VK_NULL_HANDLE!" << std::endl;
        return;
    }

    vk::raii::CommandBuffer& m_commandBuffer = m_commandBuffers[m_frameIndex];

    // Transition offscreen image to color attachment
    vk::ImageMemoryBarrier2 toColorAttachment{};
    toColorAttachment.srcStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
    toColorAttachment.srcAccessMask = vk::AccessFlagBits2::eShaderRead;
    toColorAttachment.dstStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    toColorAttachment.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    toColorAttachment.oldLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    toColorAttachment.newLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    toColorAttachment.image         = *m_offscreenImage;
    toColorAttachment.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    vk::ImageMemoryBarrier2 depthBarrier{};
    depthBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    depthBarrier.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    depthBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    depthBarrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    depthBarrier.oldLayout     = vk::ImageLayout::eUndefined;
    depthBarrier.newLayout     = vk::ImageLayout::eDepthAttachmentOptimal;
    depthBarrier.image         = *m_offscreenDepthImage;
    depthBarrier.subresourceRange = {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1};

    std::array<vk::ImageMemoryBarrier2, 2> barriers = {toColorAttachment, depthBarrier};
    vk::DependencyInfo depInfo{};
    depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    depInfo.pImageMemoryBarriers    = barriers.data();
    m_commandBuffer.pipelineBarrier2(depInfo);

    // Setup rendering to offscreen image
    vk::ClearValue clearColor = vk::ClearColorValue(m_clearColor.x,m_clearColor.y, m_clearColor.z, m_clearColor.w);
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.imageView   = m_offscreenImageView;
    colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachmentInfo.clearValue  = clearColor;

    vk::RenderingAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.imageView   = m_offscreenDepthImageView;
    depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    depthAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eDontCare;
    depthAttachmentInfo.clearValue  = clearDepth;

    vk::Rect2D renderArea{};
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent = m_offscreenExtent;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea           = renderArea;
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment     = &depthAttachmentInfo;

    m_commandBuffer.beginRendering(renderingInfo);

    // Render scene
    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
    m_commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f,
        static_cast<float>(m_offscreenExtent.width),
        static_cast<float>(m_offscreenExtent.height), 0.0f, 1.0f));
    m_commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_offscreenExtent));

    m_commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
    m_commandBuffer.bindIndexBuffer(*m_indexBuffer, 0, vk::IndexType::eUint32);

    // Get the descriptor set - use the raii wrapper directly which converts to vk::DescriptorSet
    const vk::raii::DescriptorSet& dsToUse = m_descriptorSets[m_frameIndex];

    m_commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, *dsToUse, nullptr);
    m_commandBuffer.drawIndexed(numIndices, 1, 0, 0, 0);

    m_commandBuffer.endRendering();

    // Transition offscreen image back to shader read
    vk::ImageMemoryBarrier2 toShaderRead{};
    toShaderRead.srcStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    toShaderRead.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    toShaderRead.dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
    toShaderRead.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    toShaderRead.oldLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    toShaderRead.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    toShaderRead.image         = *m_offscreenImage;
    toShaderRead.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    vk::DependencyInfo depInfo2{};
    depInfo2.imageMemoryBarrierCount = 1;
    depInfo2.pImageMemoryBarriers    = &toShaderRead;
    m_commandBuffer.pipelineBarrier2(depInfo2);
}

void RHI_Vulkan::recordImGuiCommandBuffer(uint32_t imageIndex)
{
    vk::raii::CommandBuffer& m_commandBuffer = m_commandBuffers[m_frameIndex];

    // Transition swapchain image to color attachment
    transitionImageLayoutOld(
        m_swapChainImages[imageIndex],
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits2::eNone, vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor
    );

    vk::ClearValue clearColor = vk::ClearColorValue(m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w);

    vk::RenderingAttachmentInfo attachmentInfo{};
    attachmentInfo.imageView   = m_swapChainImageViews[imageIndex];
    attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    attachmentInfo.storeOp     = vk::AttachmentStoreOp::eStore;
    attachmentInfo.clearValue  = clearColor;

    vk::Rect2D renderArea{};
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent = m_swapChainExtent;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea           = renderArea;
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &attachmentInfo;

    m_commandBuffer.beginRendering(renderingInfo);

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData != nullptr)
    {
        ImGui_ImplVulkan_RenderDrawData(drawData, *m_commandBuffer);
    }

    m_commandBuffer.endRendering();

    // Handle ImGui viewports
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    // Transition swapchain image to present
    transitionImageLayoutOld(
        m_swapChainImages[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor
    );
}

} // Namespace gcep