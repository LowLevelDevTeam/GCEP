#pragma once

#include <Engine/RHI/RHI.hpp>
#include <Engine/RHI/Vulkan/VulkanDevice.hpp>
#include <Engine/RHI/Vulkan/VulkanMesh.hpp>
#include <Engine/RHI/Vulkan/VulkanRHIDataTypes.hpp>
#include <Engine/RHI/Vulkan/VulkanTexture.hpp>
#include <ECS/headers/registry.hpp>

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
/// @brief Top-level Vulkan renderer implementing the @c gcep::RHI interface.
///
/// @c VulkanRHI owns:
///   - @c VulkanDevice - device, swapchain, and per-frame contexts.
///   - Scene resources - merged global vertex + index buffers, per-mesh SSBO,
///     GPU-driven indirect draw buffer, bindless texture array, UBO, descriptor
///     layouts / pools / sets, and both the graphics and compute (culling) pipelines.
///   - Offscreen render target - a separate color + depth image rendered into by the
///     scene pass and displayed inside an ImGui viewport via @c ImGui_ImplVulkan_AddTexture.
///   - ImGui integration - its own @c VkDescriptorPool, GLFW + Vulkan backends.
///
/// @par Frame sequence (drawFrame)
///   @code
///   processPendingOffscreenResize()   // deferred to avoid mid-frame resize hazards
///   // handle deferred swapchain resize from GLFW callback
///   beginFrame()                      // fence wait + acquire
///   perFrameUpdate()                  // mesh transform updates + SSBO upload
///   recordOffscreenCommandBuffer()    // cull pass -> scene -> offscreen image
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
    ///   3. @c initSceneResources() - UBO, bindless textures, mesh loading,
    ///      global buffers, descriptor sets, cull pipeline, and graphics pipeline.
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

    vk::raii::PhysicalDevice* getPhysDev();

    // Editor surface

    /// @brief Sets the framebuffer-resized flag.
    ///
    /// Called from the GLFW @c framebufferResizeCallback. The flag is consumed at
    /// the top of the next @c drawFrame() call, which then calls @c recreateSwapchain().
    ///
    /// @param resized  Pass @c true when the framebuffer size has changed.
    void setFramebufferResized(bool resized) noexcept { m_framebufferResized = resized; }

    /// @brief Pushes per-frame editor state into the renderer.
    ///
    /// Stores @p clearColor and @p ubo for use by @c recordOffscreenCommandBuffer()
    /// and @c perFrameUpdate() respectively. Called by the editor once per frame
    /// before @c drawFrame().
    ///
    /// @param clearColor  RGBA clear color in linear [0, 1] float space.
    /// @param ubo         Camera view and projection matrices for the current frame.
    void updateCameraUBO(UniformBufferObject ubo);

    void spawnCube(glm::vec3 pos = {0.0f, 0.0f, 0.0f});
    void spawnVikingRoom(glm::vec3 pos = {0.0f, 0.0f, 0.0f});
    void spawnAsset(char* filepath, glm::vec3 pos = {0.0f, 0.0f, 0.0f});
    void setGridPC(GridPushConstant* gridPC);

    /// @brief Uploads per-frame lighting and camera data to the persistently-mapped scene UBO.
    ///
    /// Constructs a @c SceneUBO from the provided parameters and memcpys it into all
    /// mapped scene UBO slots (one per frame-in-flight). Call this once per frame
    /// before recording the offscreen command buffer, typically from @c perFrameUpdate().
    ///
    /// @param lightDir    Normalized world-space direction pointing toward the light source.
    ///                    Does not need to be pre-normalized; the shader re-normalizes in the fragment stage.
    /// @param lightCol    Linear RGB color and intensity of the directional light (e.g. @c {1,1,1} for white).
    /// @param ambientCol  Linear RGB ambient term applied uniformly to all fragments (e.g. @c {0.05,0.05,0.05}).
    /// @param cameraPos   World-space camera position, used to compute the half-vector for specular highlights.
    void updateSceneUBO(rhi::vulkan::SceneInfos* upstr, glm::vec3 cameraPos);

    /// @brief Defers an offscreen render target resize to the next frame boundary.
    ///
    /// Safe to call at any time, including while a frame is being recorded.
    /// If @p width × @p height matches the current extent, the request is ignored.
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
    [[nodiscard]] VkDescriptorSet* getImGuiTextureDescriptor() { return &m_imguiTextureDescriptor; }

    /// @brief Returns a mutable reference to the underlying @c VulkanDevice.
    [[nodiscard]] VulkanDevice& device() noexcept { return m_device; }

    /// @brief Returns a const reference to the underlying @c VulkanDevice.
    [[nodiscard]] const VulkanDevice& device() const noexcept { return m_device; }

    void setVSync(bool vsync);
    bool isVSync();

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
    ImGui_ImplVulkan_InitInfo getUIInitInfo();

    /// @brief Applies a pending offscreen resize if one was requested via
    ///        @c requestOffscreenResize().
    ///
    /// Clears the pending flag then calls @c resizeOffscreenResources(). Called at the
    /// very start of @c drawFrame() so the resize happens between frames, never
    /// mid-recording.
    void processPendingOffscreenResize();

    /// @brief Returns the number of draw calls issued by the GPU cull pass in the last frame.
    ///
    /// Reads directly from the persistently-mapped @c m_drawCountBuffer, which is
    /// written atomically by the culling compute shader via @c InterlockedAdd.
    /// The value reflects the previous frame due to GPU/CPU pipelining - do not
    /// use it for synchronisation, only for display (e.g. an editor debug overlay).
    ///
    /// @returns Number of meshes that passed frustum culling in the previous frame.
    uint32_t getDrawCount();

    std::vector<VulkanMesh>& getMeshData();

    ECS::Registry* getRegistry();

    InitInfos* getInitInfos();

    void addTexture(ECS::EntityID id, char *path, bool mipmaps = true);

