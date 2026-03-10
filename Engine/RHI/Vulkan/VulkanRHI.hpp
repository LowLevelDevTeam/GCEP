#pragma once

#include <ECS/headers/registry.hpp>
#include <Engine/RHI/Mesh/Mesh.hpp>
#include <Engine/RHI/Mesh/MeshCache.hpp>
#include <Engine/RHI/RHI.hpp>
#include <Engine/RHI/Vulkan/VulkanDevice.hpp>
#include <Engine/RHI/Vulkan/VulkanRHIDataTypes.hpp>
#include <Engine/RHI/Vulkan/VulkanTexture.hpp>

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
/// @c VulkanRHI is the central rendering context for the engine. It owns and
/// manages the full Vulkan resource hierarchy:
///
/// @par Owned subsystems
///   - Device - @c VulkanDevice wrapping the physical/logical device,
///     swapchain, and per-frame command buffers.
///   - Scene geometry - merged global vertex + index buffers (device-local,
///     pre-allocated to @c MAX_VERTEX_BUFFER_BYTES / @c MAX_INDEX_BUFFER_BYTES),
///     a per-mesh SSBO for transforms and metadata, and a GPU-driven indirect
///     draw buffer written by the cull compute shader.
///   - Bindless textures - a partially-bound @c eCombinedImageSampler array
///     at set 1, binding 0, supporting up to @c MAX_TEXTURES entries.
///   - Offscreen render target - a separate RGBA16F color + D32 depth image
///     rendered into by the scene pass and displayed inside an ImGui viewport
///     via @c ImGui_ImplVulkan_AddTexture.
///   - ImGui integration - dedicated @c VkDescriptorPool, GLFW + Vulkan
///     backends, multi-viewport support.
///   - ECS - owns the @c ECS::Registry; entities are created/destroyed via
///     @c spawnAsset() / @c removeMesh().
///
/// @par Frame sequence (@c drawFrame)
///   @code
///   processPendingOffscreenResize()  // deferred resize - never mid-frame
///   // deferred swapchain resize from GLFW callback if flagged
///   beginFrame()                     // fence wait + swapchain acquire
///   perFrameUpdate()                 // transform sync + SSBO + UBO upload
///   recordOffscreenCommandBuffer()   // cull -> scene -> grid -> offscreen image
///   ImGui::Render()
///   recordImGuiCommandBuffer()       // ImGui draw data -> swapchain image
///   endFrame()                       // queue submit + present
///   @endcode
///
/// @par Descriptor set layout (graphics pipeline)
///   - Set 0, binding 0 - @c GPUMeshData SSBO (vertex stage)
///   - Set 1, binding 0 - bindless @c Sampler2D[] (fragment stage)
///   - Set 2, binding 0 - @c SceneUBO (fragment stage)
///   - Push constants   - @c PushConstant (vertex + fragment)
///
/// @par Thread safety
///   All public methods must be called from the main/render thread.
///   No internal synchronisation is provided.
class VulkanRHI final : public gcep::RHI
{
    friend class VulkanTexture;
public:
    /// @brief Constructs the RHI object and stores the swapchain creation parameters.
    ///
    /// No Vulkan objects are created here. Call @c setWindow() then @c initRHI()
    /// before using any other method.
    ///
    /// @param desc  Swapchain parameters (width, height, vsync flag).
    ///              The @c nativeWindowHandle field is populated by @c setWindow().
    explicit VulkanRHI(const gcep::rhi::SwapchainDesc& desc);

    /// @brief Default destructor. All Vulkan resources are destroyed by their
    ///        RAII wrappers in reverse declaration order.
    ~VulkanRHI() override;

    VulkanRHI(const VulkanRHI&)            = delete;
    VulkanRHI& operator=(const VulkanRHI&) = delete;
    VulkanRHI(VulkanRHI&&)                 = delete;
    VulkanRHI& operator=(VulkanRHI&&)      = delete;

    // gcep::RHI

    /// @brief Stores the GLFW window handle and registers the framebuffer-resize callback.
    ///
    /// Must be called before @c initRHI(). Stores @p window in @c m_swapchainDesc
    /// and registers @c glfwSetFramebufferSizeCallback so that resize events set
    /// @c m_framebufferResized, which @c drawFrame() checks at the top of every frame.
    ///
    /// @param window  The application's GLFW window. Must outlive this object.
    void setWindow(GLFWwindow* window) override;

    /// @brief Initialises all Vulkan objects and scene resources.
    ///
    /// Initialisation order:
    ///   1. @c VulkanDevice::createVulkanDevice() - instance, device, swapchain.
    ///   2. Creates @c m_uploadCommandPool for single-time submit operations.
    ///   3. @c initSceneResources() - UBO, bindless textures, mesh buffers,
    ///      descriptor sets, culling pipeline, graphics pipeline, offscreen target.
    ///
    /// @throws std::runtime_error if any initialisation step fails.
    void initRHI() override;

    /// @brief Records and submits one complete frame.
    ///
    /// Returns early without submitting if @c beginFrame() reports an out-of-date
    /// swapchain, or if a swapchain / offscreen resize is serviced on this tick.
    /// See the class-level frame sequence for the full call order.
    void drawFrame() override;

