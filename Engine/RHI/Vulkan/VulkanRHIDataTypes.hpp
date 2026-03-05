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
    float     _pad0;       ///< Padding to satisfy std430 vec3 alignment.
    glm::vec3 lightColor;  ///< Linear RGB color and intensity of the directional light.
    float     _pad1;       ///< Padding to satisfy std430 vec3 alignment.
    glm::vec3 ambientColor;///< Linear RGB ambient term added to all fragments regardless of normal.
    float     _pad2;       ///< Padding to satisfy std430 vec3 alignment.
    glm::vec3 cameraPos;   ///< World-space camera position used to compute the view vector V.
    float     _pad3;       ///< Padding to satisfy std430 vec3 alignment.
};

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
};

struct InitInfos
{
    VulkanRHI* instance;
    std::vector<Mesh>* meshData;
    ECS::Registry* registry;
    VkDescriptorSet* ds;
};

// TODO: Fit in 128 bytes - Maybe invViewProjMatrix + viewMatrix and drop grid options = 128 bytes
struct GridPushConstant
{
    glm::mat4 invView;
    glm::mat4 invProj;
    glm::mat4 viewProj;

    float cellSize     = 1.0f;
    float thickEvery   = 10.0f;
    float fadeDistance = 100.0f;
    float lineWidth    = 1.0f;
};

} // Namespace gcep::rhi::vulkan