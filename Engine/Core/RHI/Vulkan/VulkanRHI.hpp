#pragma once

#include <RHI/RHI.hpp>
#include <RHI/Vulkan/VulkanDevice.hpp>
#include <RHI/Vulkan/VulkanMesh.hpp>
#include <RHI/Vulkan/VulkanTexture.hpp>

// Externals
#include <vulkan/vulkan_raii.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

// STL
#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gcep::rhi::vulkan
{

// Vertex / UBO types

/// @brief Interleaved vertex layout: position (vec3), vertex color or normal (vec3),
///        texture coordinate (vec2). Matches the SPIR-V input layout of @c Base_Shader.spv.
struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    /// @brief Returns the binding description for a single interleaved vertex buffer
    ///        bound at binding point 0 with per-vertex input rate.
    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }

    /// @brief Returns the attribute descriptions for @c pos (location 0),
    ///        @c color (location 1), and @c texCoord (location 2).
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        return
        {{
            { 0, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, pos))      },
            { 1, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, color))    },
            { 2, 0, vk::Format::eR32G32Sfloat,    static_cast<uint32_t>(offsetof(Vertex, texCoord)) },
        }};
    }

    /// @brief Equality comparison used by the deduplication hash map in @c loadModel().
    bool operator==(const Vertex& other) const noexcept
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

/// @brief Uniform buffer data uploaded once per frame to each UBO slot.
struct UniformBufferObject
{
    glm::mat4 model; ///< Model-to-world transform (rotates over time).
    glm::mat4 view;  ///< World-to-camera (fixed look-at from (2,2,2)).
    glm::mat4 proj;  ///< Perspective projection (45° FOV, Y-flipped for Vulkan NDC).
};

/// @brief Top-level Vulkan renderer implementing the @c gcep::RHI interface.
///
/// @c Vulkan_RHI owns:
///   - @c VulkanDevice - device, swapchain, and per-frame contexts.
///   - Scene resources - mesh (vertex + index buffers), viking-room texture with mipmaps,
///     UBO, descriptor set layout / pool / sets, and the graphics pipeline.
///   - Offscreen render target - a separate color + depth image rendered into by the
///     scene pass and displayed inside an ImGui viewport via @c ImGui_ImplVulkan_AddTexture.
///   - ImGui integration - its own @c VkDescriptorPool, GLFW + Vulkan backends.
///
/// @par Frame sequence (drawFrame)
///   @code
///   processPendingOffscreenResize()   // deferred to avoid mid-frame resize hazards
///   // handle deferred swapchain resize from GLFW callback
///   beginFrame()                      // fence wait + acquire
///   updateUniformBuffer()             // CPU-side UBO write
///   recordOffscreenCommandBuffer()    // scene -> offscreen image
///   ImGui::Render()
///   recordImGuiCommandBuffer()        // ImGui draw data -> swapchain image
///   endFrame()                        // submit + present
///   @endcode
///
/// @par Thread safety
///   All public methods must be called from the main/render thread.
class VulkanRHI final : public gcep::RHI
{
    friend class VulkanTexture;
    friend class VulkanMesh;
public:
    /// @brief Constructs the RHI object and stores the swapchain creation parameters.
    ///
    /// No Vulkan objects are created here. Call @c setWindow() then @c initRHI()
    /// before using any other method.
    ///
    /// @param desc  Swapchain parameters (width, height, vsync). The
    ///              @c nativeWindowHandle field is populated by @c setWindow().
    explicit VulkanRHI(const gcep::rhi::SwapchainDesc& desc);

    ~VulkanRHI() override = default;

    VulkanRHI(const VulkanRHI&)            = delete;
    VulkanRHI& operator=(const VulkanRHI&) = delete;
    VulkanRHI(VulkanRHI&&)                 = delete;
    VulkanRHI& operator=(VulkanRHI&&)      = delete;

    // gcep::RHI