    /// @brief Waits for GPU idle, shuts down ImGui, and frees the ImGui pool.
    ///
    /// Call order: @c m_device.waitIdle() -> remove registered ImGui texture
    /// descriptor -> @c ImGui_ImplVulkan_Shutdown -> @c ImGui_ImplGlfw_Shutdown
    /// -> @c ImGui::DestroyContext -> destroy @c m_descriptorPoolImGui.
    /// Must be called before the window is destroyed.
    void cleanup() override;

    // Editor / window surface

    /// @brief Sets the framebuffer-resized flag.
    ///
    /// Called from the GLFW @c framebufferResizeCallback. The flag is consumed
    /// at the top of the next @c drawFrame() call, triggering @c recreateSwapchain().
    ///
    /// @param resized  @c true when the framebuffer dimensions have changed.
    void setFramebufferResized(bool resized) noexcept { m_framebufferResized = resized; }

    /// @brief Uploads per-frame camera matrices to the persistently-mapped UBO.
    ///
    /// Stores @p ubo for use in @c recordOffscreenCommandBuffer() during the
    /// same frame. Call once per frame before @c drawFrame().
    ///
    /// @param ubo  Camera view and projection matrices for the current frame.
    void updateCameraUBO(UniformBufferObject ubo);

    /// @brief Uploads per-frame lighting and camera data to the scene UBO.
    ///
    /// Constructs a @c SceneUBO from @p upstr and @p cameraPos and memcpys it
    /// into all persistently-mapped UBO slots (one per frame-in-flight). Call
    /// once per frame before @c drawFrame().
    ///
    /// @param upstr      Scene lighting parameters (light direction, color, ambient).
    /// @param cameraPos  World-space camera position for specular highlight computation.
    void updateSceneUBO(rhi::vulkan::SceneInfos* upstr, glm::vec3 cameraPos);

    /// @brief Defers an offscreen render target resize to the next frame boundary.
    ///
    /// Safe to call at any time, including while a frame is being recorded.
    /// Ignored if @p width × @p height matches the current extent. Applied by
    /// @c processPendingOffscreenResize() at the top of the next @c drawFrame() call.
    ///
    /// @param width   New render target width in pixels. Must be > 0.
    /// @param height  New render target height in pixels. Must be > 0.
    void requestOffscreenResize(uint32_t width, uint32_t height);

    /// @brief Enables or disables vertical synchronisation.
    ///
    /// Triggers a swapchain recreation with the requested present mode.
    /// Blocks until the GPU is idle before recreating.
    ///
    /// @param vsync  @c true for @c eFifo (vsync on), @c false for @c eImmediate.
    void setVSync(bool vsync);

    /// @brief Returns whether vertical synchronisation is currently enabled.
    [[nodiscard]] bool isVSync();

    // Scene management

    /// @brief Spawns a unit cube at @p pos and uploads it to the GPU.
    ///
    /// Creates an ECS entity, adds @c ECS::Transform and @c PhysicsComponent,
    /// loads a built-in cube OBJ, and calls @c uploadSingleMesh(). The cube is
    /// registered as a static non-moving body.
    ///
    /// @param pos  Initial world-space position. Defaults to the origin.
    void spawnCone(glm::vec3 pos = {0.0f, 0.0f, 0.0f});
    void spawnCube(glm::vec3 pos = {0.0f, 0.0f, 0.0f});
    void spawnCylinder(glm::vec3 pos = {0.0f, 0.0f, 0.0f});
    void spawnIcosphere(glm::vec3 pos = {0.0f, 0.0f, 0.0f});
    void spawnSphere(glm::vec3 pos = {0.0f, 0.0f, 0.0f});
    void spawnSuzanne(glm::vec3 pos = {0.0f, 0.0f, 0.0f});
    void spawnTorus(glm::vec3 pos = {0.0f, 0.0f, 0.0f});

    /// @brief Spawns an arbitrary OBJ asset at @p pos and uploads it to the GPU.
    ///
    /// Creates an ECS entity, loads the OBJ via @c VulkanMesh::loadMesh(),
    /// calls @c uploadSingleMesh(), and configures the transform and physics
    /// components. If the same OBJ path was previously uploaded, geometry is
    /// reused from @c m_meshCache without re-uploading.
    ///
    /// @param filepath  Path to the OBJ file. Must exist on disk.
    /// @param pos       Initial world-space position. Defaults to the origin.
    void spawnAsset(char* filepath, ECS::EntityID id, glm::vec3 pos = {0.0f, 0.0f, 0.0f});


    void removeMesh(ECS::EntityID id);


    /// @brief Destroys the mesh at @p meshIndex and compacts the GPU buffers.
    ///
    /// Releases the geometry reference in @c m_meshCache. If this was the last
    /// entity using that OBJ, physically compacts the global vertex and index
    /// buffers via @c compactGeometry(). Destroys the ECS entity, erases from
    /// @c meshes / @c m_meshData / @c m_indirectCommands, and rewrites the GPU
    /// SSBO and indirect buffer for all surviving slots.
    ///
    /// @param meshIndex  0-based index into @c meshes. Must be < @c meshes.size().

    /// @brief Loads or replaces the texture of the mesh identified by @p id.
    ///
    /// If @p path is already in the texture cache, the existing bindless slot is
    /// reused. Otherwise the texture is uploaded and registered. The mesh's
    /// @c GPUMeshData entry is updated on the next @c refreshMeshData() call.
    ///
    /// @param id      ECS @c EntityID of the target mesh.
    /// @param path    File path to the new texture (PNG / JPG / JPEG).
    /// @param mipmaps When @c true, generates a full mip chain.
    void addTexture(ECS::EntityID id, char* path, bool mipmaps = true);

