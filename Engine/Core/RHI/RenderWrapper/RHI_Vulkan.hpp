#pragma once

#include "../RHI.hpp"
#include <vulkan/vulkan_raii.hpp>

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

//#define XSTR(x) #x
//#define STR(x) XSTR(x)

//#define EditorDir ../../../../Editor
//#define File Window/Window.hpp

class RHI_Vulkan final : public RHI {
public:
    // @brief Vulkan RHI constructor.
    void initRHI() override;

    // @brief Draws the hello triangle.
    //void drawTriangle() override;

public:
    // @brief Get the current Vulkan RHI context.
    vk::raii::Context* getContext();

    // @brief Get the current Vulkan RHI instance.
    vk::raii::Instance* getInstance();

private:
    // @brief Create a Vulkan instance and check for validation layer support.
    void createInstance();

    // @brief Sets up the debug messenger when the Engine is built in Debug mode.
    void setupDebugMessenger();

    // @brief Callback function for the debug messenger.
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);

    // @brief Bind the physical device.
    void createDevice();
    void createGraphicsPipeline();
    void createCommandBuffers();
    void createSwapChain();

private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
};