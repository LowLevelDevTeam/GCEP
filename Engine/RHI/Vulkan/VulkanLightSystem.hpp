#pragma once

/// @file VulkanLightSystem.hpp
/// @brief CPU-side light data types, GPU-mapped buffer structs, and the
///        @c LightSystem class that owns all Vulkan resources for point
///        and spot lights.
///
/// @par Architecture overview
/// The light system is a self-contained subsystem that plugs directly into
/// @c VulkanRHI. It owns:
///   - A persistently-mapped, host-visible/coherent SSBO containing packed
///     @c GPULight entries (one per live light, up to @c MAX_LIGHTS).
///   - A host-visible/coherent metadata UBO (@c LightMetaUBO) that carries
///     the live light count so the lighting compute shader can early-exit.
///   - A compute pipeline (@c Compute_Lighting.spv, entry @c compMain) that
///     reads the G-buffer-like offscreen color + depth produced by the scene
///     pass, the light SSBO, and writes a shaded output into an RGBA16F
///     storage image. The result is fed into the TAA resolve stage as the
///     new "current color".
///   - All descriptor set layouts, pools, and sets needed for the above.
///
/// @par Descriptor set layout (set 3 - lighting compute)
///   - Binding 0  @c eStorageBuffer    - @c GPULight SSBO (read-only)
///   - Binding 1  @c eUniformBuffer    - @c LightMetaUBO (read-only)
///   - Binding 2  @c eCombinedImageSampler - offscreen color (read)
///   - Binding 3  @c eCombinedImageSampler - offscreen depth (read)
///   - Binding 4  @c eStorageImage     - lit output (write)
///
/// @par Push constants (@c LightingPushConstants, compute stage)
///   - @c invViewProj  - inverse view-projection for depth-buffer ray reconstruction
///   - @c cameraPos    - world-space eye position for specular computation
///   - @c screenSize   - pixel dimensions of the offscreen render target
///
/// @par Integration in @c VulkanRHI::recordOffscreenCommandBuffer()
/// Insert @c recordLightingPass() *after* @c endRendering() and *before*
/// @c recordTAAPass(). The lighting pass reads the scene color and depth as
/// sampled images and writes the lit result into @c m_litImage (storage),
/// then transitions it to @c eShaderReadOnlyOptimal for TAA.
///
/// @par Thread safety
///   All public methods must be called from the render thread. No internal
///   synchronisation is provided.

// Externals
#include <vulkan/vulkan_raii.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

// STL
#include <array>
#include <cstdint>
#include <vector>

#include <Engine/RHI/Vulkan/VulkanDevice.hpp>

