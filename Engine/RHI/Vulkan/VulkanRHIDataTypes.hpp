#pragma once

// Externals
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

// STD
#include <cstdint>

#include <ECS/headers/registry.hpp>

namespace gcep::rhi::vulkan 
{
class VulkanRHI;
class Mesh;
/// @brief Interleaved vertex layout: position (vec3), vertex color or normal (vec3),
///        texture coordinate (vec2). Matches the SPIR-V input layout of @c Base_Shader.spv.
struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;

    /// @brief Returns the binding description for a single interleaved vertex buffer
    ///        bound at binding point 0 with per-vertex input rate.
    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    /// @brief Returns the attribute descriptions for @c pos (location 0),
    ///        @c normal (location 1), and @c texCoord (location 2).
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        return
        {{
            {0, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, pos))},
            {1, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, normal))},
            {2, 0, vk::Format::eR32G32Sfloat,    static_cast<uint32_t>(offsetof(Vertex, texCoord))},
        }};
    }

    /// @brief Equality comparison used by the deduplication hash map in @c VulkanMesh::loadMesh().
    bool operator==(const Vertex& other) const noexcept
    {
        return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
    }
};

/// @brief Per-frame camera matrices, @c proj * @c view uploaded as a push constant.
///
/// @c view and @c proj are provided by the editor each frame via
/// @c VulkanRHI::updateCameraUBO(). @c proj should already have its Y axis
/// flipped for Vulkan NDC before being stored here.
struct UniformBufferObject
{
    glm::mat4 view; ///< World-to-camera transform.
    glm::mat4 proj; ///< Perspective projection (Y-flipped for Vulkan NDC).
};

/// @brief Per-mesh data stored in the GPU-side SSBO (set 0, binding 0).
///
/// One entry per mesh, indexed in the vertex shader via @c SV_StartInstanceLocation
/// which carries the @c firstInstance value set in each @c DrawIndexedIndirectCommand.
///
/// Must be kept byte-for-byte identical to the @c ObjectData / @c GPUMeshData struct
/// in both @c Base_Shader.slang and @c Cull.slang.
///
/// @note The two @c float padding fields (@c _pad0, @c _pad1) are required to satisfy
///       the std430 alignment rule for @c vec3 members inside a storage buffer.
struct GPUMeshData
{
    glm::mat4 transform;    ///< Model-to-world transform matrix.
    uint32_t  textureIndex; ///< Index into the bindless texture array (set 1, binding 0).
    uint32_t  firstIndex;   ///< Offset into the global index buffer (informational).
    uint32_t  indexCount;   ///< Number of indices belonging to this mesh (informational).
    uint32_t  vertexOffset; ///< Offset into the global vertex buffer (informational).
    glm::vec3 aabbMin;      ///< Object-space AABB minimum corner, computed at load time.
    float     _pad0;        ///< Padding to satisfy std430 vec3 alignment.
    glm::vec3 aabbMax;      ///< Object-space AABB maximum corner, computed at load time.
    float     _pad1;        ///< Padding to satisfy std430 vec3 alignment.
};

/// @brief Per-frame scene lighting and camera data uploaded to the scene UBO (set 2, binding 0).
///
/// Consumed by the fragment shader for Blinn-Phong lighting: ambient, diffuse,
/// and specular contributions. @c cameraPos is used to compute the view vector
/// for specular highlights.
///
/// Must be kept byte-for-byte identical to the @c SceneUBO struct in @c Base_Shader.slang.
///
/// @note The four @c float padding fields are required to satisfy the std430/std140
///       alignment rule that @c vec3 members must be padded to @c vec4 boundaries
///       inside a uniform or storage buffer.
struct SceneUBO
{
    glm::vec3 lightDir;    ///< Normalized world-space direction pointing TOWARD the light source.
    float cellSize     = 1.0f;
    glm::vec3 lightColor;  ///< Linear RGB color and intensity of the directional light.
    float thickEvery   = 10.0f;
    glm::vec3 ambientColor;///< Linear RGB ambient term added to all fragments regardless of normal.
    float fadeDistance = 100.0f;
    glm::vec3 cameraPos;   ///< World-space camera position used to compute the view vector V.
    float lineWidth    = 1.0f;
};