    /// @brief Stores the window handle and registers the GLFW framebuffer-resize callback.
    ///
    /// Must be called before @c initRHI(). Stores @p window into @c m_swapchainDesc
    /// and calls @c glfwSetWindowUserPointer / @c glfwSetFramebufferSizeCallback so
    /// that resize events set @c m_framebufferResized, which @c drawFrame() checks
    /// at the top of every frame.
    ///
    /// @param window  The application's GLFW window. Must remain valid for the
    ///                lifetime of this object.
    void setWindow(GLFWwindow* window) override;

    /// @brief Initialises all Vulkan objects and scene resources.
    ///
    /// Call order:
    ///   1. @c VulkanDevice::createVulkanDevice() - device + swapchain.
    ///   2. Creates the upload command pool (persistent, used for single-time submits).
    ///   3. @c initSceneResources() - descriptor layout, pipeline, texture, mesh,
    ///      UBO, descriptor pool + sets.
    ///
    /// @throws std::runtime_error if any init step fails.
    void initRHI() override;

    /// @brief Records and submits one frame.
    ///
    /// See the class-level frame sequence for the full call order.
    /// Silently returns early if @c beginFrame() signals an out-of-date swapchain
    /// or if a swapchain / offscreen resize is serviced on this tick.
    void drawFrame() override;

    /// @brief Waits for the GPU to be idle, shuts down ImGui, and frees the ImGui pool.
    ///
    /// Calls @c m_device.waitIdle(), removes any registered ImGui texture descriptor,
    /// calls @c ImGui_ImplVulkan_Shutdown / @c ImGui_ImplGlfw_Shutdown /
    /// @c ImGui::DestroyContext, and destroys @c m_descriptorPoolImGui.
    void cleanup() override;

    // Editor surface

    /// @brief Sets the framebuffer-resized flag.
    ///
    /// Called from the GLFW @c framebufferResizeCallback. The flag is consumed at
    /// the top of the next @c drawFrame() call, which then calls @c recreateSwapchain().
    ///
    /// @param resized  Pass @c true when the framebuffer size has changed.
    void setFramebufferResized(bool resized) noexcept { m_framebufferResized = resized; }

    /// @brief Updates the scene clear color used by the offscreen render pass.
    ///
    /// Called by the editor each frame to push the ImGui color picker value into the
    /// renderer. The value is consumed by @c recordOffscreenCommandBuffer() and
    /// @c recordImGuiCommandBuffer() via @c m_clearColor.
    ///
    /// @param clearColor  RGBA clear color in linear [0, 1] float space.
    void updateEditorInfo(ImVec4& clearColor);

    /// @brief Defers an offscreen render target resize to the next frame boundary.
    ///
    /// Safe to call at any time, including while a frame is being recorded.
    /// If @p width x @p height matches the current extent, the request is ignored.
    /// The resize is applied by @c processPendingOffscreenResize() at the top of
    /// the next @c drawFrame() call, after the previous frame has been presented.
    ///
    /// @param width   New offscreen render target width in pixels. Must be > 0.
    /// @param height  New offscreen render target height in pixels. Must be > 0.
    void requestOffscreenResize(uint32_t width, uint32_t height);

    /// @brief Returns the ImGui texture descriptor for the offscreen color image.
    ///
    /// Pass this to @c ImGui::Image() to display the scene inside an ImGui viewport.
    /// Valid after @c createOffscreenResources() and until the next
    /// @c resizeOffscreenResources() call, which removes and re-registers the descriptor.
    ///
    /// @returns A @c VkDescriptorSet registered with @c ImGui_ImplVulkan_AddTexture,
    ///          or @c VK_NULL_HANDLE if offscreen resources have not been created yet.
    [[nodiscard]] VkDescriptorSet getImGuiTextureDescriptor() const noexcept { return m_imguiTextureDescriptor; }

    /// @brief Returns a mutable reference to the underlying @c VulkanDevice.
    [[nodiscard]] VulkanDevice& device() noexcept { return m_device; }