namespace gcep::rhi::vulkan
{

// Constants

/// @brief Hard upper bound on the number of simultaneous lights.
///
/// Governs the size of @c GPULightBuffer and the SSBO allocation.
/// Must match @c MAX_LIGHTS in @c Compute_Lighting.slang.
static constexpr uint32_t MAX_LIGHTS = 256u;

// Light type tag

/// @brief Identifies the light model encoded in a @c GPULight entry.
///
/// The value is stored in @c GPULight::type and is tested by the Slang shader
/// to branch between the point-light and spot-light attenuation models.
enum class LightType : uint32_t
{
    Point = 0u, ///< Isotropic point light with inverse-square attenuation.
    Spot  = 1u, ///< Cone-shaped spot light with inner/outer cutoff angles.
};

// CPU-side light descriptors

/// @brief CPU-side description of a point light.
///
/// A point light radiates equally in all directions. Attenuation follows the
/// physically-based inverse-square law, clamped to a maximum influence radius
/// of @c radius world-space units.
///
/// @note Keep fields in sync with @c GPULight layout offsets.
struct PointLight
{
    glm::vec3 position  = { 0.0f, 0.0f, 0.0f }; ///< World-space origin.
    glm::vec3 color     = { 1.0f, 1.0f, 1.0f }; ///< Linear-space RGB color.
    float     intensity = 1.0f;                  ///< Luminous intensity scalar (lm/sr equivalent).
    float     radius    = 10.0f;                 ///< Maximum influence radius in world units.
};

/// @brief CPU-side description of a spot light.
///
/// A spot light illuminates a cone defined by @c innerCutoffDeg and
/// @c outerCutoffDeg (both in degrees, measured from the central axis).
/// Between the two angles the intensity is smoothly attenuated using a
/// cosine-based falloff. Beyond @c radius the light has no influence.
///
/// @note Keep fields in sync with @c GPULight layout offsets.
struct SpotLight
{
    glm::vec3 position       = { 0.0f,  1.0f, 0.0f }; ///< World-space origin.
    glm::vec3 direction      = { 0.0f, -1.0f, 0.0f }; ///< Unit vector pointing down the cone axis.
    glm::vec3 color          = { 1.0f,  1.0f, 1.0f }; ///< Linear-space RGB color.
    float     intensity      = 1.0f;                   ///< Luminous intensity scalar.
    float     radius         = 20.0f;                  ///< Maximum influence radius in world units.
    float     innerCutoffDeg = 15.0f;                  ///< Full-brightness cone half-angle (degrees).
    float     outerCutoffDeg = 30.0f;                  ///< Zero-brightness cone half-angle (degrees).
    bool      showGizmo      = true;                   ///< When false the cone wireframe gizmo is hidden.
};

// ============================================================================
// GPU-mapped structs (std140 / explicit padding)
// ============================================================================

/// @brief Tightly-packed GPU light entry.  Laid out as std430 so the Slang
///        @c StructuredBuffer can address it with @c sizeof == 64 bytes.
///
/// Both @c PointLight and @c SpotLight are packed into a single union-like
/// structure. Fields unused by a given light type are zeroed on upload.
struct alignas(16) GPULight
{
    glm::vec3 position;   ///< World-space position.
    uint32_t  type;       ///< @c LightType cast to uint.
    glm::vec3 color;      ///< Linear-space RGB, pre-multiplied by intensity.
    float     intensity;  ///< Raw intensity; used separately for HDR bloom budgeting.
    glm::vec3 direction;  ///< Normalized cone axis (spot lights only; zero for point).
    float     radius;     ///< Influence radius; fragment discards light beyond this.
    float     innerCos;   ///< cos(innerCutoffDeg) - full-brightness threshold.
    float     outerCos;   ///< cos(outerCutoffDeg) - zero-brightness threshold.
    float     _pad[2];    ///< Explicit padding to 64 bytes.
};
static_assert(sizeof(GPULight) == 64, "GPULight must be 64 bytes for SSBO stride");

/// @brief Metadata uniform uploaded alongside the light SSBO.
///
/// The Slang compute shader reads @c lightCount to early-exit per-thread
/// light loops, avoiding dead iterations over unpopulated slots.
struct alignas(16) LightMetaUBO
{
    uint32_t lightCount = 0u; ///< Number of active entries in the @c GPULight SSBO.
    float    _pad[3]    = {}; ///< Explicit std140 padding to 16 bytes.
};
static_assert(sizeof(LightMetaUBO) == 16, "LightMetaUBO must be 16 bytes (std140)");

/// @brief Push-constant block passed to the lighting compute shader.
///
/// Fits inside the guaranteed 128-byte push-constant budget.
struct LightingPushConstants
{
    glm::mat4 invViewProj; ///< Inverse of proj * view; used to reconstruct world position from depth.
    glm::vec3 cameraPos;   ///< World-space eye position for specular computation.
    float     _pad0;       ///< Explicit padding.
    glm::vec2 screenSize;  ///< Render-target dimensions in pixels.
    float     _pad1[2];    ///< Explicit padding to 16-byte boundary.
};
static_assert(sizeof(LightingPushConstants) == 96, "LightingPushConstants must be 96 bytes");

// ============================================================================
// LightSystem
// ============================================================================

/// @brief Manages the GPU light SSBO, lighting compute pipeline, and the
///        lit output image that replaces the raw scene color for TAA.
///
/// @par Ownership
///   @c LightSystem does not own the offscreen color / depth images - it
///   receives @c vk::ImageView handles from @c VulkanRHI and re-writes its
///   descriptor set whenever those images are recreated (e.g. on resize).
///
/// @par Usage pattern
///   @code
///   // One-time init (after offscreen resources exist):
///   m_lightSystem.init(m_device, width, height, offscreenColorView,
///                      offscreenDepthView, depthFormat, offscreenColorFormat);
///
///   // Each frame (before recordOffscreenCommandBuffer):
///   m_lightSystem.updateLights(pointLights, spotLights);
///
///   // Inside recordOffscreenCommandBuffer, after endRendering():
///   m_lightSystem.recordLightingPass(cmd, invViewProj, cameraPos, width, height);
///
///   // On resize (after resizeOffscreenResources):
///   m_lightSystem.onResize(width, height, newColorView, newDepthView);
///
///   // Cleanup handled automatically by RAII destructors.
///   @endcode
class LightSystem
{
public:
    /// @brief Default constructor - no Vulkan objects are created yet.
    LightSystem() = default;