    void uploadTexture(ECS::EntityID id, const char* texPath, bool mipmaps);

    void uploadMesh(ECS::EntityID id, const char* objPath);



    /// @brief Writes the grid push constant for the current frame.
    ///
    /// Stores @p gridPC for use in @c recordGridPass() during the same frame.
    /// Call once per frame before @c drawFrame() if the grid parameters have changed.
    ///
    /// @param gridPC  Grid rendering parameters (invViewProj, viewProj, cell size, etc.).
    void setGridPC(GridPushConstant* gridPC);

    void setSimulationStarted(bool started);

    void setRegistry(ECS::Registry* registry);

    // Queries

    /// @brief Returns a pointer to the mesh whose @c id field matches @p id,
    ///        or @c nullptr if no such mesh exists.
    ///
    /// Performs a linear search over @c meshes. Prefer caching the result for
    /// the duration of a single frame rather than calling this repeatedly.
    ///
    /// @param id  ECS @c EntityID to search for.
    [[nodiscard]] Mesh* findMesh(ECS::EntityID id);

    /// @brief Returns a pointer to the renderer initialisation info struct.
    [[nodiscard]] InitInfos* getInitInfos();

    /// @brief Returns the number of draw calls issued in the most recent frame.
    ///
    /// Reads directly from the persistently-mapped @c m_drawCountBuffer written
    /// atomically by the culling compute shader. The value lags by one frame due
    /// to GPU/CPU pipelining - use only for editor display, not synchronisation.
    [[nodiscard]] uint32_t getDrawCount();

    /// @brief Returns a mutable reference to the @c VulkanDevice.
    [[nodiscard]] VulkanDevice& device() noexcept { return m_device; }

    /// @brief Returns a const reference to the @c VulkanDevice.
    [[nodiscard]] const VulkanDevice& device() const noexcept { return m_device; }

    // TAA control

    /// @brief Sets the TAA blend alpha (weight of the current frame).
    ///
    /// Typical range: 0.05 – 0.15.  Higher values reduce ghosting but
    /// increase aliasing; lower values give smoother output at the cost of
    /// more ghosting on fast-moving geometry.
    ///
    /// @param alpha  Current-frame weight in [0, 1].
    void setTAABlendAlpha(float alpha) noexcept { m_taaBlendAlpha = std::clamp(alpha, 0.0f, 1.0f); }

    // ImGui integration

    /// @brief Builds and returns the @c ImGui_ImplVulkan_InitInfo for this device.
    ///
    /// Creates @c m_descriptorPoolImGui (11 descriptor types × 1000 sets), fills
    /// the init info with instance / device / queue handles, configures
    /// @c UseDynamicRendering, sets swapchain color and depth formats, and sets
    /// @c MSAASamples to @c e1.
    ///
    /// @returns A fully populated @c ImGui_ImplVulkan_InitInfo ready to pass to
    ///          @c ImGui_ImplVulkan_Init().
    /// @throws std::runtime_error if descriptor pool creation fails.
    ImGui_ImplVulkan_InitInfo getUIInitInfo();

    /// @brief Applies a pending offscreen resize if one was requested via
    ///        @c requestOffscreenResize().
    ///
    /// Clears the pending flag and calls @c resizeOffscreenResources(). Called
    /// at the very start of @c drawFrame() so the resize always occurs between
    /// frames, never mid-recording.
    void processPendingOffscreenResize();

private:
    // Init helpers

    /// @brief Calls all scene-resource init functions in dependency order.
    ///
    /// Order: UBO layout/pool/buffers/sets -> bindless layout/pool/set ->
    /// @c initMeshBuffers() -> @c createMeshDataDescriptors() ->
    /// @c createCullPipeline() -> @c createGraphicsPipeline() ->
    /// @c createGridPipeline() -> @c createOffscreenResources().
    void initSceneResources();

    /// @brief Recreates the swapchain in response to a window resize or vsync change.
    ///
    /// Spins on @c glfwGetFramebufferSize until the window is non-zero (handles
    /// minimisation), then delegates to @c VulkanDevice::recreateSwapchain().
    ///
    /// @param forVSync  @c true when called to change the present mode rather than
    ///                  to respond to a framebuffer resize.
    void recreateSwapchain(bool forVSync = false);

    // Scene UBO

    /// @brief Creates the descriptor set layout for the per-frame scene UBO.
    ///
    /// Single binding at set 2, location 0: @c eUniformBuffer, fragment stage.
    void createSceneUBOLayout();

    /// @brief Creates the descriptor pool backing the per-frame UBO descriptor sets.
    ///
    /// Allocates @c MAX_FRAMES_IN_FLIGHT uniform buffer descriptors.
    void createSceneUBOPool();

    /// @brief Allocates one host-visible, host-coherent UBO per frame-in-flight
    ///        and persistently maps each into @c m_uniformBuffersMapped.
    void createSceneUBOBuffers();

    /// @brief Allocates @c MAX_FRAMES_IN_FLIGHT descriptor sets from @c m_UBOPool
    ///        and writes each to point at its corresponding uniform buffer.
    void createSceneUBOSets();

    /// @brief Re-writes all UBO descriptor sets to point at the current uniform buffers.
    ///
    /// Called once after initial allocation and again if the buffers are recreated.
    void updateSceneUBOSets();