    /// @brief Returns a const reference to the underlying @c VulkanDevice.
    [[nodiscard]] const VulkanDevice& device() const noexcept { return m_device; }

public:
    /// @brief Builds and returns the @c ImGui_ImplVulkan_InitInfo for this device.
    ///
    /// Creates the ImGui-owned @c VkDescriptorPool (@c m_descriptorPoolImGui),
    /// fills the init info with instance / device / queue handles, configures
    /// @c UseDynamicRendering, sets the swapchain color format, queries the depth
    /// format, and sets @c MSAASamples to 1.
    ///
    /// @returns A fully populated @c ImGui_ImplVulkan_InitInfo ready to pass to
    ///          @c ImGui_ImplVulkan_Init().
    /// @throws std::runtime_error if @c vkCreateDescriptorPool fails.
    ImGui_ImplVulkan_InitInfo getInitInfo();

    /// @brief Applies a pending offscreen resize if one was requested via
    ///        @c requestOffscreenResize().
    ///
    /// Clears the pending flag then calls @c resizeOffscreenResources(). Called at the
    /// very start of @c drawFrame() so the resize happens between frames, never
    /// mid-recording.
    void processPendingOffscreenResize();

private:
    // Init helpers

    /// @brief Calls all scene-resource init functions in dependency order.
    ///
    /// Order: @c createDescriptorSetLayout -> @c createGraphicsPipeline ->
    /// @c createTextureImage -> @c createTextureImageView -> @c createTextureSampler ->
    /// @c loadModel -> @c createVertexBuffer -> @c createIndexBuffer ->
    /// @c createUniformBuffers -> @c createDescriptorPool -> @c createDescriptorSets.
    void initSceneResources();

    /// @brief Recreates the swapchain in response to a resize event.
    ///
    /// Spins on @c glfwGetFramebufferSize until the window is non-zero
    /// (handles minimization), then delegates to @c VulkanDevice::recreateSwapchain().
    void recreateSwapchain();

    // UBO + Descriptors

    /// @brief Creates one host-visible, persistently-mapped UBO per frame slot.
    ///
    /// Stores buffers in @c m_uniformBuffers, memory in @c m_uniformBuffersMemory,
    /// and mapped pointers in @c m_uniformBuffersMapped.
    void createUniformBuffers();

    /// @brief Creates the descriptor set layout with two bindings:
    ///        binding 0 - uniform buffer (vertex stage),
    ///        binding 1 - combined image sampler (fragment stage).
    void createUBODescriptorSetLayout();

    /// @brief Creates the scene @c VkDescriptorPool sized for @c MAX_FRAMES_IN_FLIGHT sets,
    ///        each containing one uniform buffer and one combined image sampler.
    void createDescriptorPool();

    /// @brief Allocates @c MAX_FRAMES_IN_FLIGHT descriptor sets and writes the UBO
    ///        and texture bindings into each one.
    void createUBODescriptorSets();

    /// @brief Rewrites the descriptor set bindings for all in-flight frames.
    ///
    /// Updates binding 0 (uniform buffer) and binding 1 (combined image sampler)
    /// for each entry in @c m_descriptorSets to reflect the current state of
    /// @c m_uniformBuffers and @c texture. Must be called after any resource that
    /// is referenced by the descriptor sets is replaced at runtime (e.g. after
    /// swapping the active texture or recreating uniform buffers).
    ///
    /// @note Descriptor sets must already have been allocated via
    ///       @c createDescriptorSets() before calling this function.
    void updateUBODescriptorSets();

    // Pipeline

    /// @brief Compiles and links the graphics pipeline for the offscreen scene pass.
    ///
    /// Reads @c Base_Shader.spv from the @c Shaders/ directory (single SPIR-V binary
    /// with entry points @c vertMain and @c fragMain). Enables depth test/write,
    /// back-face culling, counter-clockwise front faces, and dynamic viewport/scissor.
    /// Configured for dynamic rendering targeting the offscreen color + depth format.
    ///
    /// @throws std::runtime_error if the shader file cannot be read.
    void createGraphicsPipeline();

    // Offscreen resources

