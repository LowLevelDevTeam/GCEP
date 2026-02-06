#pragma once

#include "../RHI.hpp"

// Libs
#include <vulkan/vulkan_raii.hpp>

#define XSTR(x) #x
#define STR(x) XSTR(x)

namespace gcep {

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif

class RHI_Vulkan final : public RHI {
public:
    RHI_Vulkan();
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
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);

    // @brief Binds the physical device.
    void pickPhysicalDevice();

    // @brief Check if the GPU support all required features
    bool isSuitable(vk::raii::PhysicalDevice device);

    // @brief Enumerates the queue families on a given physical device.
    uint32_t findQueueFamilies(vk::raii::PhysicalDevice device);

    // @brief Creates the logical device.
    void createLogicalDevice();

    // @brief Create Window surface
    void createWindowSurface();

private:
    vk::raii::Context m_context;
    vk::raii::Instance m_instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
    vk::raii::PhysicalDevice m_physicalDevice = nullptr;
    std::vector<const char*> m_deviceExtensions = {vk::KHRSwapchainExtensionName};
    vk::raii::Device m_device = nullptr;
    vk::raii::SurfaceKHR m_surface = nullptr;
    vk::raii::Queue m_graphicsQueue = nullptr;
    GLFWwindow* m_window = nullptr;
};

} // Namespace gcep