    // Bindless textures

    /// @brief Creates the bindless descriptor set layout for the global texture array.
    ///
    /// Single binding at set 1, location 0: @c eCombinedImageSampler array of
    /// @c MAX_TEXTURES, fragment stage. Flags: @c ePartiallyBound |
    /// @c eVariableDescriptorCount | @c eUpdateAfterBind.
    /// Layout flag: @c eUpdateAfterBindPool.
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

    // Mesh / geometry buffers

    /// @brief Allocates the global vertex buffer, index buffer, mesh data SSBO,
    ///        and indirect draw buffer, then uploads all currently loaded meshes.
    ///
    /// Sizes are fixed at @c MAX_VERTEX_BUFFER_BYTES and @c MAX_INDEX_BUFFER_BYTES.
    /// Calls @c uploadSingleMesh() for each entry in @c meshes.
    void initMeshBuffers();

    /// @brief Uploads the geometry for one mesh into the global buffers.
    ///
    /// Checks @c m_meshCache first. On a cache hit the geometry region is shared
    /// and no GPU upload is performed. On a miss the vertex and index data are
    /// staged and copied into the global buffers, and the region is registered in
    /// @c m_meshCache. In both cases a new @c GPUMeshData and
    /// @c DrawIndexedIndirectCommand entry are written for this mesh's ECS slot.
    ///
    /// @param meshIndex  0-based index into @c meshes.
    int uploadSingleMesh(uint32_t meshIndex);

    /// @brief Re-writes the @c GPUMeshData and @c DrawIndexedIndirectCommand
    ///        entries for an existing mesh slot without re-uploading geometry.
    ///
    /// Called after a transform, texture, or AABB change to keep the GPU-side
    /// data in sync without a full re-upload.
    ///
    /// @param meshIndex  0-based index into @c meshes.
    void updateMeshMetadata(uint32_t meshIndex);

    /// @brief Memcpys the current @c m_meshData vector into the persistently-mapped SSBO.
    ///
    /// No staging or synchronisation overhead - the SSBO is host-visible and
    /// host-coherent. Called every frame from @c perFrameUpdate() after
    /// @c refreshMeshData().
    void updateMeshDataSSBO();

    /// @brief Creates the descriptor set layout, pool, set, and writes the SSBO
    ///        binding for the per-mesh data read by the vertex shader.
    ///
    /// Single binding at set 0, location 0: @c eStorageBuffer, vertex stage.
    /// The SSBO (@c m_meshDataSSBO) must exist before calling this.
    void createMeshDataDescriptors();

    /// @brief Re-writes the @c m_meshDataDescriptorSet binding to point at
    ///        the current SSBO.
    ///
    /// Called if the SSBO is reallocated (e.g. mesh count grows past capacity).
    void updateMeshDataSet();

    /// @brief Syncs @c m_meshData transforms and AABBs from the live @c meshes array.
    ///
    /// Must be called before @c updateMeshDataSSBO() whenever any mesh transform
    /// has changed. Iterates all meshes and overwrites @c transform, @c aabbMin,
    /// and @c aabbMax in the CPU-side @c m_meshData mirror.
    void refreshMeshData();

    // GPU-driven culling compute pipeline

    /// @brief Creates the frustum-cull compute pipeline and all associated Vulkan objects.
    ///
    /// Creates:
    ///   - @c m_cullSetLayout - three @c eStorageBuffer bindings:
    ///     binding 0 = read-only @c ObjectData array, binding 1 = write
    ///     @c DrawIndexedIndirectCommand array, binding 2 = atomic draw count.
    ///   - @c m_cullPool / @c m_cullSet - pool and set backed by the three SSBOs.
    ///   - @c m_drawCountBuffer - host-visible, host-coherent @c uint32_t buffer
    ///     persistently mapped into @c m_drawCountMapped. Reset to 0 each frame.
    ///   - @c m_cullPipelineLayout - push constant range: @c FrustumPlanes, compute stage.
    ///   - @c m_cullPipeline - compute pipeline with entry point @c cullMain from @c Cull.spv.
    ///
    /// Must be called after @c initMeshBuffers() so the SSBOs exist when the
    /// descriptor set is written.
    void createCullPipeline();

    /// @brief Records the GPU frustum-cull compute pass into the current command buffer.
    ///
    /// Steps:
    ///   1. Resets @c m_drawCountBuffer to zero via the persistently-mapped pointer.
    ///   2. Emits host-write -> compute-read barriers on the count and indirect buffers.
    ///   3. Extracts frustum planes via @c extractFrustum() and pushes them as constants.
    ///   4. Binds the cull pipeline and descriptor set, dispatches
    ///      (numberOfMeshes + (threads - 1)) / threads) thread groups.
    ///   5. Emits compute-write -> indirect-read barriers on both buffers.
    ///
    /// Must be called before @c beginRendering() in @c recordOffscreenCommandBuffer().
    void recordCullPass();

    /// @brief Extracts six world-space frustum planes from a view-projection matrix.
    ///
    /// Uses the Gribb-Hartmann method. Each plane is normalised so that
    /// @c dot(n, p) + d can be used directly as a signed distance test.
    ///
    /// @note Reference: https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
    ///
    /// @param viewProj  The combined @c proj × view matrix for the current frame.
    /// @returns A @c FrustumPlanes struct with six normalised planes and
    ///          @c objectCount set to @c m_meshData.size().
    [[nodiscard]] FrustumPlanes extractFrustum(const glm::mat4& viewProj) const;