/// @brief CPU-side description of a point light.
///
/// A point light radiates equally in all directions. Attenuation follows the
/// physically-based inverse-square law, clamped to a maximum influence radius
/// of @c radius world-space units.
///
/// @note Keep fields in sync with @c GPULight layout offsets.
struct PointLight
{
    Vector3<float> position  = { 0.0f, 0.0f, 0.0f }; ///< World-space origin.
    Vector3<float> color     = { 1.0f, 1.0f, 1.0f }; ///< Linear-space RGB color.
    float     intensity = 1.0f;                 ///< Luminous intensity scalar (lm/sr equivalent).
    float     radius    = 10.0f;                ///< Maximum influence radius in world units.
    std::string name = "Unknown";
    ECS::EntityID id = ECS::INVALID_VALUE;
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
    Vector3<float> position       = { 0.0f,  0.0f,  0.0f }; ///< World-space origin.
    Vector3<float> direction      = { 0.0f,  0.0f, -1.0f }; ///< Unit vector pointing down the cone axis.
    Vector3<float> color          = { 1.0f,  1.0f,  1.0f }; ///< Linear-space RGB color.
    float     intensity      = 1.0f;                  ///< Luminous intensity scalar.
    float     radius         = 20.0f;                 ///< Maximum influence radius in world units.
    float     innerCutoffDeg = 15.0f;                 ///< Full-brightness cone half-angle (degrees).
    float     outerCutoffDeg = 30.0f;                 ///< Zero-brightness cone half-angle (degrees).
    bool      showGizmo      = true;                  ///< When false the cone wireframe gizmo is hidden.
    std::string name = "Unknown";
    ECS::EntityID id = ECS::INVALID_VALUE;
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

/// @brief Frustum plane data passed as a push constant to the culling compute shader.
///
/// Populated each frame by @c VulkanRHI::extractFrustum() using the Gribb-Hartmann
/// method applied to the combined @c proj * @c view matrix. Each plane is stored as a
/// normalized @c vec4 where @c xyz is the inward-facing normal and @c w is the
/// signed distance from the origin, such that @c dot(n, p) + d >= 0 means
/// the point @c p is inside (or on) the plane.
///
/// @note The three @c float padding fields are required so the total struct size
///       is a multiple of 16 bytes, satisfying the Vulkan push constant alignment rules.
struct FrustumPlanes
{
    glm::vec4 planes[6];   ///< Left, right, bottom, top, near, far - all inward-facing.
    uint32_t  objectCount; ///< Total number of objects to test; used as the dispatch bound.
    float     _pad[3];     ///< Padding to 16-byte alignment.
};

struct SceneInfos
{
    glm::vec4 clearColor     = {0.1f, 0.1f, 0.1f, 1.0f};
    glm::vec3 ambientColor   = {0.2f, 0.2f, 0.2f};
    glm::vec3 lightColor     = {0.5f, 0.5f, 0.5f};
    glm::vec3 lightDirection = {1.0f, 1.0f, 0.0f};
    float cellSize     = 1.0f;
    float thickEvery   = 10.0f;
    float fadeDistance = 100.0f;
    float lineWidth    = 1.0f;
};

struct InitInfos
{
    VulkanRHI* instance;
    std::vector<Mesh>* meshData;
    std::vector<PointLight>* pointLights;
    std::vector<SpotLight>* spotLights;
    ECS::Registry* registry;
    VkDescriptorSet* ds;
};

struct GridPushConstant // mirrors Grid.slang
{
    glm::mat4 invViewProj;
    glm::mat4 viewProj;
};
static_assert(sizeof(GridPushConstant) <= 128, "GridPushConstants exceeds push constant limit");

struct SpritePushConstants  // mirrors Gizmo_LightSprite.slang
{
    glm::mat4 viewProj;
    glm::vec3 worldPos;
    float     halfSize;
    glm::vec3 color;
    float     _pad;
    glm::vec3 cameraRight;
    float     _pad2;
    glm::vec3 cameraUp;
    float     _pad3;
};
static_assert(sizeof(SpritePushConstants) <= 128, "SpritePushConstants exceeds push constant limit");

struct ConePushConstants
{
    glm::mat4 viewProj;
    glm::vec3 apex;
    float     length;
    glm::vec3 direction;
    float     outerCos;
    glm::vec3 color;
    float     innerCos;
    float     alpha;
    uint32_t  visible;
    float     _pad[2];
};
static_assert(sizeof(ConePushConstants) <= 128, "ConePushConstants exceeds push constant limit");

} // Namespace gcep::rhi::vulkan