    /// @brief Initialises all Vulkan resources owned by the light system.
    ///
    /// Creates the SSBO, meta UBO, lit output image, descriptor set layout /
    /// pool / set, and the lighting compute pipeline.  Must be called once
    /// after the offscreen resources exist.
    ///
    /// @param device            Reference to the engine's @c VulkanDevice.
    /// @param width             Offscreen render target width in pixels.
    /// @param height            Offscreen render target height in pixels.
    /// @param colorView         @c VkImageView of the offscreen RGBA16F color image.
    /// @param depthView         @c VkImageView of the offscreen D32 depth image.
    /// @param depthFormat       Vulkan format of the depth image (for the sampler).
    /// @param colorFormat       Vulkan format of the color image (for the lit output).
    /// @param uploadPool        Command pool used for single-time upload commands.
    void init(VulkanDevice&        device,
              uint32_t             width,
              uint32_t             height,
              vk::ImageView        colorView,
              vk::ImageView        depthView,
              vk::Format           depthFormat,
              vk::Format           colorFormat,
              vk::raii::CommandPool& uploadPool);

    /// @brief Destroys the lit output image and recreates it at the new size,
    ///        then re-writes the descriptor set with the new image views.
    ///
    /// Safe to call after @c VulkanRHI::resizeOffscreenResources() completes.
    ///
    /// @param width      New render target width in pixels.
    /// @param height     New render target height in pixels.
    /// @param colorView  New offscreen color image view.
    /// @param depthView  New offscreen depth image view.
    void onResize(uint32_t      width,
                  uint32_t      height,
                  vk::ImageView colorView,
                  vk::ImageView depthView);

    /// @brief Uploads point and spot light data to the GPU SSBO and meta UBO.
    ///
    /// Merges both lists into a single flat @c GPULight array (point lights
    /// first, then spot lights), clamps the total to @c MAX_LIGHTS, and
    /// memcpys into the persistently-mapped device-visible buffers.
    /// Call once per frame before @c recordLightingPass().
    void updateLights();

    void addPointLight(glm::vec3 pos);
    void addSpotLight(glm::vec3 pos);

    std::vector<PointLight>& getPointLights() { return m_pointLights; }
    std::vector<SpotLight>& getSpotLights()   { return m_spotLights; }

    /// @brief Records the lighting compute pass into @p cmd.
    ///
    /// Steps:
    ///   1. Emits pre-dispatch barriers: color + depth to @c eShaderReadOnlyOptimal,
    ///      lit-output to @c eGeneral (storage write).
    ///   2. Binds the lighting compute pipeline and descriptor set.
    ///   3. Pushes @c LightingPushConstants (invViewProj, cameraPos, screenSize).
    ///   4. Dispatches (width+7)/8 * (height+7)/8 thread groups.
    ///   5. Emits post-dispatch barrier: lit-output to @c eShaderReadOnlyOptimal
    ///      for subsequent TAA sampling.
    ///
    /// @param cmd         The active frame command buffer (not recording-begin).
    /// @param invViewProj Inverse view-projection matrix for the current frame.
    /// @param cameraPos   World-space camera position.
    /// @param width       Render-target width in pixels.
    /// @param height      Render-target height in pixels.
    void recordLightingPass(vk::CommandBuffer cmd,
                            const glm::mat4&  invViewProj,
                            const glm::vec3&  cameraPos,
                            uint32_t          width,
                            uint32_t          height);

    // Accessors

    /// @brief Returns the @c VkImageView of the lit output image.
    ///
    /// @c VulkanRHI should swap its TAA "currentColor" binding to this view
    /// so TAA resolves the lit result instead of the raw scene color.
    [[nodiscard]] vk::ImageView litImageView() const noexcept { return *m_litImageView; }

    /// @brief Returns the @c VkImage handle of the lit output.
    ///
    /// Used by @c VulkanRHI when it needs to insert additional barriers that
    /// cross subsystem boundaries (e.g. the TAA copy-back).
    [[nodiscard]] vk::Image litImage() const noexcept { return *m_litImage; }

    /// @brief Returns the number of lights uploaded in the most recent @c updateLights() call.
    [[nodiscard]] uint32_t activeLightCount() const noexcept { return m_activeLightCount; }

private:
    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    /// @brief Allocates a @c VkBuffer + @c VkDeviceMemory pair.
    void createBuffer(vk::DeviceSize          size,
                      vk::BufferUsageFlags    usage,
                      vk::MemoryPropertyFlags props,
                      vk::raii::Buffer&       outBuffer,
                      vk::raii::DeviceMemory& outMemory);