    // Graphics pipeline

    /// @brief Compiles and links the main scene graphics pipeline.
    ///
    /// Reads @c Base_Shader.spv (entry points @c vertMain / @c fragMain).
    /// Pipeline layout: set 0 = mesh SSBO, set 1 = bindless textures,
    /// set 2 = scene UBO, push constants = @c PushConstant (128 bytes max).
    /// Enables depth test/write, back-face culling (CCW front faces),
    /// dynamic viewport/scissor. Targets offscreen color + depth formats
    /// via @c VkPipelineRenderingCreateInfo.
    ///
    /// @throws std::runtime_error if the shader file cannot be read.
    void createGraphicsPipeline();

    // Grid pipeline

    /// @brief Compiles and links the infinite grid graphics pipeline.
    ///
    /// Reads @c Grid.spv (entry points @c vertMain / @c fragMain). Uses a
    /// fullscreen triangle (3 vertices, no VBO) and reconstructs the grid
    /// plane via ray-plane intersection in the fragment shader.
    /// Pipeline state: no vertex input, cull mode none, depth test enabled,
    /// depth write disabled, alpha blending enabled.
    /// Push constants = @c GridPushConstant.
    ///
    /// @throws std::runtime_error if the shader file cannot be read.
    void createGridPipeline();

    /// @brief Records the grid pass into the current frame's command buffer.
    ///
    /// Binds @c m_gridPipeline, pushes @c m_gridPC, binds the SceneUBO descriptor
    /// set, and issues @c vkCmdDraw(3, 1, 0, 0). Must be called inside an active
    /// @c beginRendering() block, after the opaque scene pass.
    void recordGridPass();

    // TAA (Temporal Anti-Aliasing)

    /// @brief Creates all TAA resources: motion-vector RT, history buffer,
    ///        resolved buffer, sampler, descriptor set layout/pool/set, and
    ///        the TAA resolve compute pipeline. Called from createOffscreenResources().
    /// @param width   Render target width in pixels.
    /// @param height  Render target height in pixels.
    void createTAAResources(uint32_t width, uint32_t height);

    /// @brief Destroys all TAA GPU resources. Called on resize or shutdown.
    void destroyTAAResources();

    /// @brief Records the TAA resolve compute pass, including barriers and the
    ///        history copy-back, into the current frame command buffer.
    void recordTAAPass();

    /// @brief Returns a Halton(2,3) jitter offset (NDC) for the given frame.
    ///
    /// The sequence has 16 entries and repeats. @p width / @p height convert
    /// from sub-pixel fractions to NDC magnitudes.
    [[nodiscard]] static glm::vec2 haltonJitter(uint32_t frameIndex, uint32_t width, uint32_t height);

    // Offscreen render target

    /// @brief Creates the offscreen color + depth images, sampler, and registers
    ///        the color image with ImGui via @c ImGui_ImplVulkan_AddTexture.
    ///
    /// The color image is immediately transitioned to @c eShaderReadOnlyOptimal
    /// so it can be sampled by ImGui before the first scene frame is rendered.
    ///
    /// @param width   Render target width in pixels.
    /// @param height  Render target height in pixels.
    void createOffscreenResources(uint32_t width, uint32_t height);

    /// @brief Destroys all offscreen resources and recreates them at the new size.
    ///
    /// Waits for GPU idle, removes and re-registers the ImGui descriptor,
    /// then calls @c createOffscreenResources().
    ///
    /// @param width   New render target width in pixels.
    /// @param height  New render target height in pixels.
    void resizeOffscreenResources(uint32_t width, uint32_t height);

    // Per-frame recording

    /// @brief Performs CPU-side per-frame updates before command recording.
    ///
    /// Calls @c refreshMeshData() to sync transforms, @c updateMeshDataSSBO()
    /// to push the SSBO to the GPU, and @c updateSceneUBO() to upload UBO data
    /// into the mapped buffers. Also runs any procedural animation callbacks.
    void perFrameUpdate();

    /// @brief Records the full offscreen scene pass into the current command buffer.
    ///
    /// Steps: transition offscreen images -> @c recordCullPass() -> @c beginRendering()
    /// -> bind graphics pipeline, viewport, scissor, buffers, descriptors, push
    /// constants -> @c drawIndexedIndirectCount() -> @c recordGridPass() ->
    /// @c endRendering() -> transition color image to @c eShaderReadOnlyOptimal.
    void recordOffscreenCommandBuffer();

    /// @brief Records the ImGui pass into the current frame's command buffer,
    ///        targeting the acquired swapchain image.
    ///
    /// Steps: transition swapchain image to @c eColorAttachmentOptimal ->
    /// @c beginRendering() -> @c ImGui_ImplVulkan_RenderDrawData() ->
    /// @c endRendering() -> handle multi-viewport platform windows ->
    /// transition swapchain image to @c ePresentSrcKHR.
    void recordImGuiCommandBuffer();

    // Compaction