private:
    // Init helpers

    /// @brief Calls all scene-resource init functions in dependency order.
    ///
    /// Order: UBO layout/pool/buffers/sets -> bindless layout/pool/set ->
    /// mesh loading -> @c initMeshBuffers() -> @c createMeshDataDescriptors() ->
    /// @c createCullPipeline() -> @c createGraphicsPipeline().
    void initSceneResources();

    /// @brief Recreates the swapchain in response to a resize event.
    ///
    /// Spins on @c glfwGetFramebufferSize until the window is non-zero
    /// (handles minimization), then delegates to @c VulkanDevice::recreateSwapchain().
    void recreateSwapchain(bool forVSync = false);

    // UBO

    /// @brief Creates the descriptor set layout for the per-frame scene UBO.
    ///
    /// Single binding at location 2: @c eUniformBuffer, fragment stage only.
    void createSceneUBOLayout();

    /// @brief Creates the descriptor pool that backs the per-frame UBO descriptor sets.
    ///
    /// Allocates @c MAX_FRAMES_IN_FLIGHT uniform buffer descriptors.
    void createSceneUBOPool();

    /// @brief Allocates one host-visible, host-coherent UBO per frame-in-flight
    ///        and maps each persistently into @c m_uniformBuffersMapped.
    void createSceneUBOBuffers();

    /// @brief Allocates @c MAX_FRAMES_IN_FLIGHT descriptor sets from @c m_UBOPool
    ///        and writes each one to point at its corresponding uniform buffer.
    void createSceneUBOSets();

    /// @brief Re-writes all UBO descriptor sets to point at the current uniform buffers.
    ///
    /// Called once after initial allocation and again if buffers are recreated.
    void updateSceneUBOSets();

    // Pipeline

    /// @brief Compiles and links the graphics pipeline for the offscreen scene pass.
    ///
    /// Reads @c Base_Shader.spv from the @c Shaders/ directory (single SPIR-V binary
    /// with entry points @c vertMain and @c fragMain). Pipeline layout uses two
    /// descriptor sets: set 0 = mesh data SSBO, set 1 = bindless texture array.
    /// A single push constant range carries the 64-byte @c viewProj matrix.
    /// Enables depth test/write, back-face culling, counter-clockwise front faces,
    /// and dynamic viewport/scissor. Configured for dynamic rendering targeting
    /// the offscreen color + depth format.
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

    /// @brief Performs all CPU-side per-frame updates before command recording.
    ///
    /// Computes @c time from @c m_startTime, applies procedural mesh animations
    /// (rotation on meshes[0], sinusoidal X translation on meshes[1]), calls
    /// @c refreshMeshData() to sync @c m_meshData with current transforms,
    /// @c updateMeshDataSSBO() to push the SSBO to the GPU, and
    /// @c updateSceneUBO() to copy UBO data into the mapped buffers.
    void perFrameUpdate();

    /// @brief Records the GPU frustum-cull compute pass into the current command buffer.
    ///
    /// Steps performed in order:
    ///   1. Resets @c m_drawCountBuffer to zero via the persistently-mapped pointer.
    ///   2. Emits a host-write -> compute-shader barrier on the count buffer, and a
    ///      draw-indirect-read -> compute-shader-write barrier on the indirect buffer.
    ///   3. Extracts frustum planes from the current @c viewProj matrix via
    ///      @c extractFrustum() and uploads them as a push constant.
    ///   4. Binds the cull pipeline and descriptor set, dispatches
    ///      @c ceil(objectCount / 64) thread groups.
    ///   5. Emits compute-shader-write -> draw-indirect-read barriers on both
    ///      the indirect buffer and the count buffer.
    ///
    /// Must be called before @c beginRendering() in @c recordOffscreenCommandBuffer().
    void recordCullPass();

    void createGridPipeline();
    void recordGridPass();

    /// @brief Records the scene pass into the current frame's command buffer.
    ///
    /// Transitions the offscreen color image (@c eShaderReadOnlyOptimal ->
    /// @c eColorAttachmentOptimal) and depth image (@c eUndefined ->
    /// @c eDepthAttachmentOptimal), calls @c recordCullPass(), begins a dynamic
    /// render pass, binds the graphics pipeline, viewport, scissor, global vertex
    /// and index buffers, descriptor sets (mesh SSBO + bindless textures), pushes
    /// the @c viewProj constant, then issues @c drawIndexedIndirectCount using
    /// the GPU-written @c m_indirectBuffer and @c m_drawCountBuffer. Transitions
    /// color back to @c eShaderReadOnlyOptimal after @c endRendering so ImGui
    /// can sample it.
    void recordOffscreenCommandBuffer();

    /// @brief Records the ImGui pass into the current frame's command buffer,
    ///        targeting the acquired swapchain image.
    ///
    /// Transitions the swapchain image (@c eUndefined -> @c eColorAttachmentOptimal),
    /// begins a dynamic render pass, calls @c ImGui_ImplVulkan_RenderDrawData,
    /// ends the pass, handles multi-viewport platform windows, then transitions the
    /// swapchain image to @c ePresentSrcKHR.
    void recordImGuiCommandBuffer();

    // Bindless textures

    /// @brief Creates the bindless descriptor set layout for the global texture array.
    ///
    /// Single binding at location 0: @c eCombinedImageSampler array of @c MAX_TEXTURES,
    /// fragment stage. Flags: @c ePartiallyBound | @c eVariableDescriptorCount |
    /// @c eUpdateAfterBind. Layout creation flag: @c eUpdateAfterBindPool.
    void createBindlessLayout();

    /// @brief Creates the descriptor pool backing the bindless texture set.
    ///
    /// Flags: @c eUpdateAfterBind | @c eFreeDescriptorSet.
    /// Capacity: @c MAX_TEXTURES combined image samplers, 1 set.
    void createBindlessPool();

    /// @brief Allocates the single global bindless descriptor set from @c m_bindlessPool.
    ///
    /// Uses @c VkDescriptorSetVariableDescriptorCountAllocateInfo with
    /// @c MAX_TEXTURES as the variable descriptor count.
    void createBindlessSet();

    // Mesh data / GPU scene buffers

    /// @brief Builds and uploads the global vertex buffer, index buffer, per-mesh
    ///        SSBO, and indirect draw command buffer from the loaded @c meshes.
    ///
    /// Iterates over @c meshes once to accumulate total byte sizes, allocates one
    /// host-visible staging buffer for vertices and one for indices, memcpys all
    /// mesh data contiguously, then transfers both to device-local buffers in a
    /// single command buffer. Simultaneously builds @c m_meshData and
    /// @c m_indirectCommands (one entry per mesh, @c firstInstance = mesh index).
    /// Finally calls @c uploadMeshDataSSBO() (initial upload) and
    /// @c uploadIndirectBuffer().
    void initMeshBuffers();

    /// @brief Copies the current @c m_meshData vector into the mapped SSBO.
    ///
    /// The SSBO is host-visible and persistently mapped (@c m_meshDataSSBOMapped),
    /// so this is a plain @c memcpy with no staging or synchronisation overhead.
    /// Called every frame from @c perFrameUpdate() after @c refreshMeshData().
    void updateMeshDataSSBO();

    /// @brief Uploads the @c m_indirectCommands vector to @c m_indirectBuffer
    ///        via a staging buffer.
    ///
    /// The indirect buffer is device-local with @c eIndirectBuffer |
    /// @c eStorageBuffer | @c eTransferDst usage. @c eStorageBuffer is required
    /// because the cull compute shader writes into it each frame.
    /// Called once during @c initMeshBuffers().
    void uploadIndirectBuffer();

    /// @brief Creates the descriptor set layout, pool, set, and writes the SSBO
    ///        binding for the per-mesh data accessible in the vertex shader.
    ///
    /// Single binding at location 0: @c eStorageBuffer, vertex stage.
    /// The SSBO backing @c m_meshDataSSBO must already exist before calling this.
    void createMeshDataDescriptors();

    /// @brief Re-writes the @c m_meshDataDescriptorSet binding after the SSBO is
    ///        recreated (e.g. if mesh count changes and the buffer is reallocated).
    void updateMeshDataSet();

    /// @brief Syncs @c m_meshData[i].transform (and AABB) from each @c meshes[i].
    ///
    /// Must be called before @c updateMeshDataSSBO() whenever any mesh transform
    /// has changed. Iterates over all meshes and overwrites the @c transform,
    /// @c aabbMin, and @c aabbMax fields in the CPU-side @c m_meshData mirror.
    void refreshMeshData();

    // Culling compute pipeline

    /// @brief Creates the GPU frustum culling compute pipeline and all associated
    ///        Vulkan objects.
    ///
    /// Creates:
    ///   - @c m_cullSetLayout - three @c eStorageBuffer bindings:
    ///     binding 0 = read-only @c ObjectData array, binding 1 = write
    ///     @c DrawIndexedIndirectCommand array, binding 2 = atomic draw count.
    ///   - @c m_cullPool / @c m_cullSet - pool + set backed by the three SSBOs.
    ///   - @c m_drawCountBuffer - host-visible, host-coherent @c uint32_t buffer
    ///     persistently mapped into @c m_drawCountMapped. Reset to 0 each frame
    ///     before the dispatch.
    ///   - @c m_cullPipelineLayout - push constant range: @c FrustumPlanes,
    ///     compute stage.
    ///   - @c m_cullPipeline - compute pipeline reading @c Cull.spv with entry
    ///     point @c cullMain.
    ///
    /// Must be called after @c uploadMeshDataSSBO() and @c uploadIndirectBuffer()
    /// so the SSBOs exist when the descriptor set is written.
    void createCullPipeline();

    /// @brief Extracts the six world-space frustum planes from a combined
    ///        view-projection matrix using the Gribb-Hartmann method.
    ///
    /// Each plane is derived from a row combination of @p viewProj and then
    /// normalised so that the plane equation @c dot(n, p) + d can be used
    /// directly as a signed distance test.
    ///
    /// @note See https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
    ///
    /// @param viewProj  The combined @c proj * view matrix for the current frame.
    /// @returns A @c FrustumPlanes struct with six normalised planes and
    ///          @c objectCount set to @c m_meshData.size().
    [[nodiscard]] FrustumPlanes extractFrustum(const glm::mat4& viewProj) const;

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

    void uploadSingleMesh(uint32_t meshIndex);
    void updateMeshMetadata(uint32_t meshIndex);

