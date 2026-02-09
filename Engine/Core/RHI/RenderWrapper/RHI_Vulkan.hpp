#pragma once

#include <Engine/Core/RHI/RHI.hpp>

// Externals
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#include <vulkan/vulkan_raii.hpp>

// Forward declaration
struct ImGui_ImplVulkan_InitInfo;

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
    // @brief Vulkan RHI constructor.
    RHI_Vulkan() = default;

    // @brief Initializes the Vulkan renderer.
    void initRHI() override;

    // @brief Draws the hello triangle.
    void drawFrame() override;

    // @brief Sets the native window handle for Vulkan.
    void setWindow(GLFWwindow* window) override;

    // @brief Cleans up the Vulkan renderer.
    void cleanup() override;

public:
    // @brief Get the current Vulkan RHI context.
    [[nodiscard("Context value ignored")]]
    vk::raii::Context* getContext();

    // @brief Get the current Vulkan RHI instance.
    [[nodiscard("Instance value ignored")]]
    vk::raii::Instance* getInstance();

    [[nodiscard("Physical device value ignored")]]
    vk::raii::PhysicalDevice* getPhysicalDevice();

    // @brief Getter allowing the initialization in ImGui Window
    ImGui_ImplVulkan_InitInfo getInitInfo();

    // @brief m_framebufferResized setter.
    void setFramebufferResized(bool resized);

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

    // @brief Creates the command pool for the command buffers
    void createCommandPool();

    // @brief Create vertex buffer
    void createVertexBuffer();

    // @brief Creates the command buffer for recording draw commands.
    void createCommandBuffer();

    // @brief Records the command buffer at a given image index
    void recordCommandBuffer(uint32_t imageIndex);

    // @brief Helper function to transition an image layout using given source and destination masks.
    void transitionImageLayout(uint32_t imageIndex,
        vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
        vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask);

    // @brief Creates the synchronization objects for the draw frame function.
    void createSyncObjects();

    // @brief Cleans up the swap chain on recreation event.
    void cleanupSwapChain();

    // @brief Recreates the swapchain and its image views on resize events.
    void recreateSwapChain();

    // @brief Finds the memory type of the device using a filter.
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

private:
    vk::raii::Context                m_context;
    vk::raii::Instance               m_instance         = nullptr;

    vk::raii::PhysicalDevice         m_physicalDevice   = nullptr;
    vk::raii::Device                 m_device           = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger   = nullptr;
    vk::raii::SurfaceKHR             m_surface          = nullptr;
    vk::raii::Queue                  m_graphicsQueue    = nullptr;
    uint32_t                         m_graphicsIndex    = 0;
    vk::raii::SwapchainKHR           m_swapChain        = nullptr;
    vk::SurfaceFormatKHR             m_swapChainSurfaceFormat;
    vk::Extent2D                     m_swapChainExtent;
    std::vector<vk::Image>           m_swapChainImages;
    std::vector<vk::raii::ImageView> m_swapChainImageViews;
    vk::raii::PipelineLayout         m_pipelineLayout   = nullptr;
    vk::raii::Pipeline               m_graphicsPipeline = nullptr;
    vk::raii::CommandPool            m_commandPool      = nullptr;

    // TODO: Move later
    vk::raii::Buffer m_vertexBuffer                     = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory         = nullptr;

    //
    std::vector<vk::raii::CommandBuffer> m_commandBuffers;
    std::vector<vk::raii::Semaphore> m_presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::raii::Fence> m_inFlightFences;

    bool m_framebufferResized = false;

    uint32_t m_frameIndex = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    std::vector<const char*> m_deviceExtensions = {vk::KHRSwapchainExtensionName};

    GLFWwindow* m_window = nullptr;
};

} // Namespace gcep