    /// @brief Creates the offscreen color image, depth image, sampler, and
    ///        registers the color image with ImGui via @c ImGui_ImplVulkan_AddTexture.
    ///
    /// The color image is immediately transitioned to @c eShaderReadOnlyOptimal
    /// so it can be sampled by ImGui before the first scene frame is rendered.
    ///
    /// @param width   Render target width in pixels.
    /// @param height  Render target height in pixels.
    void createOffscreenResources(uint32_t width, uint32_t height);

    /// @brief Waits for the GPU, destroys all offscreen resources, and recreates them
    ///        at the new resolution. Also removes and re-registers the ImGui descriptor.
    ///
    /// @param width   New render target width in pixels.
    /// @param height  New render target height in pixels.
    void resizeOffscreenResources(uint32_t width, uint32_t height);

    // Per-frame recording

    /// @brief Writes the current model-view-projection matrices into all UBO slots.
    ///
    /// The model matrix rotates at 45°/s around Z using @c m_startTime as epoch.
    /// The view is a fixed look-at from (2,2,2) to the origin.
    /// The projection uses a 45° vertical FOV with the Y axis flipped for Vulkan NDC.
    /// Uses the offscreen extent's aspect ratio; falls back to 1.0 if extent is zero.
    void updateUniformBuffer();

    /// @brief Records the scene pass into the current frame's command buffer.
    ///
    /// Transitions the offscreen color image (@c eShaderReadOnlyOptimal ->
    /// @c eColorAttachmentOptimal) and depth image (@c eUndefined ->
    /// @c eDepthAttachmentOptimal), begins a dynamic render pass, binds the pipeline,
    /// viewport, scissor, vertex buffer, index buffer, and descriptor set, then issues
    /// @c drawIndexed. Transitions color back to @c eShaderReadOnlyOptimal after
    /// @c endRendering so ImGui can sample it.
    ///
    /// No-op if any offscreen resource handle is null.
    void recordOffscreenCommandBuffer();

    /// @brief Records the ImGui pass into the current frame's command buffer,
    ///        targeting the acquired swapchain image.
    ///
    /// Transitions the swapchain image (@c eUndefined -> @c eColorAttachmentOptimal),
    /// begins a dynamic render pass, calls @c ImGui_ImplVulkan_RenderDrawData,
    /// ends the pass, handles multi-viewport platform windows, then transitions the
    /// swapchain image to @c ePresentSrcKHR.
    void recordImGuiCommandBuffer();

    // Low-level Vulkan helpers