    /// @brief Compacts a GPU buffer by removing a byte range using a temporary scratch buffer.
    ///
    /// Allocates a device-local scratch buffer sized to the tail (bytes after the
    /// hole), copies the tail out via single-time commands, then copies it back
    /// over the hole. Avoids the undefined behaviour of self-overlapping
    /// @c vkCmdCopyBuffer calls.
    ///
    /// @param buffer            The buffer to compact (vertex or index).
    /// @param removeByteOffset  Byte offset of the start of the region to remove.
    /// @param removeByteSize    Byte size of the region to remove.
    /// @param totalBytesFilled  [in/out] Running watermark; decremented by @p removeByteSize.
    void compactBuffer(vk::raii::Buffer& buffer,
                       vk::DeviceSize    removeByteOffset,
                       vk::DeviceSize    removeByteSize,
                       vk::DeviceSize&   totalBytesFilled);

    /// @brief Removes a geometry region from both global buffers and patches all
    ///        surviving mesh metadata to reflect the shifted offsets.
    ///
    /// Calls @c compactBuffer() for the vertex and index buffers, updates
    /// @c m_totalVertexCount / @c m_totalIndexCount, calls
    /// @c m_meshCache.patchAfterCompaction(), then patches every surviving entry
    /// in @c m_meshData and @c m_indirectCommands whose offsets fall after the
    /// removed region.
    ///
    /// @param removedVertexOffset  @c vertexOffset of the region being removed.
    /// @param removedFirstIndex    @c firstIndex   of the region being removed.
    /// @param removedVertexCount   Number of vertices in the removed region.
    /// @param removedIndexCount    Number of indices  in the removed region.
    void compactGeometry(uint32_t removedVertexOffset, uint32_t removedFirstIndex,
                         uint32_t removedVertexCount,  uint32_t removedIndexCount);

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
    /// @param image        The image to view.
    /// @param format       Must match the image's creation format.
    /// @param aspectFlags  @c eColor or @c eDepth.
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

    /// @brief Copies @p size bytes from @p src to @p dst via a single-time command buffer.
    ///
    /// @param src   Source buffer (must have @c eTransferSrc usage).
    /// @param dst   Destination buffer (must have @c eTransferDst usage).
    /// @param size  Number of bytes to copy.
    void copyBuffer(vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size);

    /// @brief Emits a @c pipelineBarrier2 image transition into the active frame
    ///        command buffer (not a single-time submit).
    ///
    /// @param image      The image to transition.
    /// @param oldLayout  Layout the image is currently in.
    /// @param newLayout  Target layout.
    /// @param srcAccess  Access types that must complete before the barrier.
    /// @param dstAccess  Access types that must wait until after the barrier.
    /// @param srcStage   Pipeline stage(s) producing @p srcAccess.
    /// @param dstStage   Pipeline stage(s) consuming @p dstAccess.
    /// @param aspect     Image aspect mask (@c eColor or @c eDepth).
    /// @param mipLevels  Number of mip levels covered by the barrier.
    void transitionImageInFrame(vk::Image image,
                                vk::ImageLayout oldLayout,        vk::ImageLayout newLayout,
                                vk::AccessFlags2 srcAccess,       vk::AccessFlags2 dstAccess,
                                vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage,
                                vk::ImageAspectFlags aspect,      uint32_t mipLevels);

    /// @brief Allocates a primary command buffer from @c m_uploadCommandPool
    ///        and begins recording with @c eOneTimeSubmit.
    ///
    /// Always pair with @c endSingleTimeCommands().
    ///
    /// @returns A heap-allocated @c vk::raii::CommandBuffer in the recording state.
    [[nodiscard]] std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands();

    /// @brief Ends recording, submits to the graphics queue, waits for idle,
    ///        and frees the command buffer.
    ///
    /// @param cmd  The command buffer returned by @c beginSingleTimeCommands().
    void endSingleTimeCommands(vk::raii::CommandBuffer& cmd);

    /// @brief Finds the index of a device memory type satisfying @p typeFilter and @p props.
    ///
    /// @param typeFilter  Bitmask of acceptable memory type indices from
    ///                    @c VkMemoryRequirements::memoryTypeBits.
    /// @param props       Required property flags (e.g. @c eDeviceLocal).
    /// @returns The matching memory type index.
    /// @throws std::runtime_error if no suitable memory type exists.
    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter,
                                          vk::MemoryPropertyFlags props) const;

    /// @brief Returns the best depth format supported by the physical device.
    ///
    /// Tests @c eD32Sfloat -> @c eD32SfloatS8Uint -> @c eD24UnormS8Uint in order,
    /// requiring @c eDepthStencilAttachment optimal tiling support.
    ///
    /// @returns The first supported depth (or depth-stencil) format.
    [[nodiscard]] vk::Format findDepthFormat() const;

    /// @brief Finds the first format in @p candidates supporting @p tiling and @p features.
    ///
    /// @param candidates  Ordered list of formats to test.
    /// @param tiling      @c eLinear or @c eOptimal.
    /// @param features    Required @c VkFormatFeatureFlags.
    /// @returns The first matching format.
    /// @throws std::runtime_error if no candidate satisfies the requirements.
    [[nodiscard]] vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates,
                                                  vk::ImageTiling tiling,
                                                  vk::FormatFeatureFlags features) const;

    /// @brief Reads a SPIR-V binary from @c <cwd>/Shaders/<fileName>.
    ///
    /// @param fileName  File name including extension (e.g. @c "Base_Shader.spv").
    /// @returns The raw binary as a @c std::vector<char>.
    /// @throws std::runtime_error if the file cannot be opened.
    [[nodiscard]] static std::vector<char> readShader(const std::string& fileName);

    /// @brief Wraps a SPIR-V binary in a @c VkShaderModule.
    ///
    /// @param code  SPIR-V binary as returned by @c readShader(). Size must be
    ///              a multiple of 4 bytes.
    /// @returns A @c vk::raii::ShaderModule valid until destroyed.
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;

