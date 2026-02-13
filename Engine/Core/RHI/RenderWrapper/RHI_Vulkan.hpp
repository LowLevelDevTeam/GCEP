#pragma once

#include <Engine/Core/RHI/RHI.hpp>

// Externals
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#include <vulkan/vulkan_raii.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <imgui.h>

// Forward declaration
struct ImGui_ImplVulkan_InitInfo;

namespace gcep
{

// Tutorial data
struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        return
        {
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord))
        };
    }
};

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif

class RHI_Vulkan final : public RHI
{
public:
    /// @brief Vulkan RHI constructor.
    RHI_Vulkan() = default;

    /// @brief Initializes the Vulkan renderer.
    void initRHI() override;

    /// @brief Draws the hello triangle.
    void drawFrame() override;

    /// @brief Sets the native window handle for Vulkan.
    void setWindow(GLFWwindow* window) override;

    /// @brief Cleans up the Vulkan renderer.
    void cleanup() override;

public:

    // @brief Fonction that init editor HUD variables
    void updateEditorInfo(ImVec4& clearColor);

    /// @brief Get the current Vulkan RHI context.
    [[nodiscard("Context value ignored")]]
    vk::raii::Context* getContext();

    /// @brief Get the current Vulkan RHI instance.
    [[nodiscard("Instance value ignored")]]
    vk::raii::Instance* getInstance();

    [[nodiscard("Physical device value ignored")]]
    vk::raii::PhysicalDevice* getPhysicalDevice();

    /// @brief Getter allowing the initialization in ImGui Window
    ImGui_ImplVulkan_InitInfo getInitInfo();

    /// @brief m_framebufferResized setter.
    void setFramebufferResized(bool resized);

    /// @brief Creates offscreen rendering resources for ImGui viewport
    void createOffscreenResources(uint32_t width, uint32_t height);

    /// @brief Resizes offscreen resources when viewport size changes
    void resizeOffscreenResources(uint32_t width, uint32_t height);

    /// @brief Request a resize of offscreen resources (deferred until safe)
    void requestOffscreenResize(uint32_t width, uint32_t height);

    /// @brief Process pending offscreen resize if any (call at start of frame)
    void processPendingOffscreenResize();

    /// @brief Get the ImGui texture descriptor for the offscreen image
    [[nodiscard]] VkDescriptorSet getImGuiTextureDescriptor() const { return m_imguiTextureDescriptor; }

    /// @brief Get the offscreen extent
    [[nodiscard]] vk::Extent2D getOffscreenExtent() const { return m_offscreenExtent; }

private:
    /// @brief Create a Vulkan instance and check for validation layer support.
    void createInstance();

    /// @brief Sets up the debug messenger when the Engine is built in Debug mode.
    void setupDebugMessenger();

    /// @brief Callback function for the debug messenger.
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);

    /// @brief Binds the physical device.
    void pickPhysicalDevice();

    /// @brief Check if the GPU supports all required features.
    bool isSuitable(vk::raii::PhysicalDevice device);

    /// @brief Enumerates the queue families on a given physical device.
    uint32_t findQueueFamilies(vk::raii::PhysicalDevice device);

    /// @brief Creates the logical device.
    void createLogicalDevice();

    /// @brief Create the window surface.
    void createWindowSurface();

    /// @brief Creates the swap chain.
    void createSwapChain();
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

    /// @brief Creates the swap chain's image views.
    void createImageViews();

    /// @brief Create Graphics Pipeline
    void createGraphicsPipeline();
    static std::vector<char> readShader(const std::string& fileName);
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;

    /// @brief Creates the command pool for the command buffers
    void createCommandPool();

    /// @brief Create vertex buffer
    void createVertexBuffer();

    /// @brief Create index buffer
    void createIndexBuffer();

    /// @brief Creates the command buffer for recording draw commands.
    void createCommandBuffer();

    /// @brief Records the command buffer at a given image index
    void recordCommandBuffer(uint32_t imageIndex);

    /// @brief Helper function to transition an image layout using given source and destination masks.
    void transitionImageLayoutOld(
        vk::Image image,
        vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
        vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
        vk::ImageAspectFlags imageAspectFlags
    );
    void transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

    /// @brief Creates the synchronization objects for the draw frame function.
    void createSyncObjects();

    /// @brief Cleans up the swap chain on recreation event.
    void cleanupSwapChain();

    /// @brief Recreates the swapchain and its image views on resize events.
    void recreateSwapChain();

    /// @brief Finds the memory type of the device using a filter.
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    /// @brief Creates a Vulkan buffer with all the provided arguments.
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);

    /// @brief Copies a Vulkan buffer to another destination buffer
    void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size);

    /// @brief Creates Vulkan uniform buffers
    void createUniformBuffers();

    /// @brief Updates uniform buffers every frame
    void updateUniformBuffer(uint32_t currentImage);

    /// @brief Creates the Vulkan renderer descriptor pool
    void createDescriptorPool();

    /// @brief Creates the Vulkan renderer descriptor sets
    void createDescriptorSets();

    /// @brief Creates the Vulkan renderer descriptor layout(s) (later on)
    void createDescriptorSetLayout();

    /// @brief Creates an image from a texture (PNG / JPEG)
    void createTextureImage();

    /// @brief Creates an image buffer.
    void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory);

    void createTextureImageView();

    void createTextureSampler();

    vk::raii::ImageView createImageView(vk::raii::Image &image, vk::Format format, vk::ImageAspectFlags aspectFlags);

    /// @brief Copies buffer data to image buffer data.
    void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height);

    /// @brief Begins a single time command
    /// @returns A command buffer to execute the single time command.
    std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands();

    /// @brief Ends a single time command for a given command buffer.
    void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer);

    /// @brief Creates test textures depth ressources
    void createDepthResources();

    /// @brief Finds a supported image format given proper parameters
    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

    /// @brief Finds the device supported depth format
    vk::Format findDepthFormat();

    /// @brief Determines if a certain format has a stencil component.
    bool hasStencilComponent(vk::Format format);

    /// @brief Create images for ImGui
    void createImGuiImage();

    /// @brief Records command buffer for offscreen scene rendering
    void recordOffscreenCommandBuffer();

    /// @brief Records command buffer for ImGui UI rendering to swapchain
    void recordImGuiCommandBuffer(uint32_t imageIndex);

    /// @brief Loads the test obj model
    void loadModel();

