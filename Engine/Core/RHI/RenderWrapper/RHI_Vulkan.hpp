#pragma once

#include <Engine/Core/RHI/RHI.hpp>

// Libs
#include <vulkan/vulkan_raii.hpp>

#define XSTR(x) #x
#define STR(x) XSTR(x)

namespace gcep
{

const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif

class RHI_Vulkan final : public RHI
{
public:
    RHI_Vulkan() = default;
    // @brief Vulkan RHI constructor.
    void initRHI() override;

    // @brief Draws the hello triangle.
    void drawTriangle() override;

    void setWindow(GLFWwindow* window) override;

public:
    // @brief Get the current Vulkan RHI context.
    [[nodiscard("Context value ignored")]]
    vk::raii::Context* getContext();

    // @brief Get the current Vulkan RHI instance.
    [[nodiscard("Instance value ignored")]]
    vk::raii::Instance* getInstance();

    [[nodiscard("Physical device value ignored")]]
    vk::raii::PhysicalDevice* getPhysicalDevice();

private:
    // @brief Create a Vulkan instance and check for validation layer support.
    void createInstance();

    // @brief Sets up the debug messenger when the Engine is built in Debug mode.
    void setupDebugMessenger();

    // @brief Callback function for the debug messenger.
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);

    // @brief Binds the physical device.
    void pickPhysicalDevice();

    // @brief Check if the GPU supports all required features.
    bool isSuitable(vk::raii::PhysicalDevice device);

    // @brief Enumerates the queue families on a given physical device.
    uint32_t findQueueFamilies(vk::raii::PhysicalDevice device);

    // @brief Creates the logical device.
    void createLogicalDevice();

    // @brief Create the window surface.
    void createWindowSurface();

    // @brief Creates the swap chain.
    void createSwapChain();
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

    // @brief Creates the swap chain's image views.
    void createImageViews();

    // @brief Create Graphics Pipeline
    void createGraphicsPipeline();
    static std::vector<char> readShader(const std::string& fileName);
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;

private:
    // Keep these two variables declared first - order of destruction is reversed - they need to get destroyed last.

    vk::raii::Context                m_context;
    vk::raii::Instance               m_instance = nullptr;

    vk::raii::PhysicalDevice         m_physicalDevice = nullptr;
    vk::raii::Device                 m_device = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
    vk::raii::SurfaceKHR             m_surface = nullptr;
    vk::raii::Queue                  m_graphicsQueue = nullptr;
    vk::raii::SwapchainKHR           m_swapChain = nullptr;
    vk::SurfaceFormatKHR             m_swapChainSurfaceFormat;
    vk::Extent2D                     m_swapChainExtent;
    std::vector<vk::Image>           m_swapChainImages;
    std::vector<vk::raii::ImageView> m_swapChainImageViews;

    std::vector<const char*> m_deviceExtensions = {vk::KHRSwapchainExtensionName};

    GLFWwindow* m_window = nullptr;
};

} // Namespace gcep