private:
    // Members

    gcep::rhi::SwapchainDesc m_swapchainDesc;
    VulkanDevice             m_device;
    bool                     m_framebufferResized = false;

    /// ImGui's dedicated descriptor pool (11 types x 1000 sets).
    VkDescriptorPool         m_descriptorPoolImGui = VK_NULL_HANDLE;

    /// Persistent command pool for @c beginSingleTimeCommands() / @c endSingleTimeCommands().
    vk::raii::CommandPool    m_uploadCommandPool   = nullptr;

    /// Swapchain format cached at init for pipeline creation and ImGui init info.
    vk::Format               m_swapchainFormat     = vk::Format::eUndefined;

    /// Persistently-mapped pointer into @c m_meshDataSSBOMemory.
    /// Written by @c updateMeshDataSSBO(); never unmapped.
    void* m_meshDataSSBOMapped = nullptr;

    /// Persistently-mapped pointer into @c m_drawCountBufferMemory.
    /// Reset to 0 each frame in @c recordCullPass(); read by @c getDrawCount().
    void* m_drawCountMapped    = nullptr;

    // Bindless texture array - set 1 in the graphics pipeline
    vk::raii::DescriptorSetLayout m_bindlessLayout = nullptr;
    vk::raii::DescriptorPool      m_bindlessPool   = nullptr;

    // UBO
    std::vector<vk::raii::Buffer>       m_uniformBuffers;
    std::vector<vk::raii::DeviceMemory> m_uniformBuffersMemory;

    /// Persistently-mapped pointers into @c m_uniformBuffersMemory; written each frame.
    std::vector<void*>                   m_uniformBuffersMapped;
    vk::raii::DescriptorSetLayout        m_UBOLayout = nullptr;
    vk::raii::DescriptorPool             m_UBOPool   = nullptr;
    std::vector<vk::raii::DescriptorSet> m_UBOSets;

    vk::raii::DescriptorSet       m_bindlessSet    = nullptr;
    // Global scene geometry buffers (all meshes merged into one allocation each)
    vk::raii::Buffer       m_globalVertexBuffer       = nullptr;
    vk::raii::DeviceMemory m_globalVertexBufferMemory = nullptr;
    vk::raii::Buffer       m_globalIndexBuffer        = nullptr;

    vk::raii::DeviceMemory m_globalIndexBufferMemory  = nullptr;

    // Mesh data SSBO - set 0 in the graphics pipeline, read by the vertex shader
    vk::raii::DescriptorSetLayout m_meshDataDescriptorSetLayout = nullptr;
    vk::raii::DescriptorPool      m_meshDataDescriptorPool      = nullptr;
    vk::raii::DescriptorSet       m_meshDataDescriptorSet       = nullptr;
    vk::raii::Buffer              m_meshDataSSBO                = nullptr;
    vk::raii::DeviceMemory        m_meshDataSSBOMemory          = nullptr;

    // Indirect draw buffer - written by the cull compute shader, consumed by drawIndexedIndirectCount
    vk::raii::Buffer       m_indirectBuffer       = nullptr;
    vk::raii::DeviceMemory m_indirectBufferMemory = nullptr;
    // Pre-allocated capacities
    static constexpr vk::DeviceSize MAX_VERTEX_BUFFER_BYTES = 512 * 1024 * 1024; // 512 MB
    static constexpr vk::DeviceSize MAX_INDEX_BUFFER_BYTES  = 256 * 1024 * 1024; // 256 MB

    // Runtime fill tracking
    vk::DeviceSize m_vertexBytesFilled = 0;
    vk::DeviceSize m_indexBytesFilled  = 0;
    uint32_t       m_totalVertexCount  = 0;
    uint32_t       m_totalIndexCount   = 0;

    // Persistent mapped SSBO for indirect commands
    void*          m_indirectBufferMapped   = nullptr;
    vk::DeviceSize m_indirectBufferCapacity = 0;

    // CPU-side mirrors of GPU buffers, rebuilt each frame by refreshMeshData()
    std::vector<GPUMeshData>                    m_meshData;
    std::vector<vk::DrawIndexedIndirectCommand> m_indirectCommands;

    std::vector<VulkanMesh> meshes;

    // Frustum culling compute pipeline
    vk::raii::Pipeline            m_cullPipeline       = nullptr;
    vk::raii::PipelineLayout      m_cullPipelineLayout = nullptr;
    vk::raii::DescriptorSetLayout m_cullSetLayout      = nullptr;
    vk::raii::DescriptorPool      m_cullPool           = nullptr;
    vk::raii::DescriptorSet       m_cullSet            = nullptr;

    // Grid pipeline
    vk::raii::Pipeline            m_gridPipeline       = nullptr;
    vk::raii::PipelineLayout      m_gridPipelineLayout = nullptr;

    /// Host-visible, persistently-mapped atomic draw count written by the cull shader.
    vk::raii::Buffer       m_drawCountBuffer       = nullptr;
    vk::raii::DeviceMemory m_drawCountBufferMemory = nullptr;

    // Graphics pipeline
    vk::raii::PipelineLayout m_pipelineLayout   = nullptr;
    vk::raii::Pipeline       m_graphicsPipeline = nullptr;

    // Offscreen render target
    vk::Extent2D           m_offscreenExtent{};
    vk::raii::Image        m_offscreenImage            = nullptr;
    vk::raii::DeviceMemory m_offscreenImageMemory      = nullptr;
    vk::raii::ImageView    m_offscreenImageView        = nullptr;
    vk::raii::Image        m_offscreenDepthImage       = nullptr;
    vk::raii::DeviceMemory m_offscreenDepthImageMemory = nullptr;
    vk::raii::ImageView    m_offscreenDepthImageView   = nullptr;
    vk::raii::Sampler      m_offscreenSampler          = nullptr;

    /// Registered with @c ImGui_ImplVulkan_AddTexture; displayed via @c ImGui::Image().
    VkDescriptorSet m_imguiTextureDescriptor = VK_NULL_HANDLE;

    bool     m_offscreenResizePending = false;
    uint32_t m_pendingOffscreenWidth  = 0;
    uint32_t m_pendingOffscreenHeight = 0;

    /// Cached draw count read from @c m_drawCountBuffer after each frame for the editor overlay.
    uint32_t m_drawCount = 0;

    glm::vec4           m_clearColor{ 0.1f, 0.1f, 0.1f, 1.0f };
    UniformBufferObject m_cameraUBO;
    SceneUBO            m_sceneUBO;
    float               m_shininess;

    ECS::Registry m_ECSRegistry;

    static constexpr int      MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr uint32_t MAX_MESHES           = 4096;
    InitInfos m_initInfos;
    GridPushConstant m_gridPC;

    /// Epoch used for procedural animations in @c perFrameUpdate().
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
                (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};

} // Namespace std