private:
    vk::raii::Context                m_context;
    vk::raii::Instance               m_instance            = nullptr;

    vk::raii::PhysicalDevice         m_physicalDevice      = nullptr;
    vk::raii::Device                 m_device              = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger      = nullptr;
    vk::raii::SurfaceKHR             m_surface             = nullptr;
    vk::raii::Queue                  m_graphicsQueue       = nullptr;
    uint32_t                         m_graphicsIndex       = 0;
    vk::raii::SwapchainKHR           m_swapChain           = nullptr;
    vk::SurfaceFormatKHR             m_swapChainSurfaceFormat;
    vk::Extent2D                     m_swapChainExtent;
    std::vector<vk::Image>           m_swapChainImages;
    std::vector<vk::raii::ImageView> m_swapChainImageViews;
    vk::raii::DescriptorSetLayout    m_descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout         m_pipelineLayout      = nullptr;
    vk::raii::Pipeline               m_graphicsPipeline    = nullptr;
    vk::raii::CommandPool            m_commandPool         = nullptr;
    vk::raii::DescriptorPool         m_descriptorPool      = nullptr;
    vk::raii::Image                  m_textureImage        = nullptr;
    vk::raii::DeviceMemory           m_textureImageMemory  = nullptr;
    vk::raii::ImageView              m_textureImageView    = nullptr;
    vk::raii::Sampler                m_textureSampler      = nullptr;
    vk::raii::Image                  m_depthImage          = nullptr;
    vk::raii::DeviceMemory           m_depthImageMemory    = nullptr;
    vk::raii::ImageView              m_depthImageView      = nullptr;

    // Offscreen rendering for ImGui viewport
    vk::raii::Image                  m_offscreenImage        = nullptr;
    vk::raii::DeviceMemory           m_offscreenImageMemory  = nullptr;
    vk::raii::ImageView              m_offscreenImageView    = nullptr;
    vk::raii::Sampler                m_offscreenSampler      = nullptr;
    vk::raii::Image                  m_offscreenDepthImage       = nullptr;
    vk::raii::DeviceMemory           m_offscreenDepthImageMemory = nullptr;
    vk::raii::ImageView              m_offscreenDepthImageView   = nullptr;
    VkDescriptorSet                  m_imguiTextureDescriptor    = VK_NULL_HANDLE;
    vk::Extent2D                     m_offscreenExtent           = {800, 600};
    bool                             m_offscreenResizePending    = false;
    uint32_t                         m_pendingOffscreenWidth     = 0;
    uint32_t                         m_pendingOffscreenHeight    = 0;

    ImVec4 m_clearColor;

    // TODO: Move later
    vk::raii::Buffer m_vertexBuffer                        = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory            = nullptr;
    vk::raii::Buffer m_indexBuffer                         = nullptr;
    vk::raii::DeviceMemory m_indexBufferMemory             = nullptr;
    // End of TODO

    // ImGui members
    VkDescriptorPool m_descriptorPoolImGui                 = nullptr;
    std::vector<vk::raii::DescriptorSet> m_dS;
    std::vector<vk::raii::ImageView> m_imageView;
    std::vector<vk::raii::Image> m_image;
    std::vector<vk::raii::DeviceMemory> m_imageMemory;
    std::vector<vk::raii::Sampler> m_sampler;
    std::vector<vk::raii::Buffer> m_uploadBuffer;
    std::vector<vk::raii::DeviceMemory> m_uploadBufferMemory;

    std::vector<vk::raii::Buffer> m_uniformBuffers;
    std::vector<vk::raii::DeviceMemory> m_uniformBuffersMemory;
    std::vector<void*> m_uniformBuffersMapped;
    std::vector<vk::raii::DescriptorSet> m_descriptorSets;
    std::vector<vk::raii::CommandBuffer> m_commandBuffers;
    std::vector<vk::raii::Semaphore> m_presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::raii::Fence> m_inFlightFences;

    std::vector<Vertex>    vertices;
    std::vector<uint32_t>  indices;
    uint32_t               numIndices = 0;

    bool m_framebufferResized = false;

    uint32_t m_frameIndex = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    std::vector<const char*> m_deviceExtensions = {vk::KHRSwapchainExtensionName};

    GLFWwindow* m_window = nullptr;
};

} // Namespace gcep