    /// @brief Creates a @c VkImage and binds device memory satisfying @p properties.
    ///
    /// @param width        Image width in pixels.
    /// @param height       Image height in pixels.
    /// @param mipLevels    Number of mip levels to allocate.
    /// @param format       Pixel format (e.g. @c eR8G8B8A8Srgb).
    /// @param tiling       @c eOptimal for device-local images; @c eLinear for host reads.
    /// @param usage        Usage flags (sampled, color attachment, transfer, etc.).
    /// @param properties   Required memory property flags (e.g. @c eDeviceLocal).
    /// @param image        [out] Receives the created @c vk::raii::Image.
    /// @param imageMemory  [out] Receives the bound @c vk::raii::DeviceMemory.
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                     vk::Format format, vk::ImageTiling tiling,
                     vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
                     vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);

    /// @brief Creates a 2D @c VkImageView for the given image.
    ///
    /// @param image        The image to create a view for.
    /// @param format       Must match the image's creation format.
    /// @param aspectFlags  e.g. @c eColor or @c eDepth.
    /// @param mipLevels    Number of mip levels exposed by the view.
    /// @returns A new @c vk::raii::ImageView.
    [[nodiscard]] vk::raii::ImageView createImageView(vk::raii::Image& image,
                                                      vk::Format format,
                                                      vk::ImageAspectFlags aspectFlags,
                                                      uint32_t mipLevels);

    /// @brief Creates a @c VkBuffer and binds device memory satisfying @p properties.
    ///
    /// @param size          Buffer size in bytes.
    /// @param usage         Usage flags (vertex, index, uniform, transfer, etc.).
    /// @param properties    Required memory property flags.
    /// @param buffer        [out] Receives the created @c vk::raii::Buffer.
    /// @param bufferMemory  [out] Receives the bound @c vk::raii::DeviceMemory.
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties,
                      vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);

    /// @brief Copies @p size bytes from @p src to @p dst using a single-time command buffer.
    ///
    /// @param src   Source buffer (must have @c eTransferSrc usage).
    /// @param dst   Destination buffer (must have @c eTransferDst usage).
    /// @param size  Number of bytes to copy.
    void copyBuffer(vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size);

    /// @brief Emits a @c pipelineBarrier2 into the currently-open frame command buffer.
    ///
    /// Used to transition swapchain and offscreen images during recording, where a
    /// single-time submit would be incorrect. Uses Synchronization2 for precise
    /// stage and access masks.
    ///
    /// @param image      The image to transition.
    /// @param oldLayout  Layout the image is currently in.
    /// @param newLayout  Layout to transition into.
    /// @param srcAccess  Access types that must complete before the barrier.
    /// @param dstAccess  Access types that must wait until after the barrier.
    /// @param srcStage   Pipeline stage(s) that produce @p srcAccess.
    /// @param dstStage   Pipeline stage(s) that consume @p dstAccess.
    /// @param aspect     Image aspect mask (e.g. @c eColor or @c eDepth).
    /// @param mipLevels  Number of mip levels covered by the barrier.
    void transitionImageInFrame(vk::Image image,
                                vk::ImageLayout oldLayout,   vk::ImageLayout newLayout,
                                vk::AccessFlags2 srcAccess,  vk::AccessFlags2 dstAccess,
                                vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage,
                                vk::ImageAspectFlags aspect, uint32_t mipLevels);

    /// @brief Allocates a primary command buffer from @c m_uploadCommandPool and
    ///        begins recording with @c eOneTimeSubmit.
    ///
    /// Pair each call with @c endSingleTimeCommands().
    ///
    /// @returns A heap-allocated @c vk::raii::CommandBuffer in the recording state.
    [[nodiscard]] std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands();

    /// @brief Ends recording, submits the command buffer, and blocks until the queue is idle.
    ///
    /// @param cmd  The command buffer returned by @c beginSingleTimeCommands().
    ///             Freed when this function returns.
    void endSingleTimeCommands(vk::raii::CommandBuffer& cmd);

    /// @brief Finds the index of a device memory type satisfying both @p typeFilter
    ///        and @p props.
    ///
    /// @param typeFilter  Bitmask of acceptable memory type indices from
    ///                    @c VkMemoryRequirements::memoryTypeBits.
    /// @param props       Required property flags (e.g. @c eDeviceLocal).
    /// @returns The matching memory type index.
    /// @throws std::runtime_error if no suitable type exists.
    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags props) const;

    /// @brief Returns the best depth format supported by the physical device.
    ///
    /// Queries @c findSupportedFormat() with candidates
    /// @c eD32Sfloat -> @c eD32SfloatS8Uint -> @c eD24UnormS8Uint and
    /// requires @c eDepthStencilAttachment optimal tiling support.
    ///
    /// @returns The first supported depth (or depth-stencil) format.
    [[nodiscard]] vk::Format findDepthFormat() const;

    /// @brief Finds the first format in @p candidates supported for @p tiling and @p features.
    ///
    /// @param candidates  Ordered list of formats to test.
    /// @param tiling      @c eLinear or @c eOptimal.
    /// @param features    Required @c VkFormatFeatureFlags (e.g. @c eDepthStencilAttachment).
    /// @returns The first matching format.
    /// @throws std::runtime_error if no candidate satisfies the requirements.
    [[nodiscard]] vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates,
                                                 vk::ImageTiling tiling,
                                                 vk::FormatFeatureFlags features) const;

    /// @brief Reads a SPIR-V shader binary from @c <cwd>/Shaders/<fileName>.
    ///
    /// @param fileName  File name including extension (e.g. @c "Base_Shader.spv").
    /// @returns The raw binary data as a @c std::vector<char>.
    /// @throws std::runtime_error if the file cannot be opened.
    [[nodiscard]] static std::vector<char> readShader(const std::string& fileName);

    /// @brief Wraps raw SPIR-V bytes in a @c VkShaderModule.
    ///
    /// @param code  SPIR-V binary as returned by @c readShader(). Size must be a
    ///              multiple of 4 bytes.
    /// @returns A @c vk::raii::ShaderModule valid until destroyed.
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;

    void createTextureDescriptorSetLayout();