    /// @brief Allocates a @c VkImage + @c VkDeviceMemory pair.
    void createImage(uint32_t                width,
                     uint32_t                height,
                     vk::Format              format,
                     vk::ImageUsageFlags     usage,
                     vk::raii::Image&        outImage,
                     vk::raii::DeviceMemory& outMemory);

    /// @brief Creates a 2-D, single-mip @c VkImageView for @p image.
    [[nodiscard]] vk::raii::ImageView createImageView(vk::raii::Image&     image,
                                                      vk::Format           format,
                                                      vk::ImageAspectFlags aspect);

    /// @brief Queries the physical device for a suitable memory type index.
    [[nodiscard]] uint32_t findMemoryType(uint32_t                typeFilter,
                                          vk::MemoryPropertyFlags props) const;

    /// @brief One-shot command submit helper (mirrors VulkanRHI pattern).
    [[nodiscard]] std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands();

    /// @brief Ends + submits + waits for the one-shot command buffer.
    void endSingleTimeCommands(vk::raii::CommandBuffer& cmd);

    /// @brief Creates the lit output image and its view, transitions to
    ///        @c eShaderReadOnlyOptimal, and creates the nearest-clamp sampler.
    void createLitOutputImage(uint32_t width, uint32_t height);

    /// @brief Destroys the lit output image, view, and sampler.
    void destroyLitOutputImage();

    /// @brief Creates the GPU light SSBO and the @c LightMetaUBO, both
    ///        persistently mapped.
    void createLightBuffers();

    /// @brief Creates the descriptor set layout, pool, and set, then writes
    ///        all bindings for the current offscreen views and light buffers.
    ///
    /// @param colorView  Current offscreen color image view.
    /// @param depthView  Current offscreen depth image view.
    void createDescriptors(vk::ImageView colorView, vk::ImageView depthView);

    /// @brief Re-writes the descriptor set in place with updated image views.
    ///
    /// Called from @c onResize() after the offscreen images have been replaced.
    ///
    /// @param colorView  New offscreen color image view.
    /// @param depthView  New offscreen depth image view.
    void updateDescriptors(vk::ImageView colorView, vk::ImageView depthView);

    /// @brief Creates the compute pipeline and its layout from
    ///        @c Compute_Lighting.spv.
    void createLightingPipeline();

    /// @brief Reads a SPIR-V binary from <cwd>/Assets/Shaders/<fileName>.
    [[nodiscard]] static std::vector<char> readShader(const std::string& fileName);

    // Members

    std::vector<PointLight> m_pointLights;
    std::vector<SpotLight>  m_spotLights;

    VulkanDevice*          m_device     = nullptr;  ///< Non-owning pointer to the engine device.
    vk::raii::CommandPool* m_uploadPool = nullptr;  ///< Non-owning pointer to the upload pool.
    bool                   m_initialised = false;   ///< Set to true after init() completes successfully.

    // Light data buffers
    vk::raii::Buffer       m_lightSSBO       = nullptr; ///< Device-local SSBO; MAX_LIGHTS * GPULight.
    vk::raii::DeviceMemory m_lightSSBOMemory = nullptr;
    void*                  m_lightSSBOMapped = nullptr; ///< Persistently-mapped host pointer.

    vk::raii::Buffer       m_lightMetaUBO       = nullptr; ///< Host-visible UBO for LightMetaUBO.
    vk::raii::DeviceMemory m_lightMetaUBOMemory = nullptr;
    void*                  m_lightMetaMapped    = nullptr; ///< Persistently-mapped host pointer.

    // Lit output image (produced by the lighting compute, fed into TAA)
    vk::raii::Image        m_litImage       = nullptr;
    vk::raii::DeviceMemory m_litImageMemory = nullptr;
    vk::raii::ImageView    m_litImageView   = nullptr;
    vk::raii::Sampler      m_litSampler     = nullptr; ///< Nearest-clamp sampler for the lit output.

    vk::Format m_colorFormat = vk::Format::eR16G16B16A16Sfloat; ///< Cached lit image format.

    // Descriptors
    vk::raii::DescriptorSetLayout m_setLayout  = nullptr;
    vk::raii::DescriptorPool      m_pool       = nullptr;
    vk::raii::DescriptorSet       m_set        = nullptr;
    vk::raii::Sampler             m_inputSampler = nullptr; ///< Nearest sampler for reading scene images.

    // Pipeline
    vk::raii::PipelineLayout m_pipelineLayout = nullptr;
    vk::raii::Pipeline       m_pipeline       = nullptr;

    uint32_t m_activeLightCount = 0u; ///< Last uploaded light count; reported to editor.
};

} // Namespace gcep::rhi::vulkan
