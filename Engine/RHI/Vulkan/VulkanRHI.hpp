#pragma once

#include <ECS/headers/registry.hpp>
#include <Engine/RHI/Mesh/Mesh.hpp>
#include <Engine/RHI/Mesh/MeshCache.hpp>
#include <Engine/RHI/RHI.hpp>
#include <Engine/RHI/Vulkan/VulkanDevice.hpp>
#include <Engine/RHI/Vulkan/VulkanLightSystem.hpp>
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
/// Private members and helpers are split across @c VulkanRHI_private_*.hpp
/// files included at the bottom of the class to keep this header readable.
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

    void spawnLight(LightType type, ECS::EntityID id, glm::vec3 pos = {0.0f, 0.0f, 0.0f});

    /// @brief Destroys the mesh at @p meshIndex and compacts the GPU buffers.
    ///
    /// Releases the geometry reference in @c m_meshCache. If this was the last
    /// entity using that OBJ, physically compacts the global vertex and index
    /// buffers via @c compactGeometry(). Destroys the ECS entity, erases from
    /// @c meshes / @c m_meshData / @c m_indirectCommands, and rewrites the GPU
    /// SSBO and indirect buffer for all surviving slots.
    ///
    /// @param meshIndex  0-based index into @c meshes. Must be < @c meshes.size().
    void removeMesh(ECS::EntityID id);

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

    void setSimulationStarted(bool started);

    void setRegistry(ECS::Registry* registry);

    // Queries

    /// @brief Returns a reference to the renderer light system.
    [[nodiscard]] LightSystem& getLightSystem() noexcept { return m_lightSystem; }

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
    void initSceneResources();

    /// @brief Recreates the swapchain in response to a window resize or vsync change.
    ///
    /// Spins on @c glfwGetFramebufferSize until the window is non-zero (handles
    /// minimisation), then delegates to @c VulkanDevice::recreateSwapchain().
    ///
    /// @param forVSync  @c true when called to change the present mode rather than
    ///                  to respond to a framebuffer resize.
    void recreateSwapchain(bool forVSync = false);

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

private:
    // Core
    gcep::rhi::SwapchainDesc m_swapchainDesc;

    /// @brief Wraps the Vulkan instance, physical device, logical device, and swapchain.
    VulkanDevice m_device;

    LightSystem m_lightSystem;

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
    ECS::Registry*      m_ECSRegistry = nullptr;
    bool                simulationStarted = false;

    std::chrono::high_resolution_clock::time_point m_startTime = std::chrono::high_resolution_clock::now();

    static constexpr int      MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr uint32_t MAX_MESHES           = 4096;

    // Private subsystem declarations
    #include "VulkanRHI_private_Descriptors.hpp"
    #include "VulkanRHI_private_Geometry.hpp"
    #include "VulkanRHI_private_Pipelines.hpp"
    #include "VulkanRHI_private_Gizmos.hpp"
    #include "VulkanRHI_private_Offscreen.hpp"
    #include "VulkanRHI_private_TAA.hpp"
    #include "VulkanRHI_private_Helpers.hpp"
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