private:
    // Members
    
    gcep::rhi::SwapchainDesc m_swapchainDesc;
    VulkanDevice             m_device;
    bool                     m_framebufferResized = false;

    /// ImGui's dedicated descriptor pool (11 types x 1000 sets).
    VkDescriptorPool         m_descriptorPoolImGui = VK_NULL_HANDLE;

    /// Persistent command pool for @c beginSingleTimeCommands() / @c endSingleTimeCommands().
    vk::raii::CommandPool    m_uploadCommandPool   = nullptr;

    /// Swapchain format cached at init for pipeline creation.
    vk::Format               m_swapchainFormat     = vk::Format::eUndefined;

    std::vector<vk::raii::Buffer>       m_uniformBuffers;
    std::vector<vk::raii::DeviceMemory> m_uniformBuffersMemory;

    /// Persistently-mapped pointers into @c m_uniformBuffersMemory; written each frame.
    std::vector<void*>                  m_uniformBuffersMapped;

    vk::raii::DescriptorSetLayout        m_UBOdescriptorSetLayout = nullptr;
    vk::raii::DescriptorSetLayout        m_TextureDescriptorSetLayout = nullptr;
    vk::raii::DescriptorPool             m_descriptorPool      = nullptr;
    std::vector<vk::raii::DescriptorSet> m_descriptorSets;

    VulkanTexture            texture;
    VulkanMesh               mesh;
    VulkanTexture            texture2;
    VulkanMesh               mesh2;

    vk::raii::PipelineLayout m_pipelineLayout   = nullptr;
    vk::raii::Pipeline       m_graphicsPipeline = nullptr;

    vk::Extent2D             m_offscreenExtent{};
    vk::raii::Image          m_offscreenImage            = nullptr;
    vk::raii::DeviceMemory   m_offscreenImageMemory      = nullptr;
    vk::raii::ImageView      m_offscreenImageView        = nullptr;
    vk::raii::Image          m_offscreenDepthImage       = nullptr;
    vk::raii::DeviceMemory   m_offscreenDepthImageMemory = nullptr;
    vk::raii::ImageView      m_offscreenDepthImageView   = nullptr;
    vk::raii::Sampler        m_offscreenSampler          = nullptr;

    /// Registered with @c ImGui_ImplVulkan_AddTexture; displayed via @c ImGui::Image().
    VkDescriptorSet          m_imguiTextureDescriptor    = VK_NULL_HANDLE;

    bool                     m_offscreenResizePending  = false;
    uint32_t                 m_pendingOffscreenWidth   = 0;
    uint32_t                 m_pendingOffscreenHeight  = 0;

    ImVec4                   m_clearColor{ 0.1f, 0.1f, 0.1f, 1.0f };

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    /// Epoch for the model-rotation animation in @c updateUniformBuffer().
    std::chrono::high_resolution_clock::time_point m_startTime =
            std::chrono::high_resolution_clock::now();
};

} // Namespace gcep::rhi::vulkan

namespace std {
/// @brief Hash specialisation for @c Vertex enabling its use as an @c unordered_map key.
///
/// Combines the GLM hashes of @c pos, @c color, and @c texCoord using the standard
/// hash-combine XOR/shift pattern. Requires @c GLM_ENABLE_EXPERIMENTAL and
/// @c <glm/gtx/hash.hpp>.
template<> struct hash<gcep::rhi::vulkan::Vertex> {
    size_t operator()(gcep::rhi::vulkan::Vertex const& vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};

} // Namespace std