private:
    // Core
    gcep::rhi::SwapchainDesc m_swapchainDesc;

    /// @brief Wraps the Vulkan instance, physical device, logical device, and swapchain.
    VulkanDevice m_device;

    bool                     m_framebufferResized = false;

    /// @brief ImGui-owned descriptor pool (11 types × 1000 sets).
    VkDescriptorPool         m_descriptorPoolImGui = VK_NULL_HANDLE;

    /// @brief Persistent command pool for @c beginSingleTimeCommands() uploads.
    vk::raii::CommandPool    m_uploadCommandPool   = nullptr;

    /// @brief Swapchain image format cached at init for pipeline and ImGui configuration.
    vk::Format               m_swapchainFormat     = vk::Format::eUndefined;

    glm::vec4           m_clearColor{ 0.1f, 0.1f, 0.1f, 1.0f };
    UniformBufferObject m_cameraUBO;
    SceneUBO            m_sceneUBO;
    float               m_shininess = 32.0f;
    GridPushConstant    m_gridPC;
    InitInfos           m_initInfos;
    ECS::Registry*       m_ECSRegistry = nullptr;
    bool                simulationStarted = false;

    std::chrono::high_resolution_clock::time_point m_startTime = std::chrono::high_resolution_clock::now();

    static constexpr int      MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr uint32_t MAX_MESHES           = 4096;

    // Scene UBO - set 2, binding 0
    std::vector<vk::raii::Buffer>        m_uniformBuffers;
    std::vector<vk::raii::DeviceMemory>  m_uniformBuffersMemory;

    /// @brief Persistently-mapped pointers into @c m_uniformBuffersMemory.
    ///        Written each frame by @c updateSceneUBO().
    std::vector<void*>                   m_uniformBuffersMapped;
    vk::raii::DescriptorSetLayout        m_UBOLayout = nullptr;
    vk::raii::DescriptorPool             m_UBOPool   = nullptr;
    std::vector<vk::raii::DescriptorSet> m_UBOSets;

    // Bindless texture array - set 1, binding 0
    vk::raii::DescriptorSetLayout m_bindlessLayout = nullptr;
    vk::raii::DescriptorPool      m_bindlessPool   = nullptr;
    vk::raii::DescriptorSet       m_bindlessSet    = nullptr;

    // Mesh data SSBO - set 0, binding 0
    vk::raii::DescriptorSetLayout m_meshDataDescriptorSetLayout = nullptr;
    vk::raii::DescriptorPool      m_meshDataDescriptorPool      = nullptr;
    vk::raii::DescriptorSet       m_meshDataDescriptorSet       = nullptr;
    vk::raii::Buffer              m_meshDataSSBO                = nullptr;
    vk::raii::DeviceMemory        m_meshDataSSBOMemory          = nullptr;

    /// @brief Persistently-mapped pointer into @c m_meshDataSSBOMemory.
    ///        Written by @c updateMeshDataSSBO(); never unmapped.
    void* m_meshDataSSBOMapped = nullptr;

    // Global geometry buffers
    static constexpr vk::DeviceSize MAX_VERTEX_BUFFER_BYTES = 128 * 1024 * 1024; ///< 512 MB vertex budget.
    static constexpr vk::DeviceSize MAX_INDEX_BUFFER_BYTES  = 64 * 1024 * 1024; ///< 256 MB index budget.

    vk::raii::Buffer       m_globalVertexBuffer       = nullptr;
    vk::raii::DeviceMemory m_globalVertexBufferMemory = nullptr;
    vk::raii::Buffer       m_globalIndexBuffer        = nullptr;
    vk::raii::DeviceMemory m_globalIndexBufferMemory  = nullptr;

    vk::DeviceSize m_vertexBytesFilled = 0; ///< Current byte watermark in @c m_globalVertexBuffer.
    vk::DeviceSize m_indexBytesFilled  = 0; ///< Current byte watermark in @c m_globalIndexBuffer.
    uint32_t       m_totalVertexCount  = 0; ///< Total uploaded vertex count across all meshes.
    uint32_t       m_totalIndexCount   = 0; ///< Total uploaded index  count across all meshes.

    // Indirect draw buffer + culling

    /// @brief Host-visible indirect draw command buffer written by the cull shader.
    vk::raii::Buffer       m_indirectBuffer       = nullptr;
    vk::raii::DeviceMemory m_indirectBufferMemory = nullptr;

    /// @brief Persistently-mapped pointer into @c m_indirectBufferMemory.
    void*          m_indirectBufferMapped   = nullptr;
    vk::DeviceSize m_indirectBufferCapacity = 0;

    /// @brief Host-visible atomic draw count written by the cull shader.
    vk::raii::Buffer       m_drawCountBuffer       = nullptr;
    vk::raii::DeviceMemory m_drawCountBufferMemory = nullptr;

    /// @brief Persistently-mapped pointer into @c m_drawCountBufferMemory.
    ///        Reset to 0 each frame before the cull dispatch.
    void* m_drawCountMapped = nullptr;

    /// @brief Cached draw count read from @c m_drawCountBuffer after each frame.
    ///        Used only for editor display. Lags by one frame.
    uint32_t m_drawCount = 0;

    vk::raii::Pipeline            m_cullPipeline       = nullptr;
    vk::raii::PipelineLayout      m_cullPipelineLayout = nullptr;
    vk::raii::DescriptorSetLayout m_cullSetLayout      = nullptr;
    vk::raii::DescriptorPool      m_cullPool           = nullptr;
    vk::raii::DescriptorSet       m_cullSet            = nullptr;

    // Pipelines
    vk::raii::PipelineLayout m_pipelineLayout    = nullptr;
    vk::raii::Pipeline       m_graphicsPipeline  = nullptr;
    vk::raii::Pipeline       m_gridPipeline      = nullptr;
    vk::raii::PipelineLayout m_gridPipelineLayout = nullptr;

    // Offscreen render target
    vk::Extent2D           m_offscreenExtent{};
    vk::raii::Image        m_offscreenImage            = nullptr;
    vk::raii::DeviceMemory m_offscreenImageMemory      = nullptr;
    vk::raii::ImageView    m_offscreenImageView        = nullptr;
    vk::raii::Image        m_offscreenDepthImage       = nullptr;
    vk::raii::DeviceMemory m_offscreenDepthImageMemory = nullptr;
    vk::raii::ImageView    m_offscreenDepthImageView   = nullptr;
    vk::raii::Sampler      m_offscreenSampler          = nullptr;

    /// @brief Registered via @c ImGui_ImplVulkan_AddTexture; displayed by @c ImGui::Image().
    VkDescriptorSet m_imguiTextureDescriptor = VK_NULL_HANDLE;

    bool     m_offscreenResizePending = false;
    uint32_t m_pendingOffscreenWidth  = 0;
    uint32_t m_pendingOffscreenHeight = 0;

    // TAA (Temporal Anti-Aliasing)

    float   m_taaBlendAlpha = 0.10f; ///< Current-frame blend weight.
    uint32_t m_taaFrameIndex = 0;    ///< Monotonically increasing frame counter for jitter.

    /// @brief Motion-vector render target.
    ///        Written by the scene pass as a second colour attachment.
    vk::raii::Image        m_motionImage            = nullptr;
    vk::raii::DeviceMemory m_motionImageMemory      = nullptr;
    vk::raii::ImageView    m_motionImageView        = nullptr;
    vk::Format             m_motionVectorsFormat    = vk::Format::eR16G16Sfloat;

    /// @brief History buffer: accumulates the previous resolved frame.
    vk::raii::Image        m_taaHistoryImage        = nullptr;
    vk::raii::DeviceMemory m_taaHistoryImageMemory  = nullptr;
    vk::raii::ImageView    m_taaHistoryImageView    = nullptr;

    /// @brief Resolved output written by the TAA compute shader.
    ///        Copied back to historyImage and bound as the ImGui texture.
    vk::raii::Image        m_taaResolvedImage       = nullptr;
    vk::raii::DeviceMemory m_taaResolvedImageMemory = nullptr;
    vk::raii::ImageView    m_taaResolvedImageView   = nullptr;

    /// @brief Bilinear sampler shared by the TAA resolve shader.
    vk::raii::Sampler      m_taaSampler             = nullptr;

    // TAA descriptor set / pipeline
    vk::raii::DescriptorSetLayout m_taaSetLayout  = nullptr;
    vk::raii::DescriptorPool      m_taaPool       = nullptr;
    vk::raii::DescriptorSet       m_taaSet        = nullptr;
    vk::raii::PipelineLayout      m_taaPipelineLayout = nullptr;
    vk::raii::Pipeline            m_taaPipeline       = nullptr;

    /// @brief Previous-frame (unjittered) view-projection matrix for motion vectors.
    glm::mat4 m_prevViewProj = glm::mat4(1.0f);

    // CPU-side scene mirrors

    /// @brief CPU mirror of the mesh data SSBO. Rebuilt each frame by @c refreshMeshData().
    std::vector<GPUMeshData>                    m_meshData;

    /// @brief CPU mirror of the indirect command buffer. Patched on spawn/remove.
    std::vector<vk::DrawIndexedIndirectCommand> m_indirectCommands;

    std::vector<Mesh> meshes;        ///< All live mesh entities.
    MeshCache         m_meshCache;   ///< OBJ path -> geometry region, ref-counted.

    /// @brief OBJ path -> texture cache entry (@c shared_ptr<TextureData> + bindless slot).
    ///        Declared before @c m_device so it destructs first, releasing GPU texture memory.
    std::unordered_map<std::string, TextureCacheEntry> m_textureCache;
};

} // Namespace gcep::rhi::vulkan

namespace std
{
/// @brief Hash specialisation for @c gcep::rhi::vulkan::Vertex enabling its use
///        as an @c unordered_map key during OBJ vertex deduplication.
///
/// Combines the GLM hashes of @c pos, @c normal, and @c texCoord using the
/// standard hash-combine XOR/shift pattern. Requires @c GLM_ENABLE_EXPERIMENTAL
/// and @c \<glm/gtx/hash.hpp\>.
template<> struct hash<gcep::rhi::vulkan::Vertex>
{
    size_t operator()(gcep::rhi::vulkan::Vertex const& v) const
    {
        return ((hash<glm::vec3>()(v.pos) ^
                (hash<glm::vec3>()(v.normal) << 1)) >> 1) ^
                (hash<glm::vec2>()(v.texCoord) << 1);
    }
};

} // Namespace std
