#pragma once

#include <Engine/Core/ECS/headers/entity_component.hpp>
#include <Engine/RHI/Vulkan/VulkanRHIDataTypes.hpp>
#include <Engine/RHI/Vulkan/VulkanTexture.hpp>
#include <ECS/Components/physics_component.hpp>
#include <ECS/Components/transform.hpp>
#include <PhysicsWrapper/physics_shape.hpp>

// Externals
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

// STL
#include <cstdint>
#include <filesystem>
#include <vector>

namespace gcep::rhi::vulkan
{
class VulkanRHI;

/// @brief CPU-side geometry, transform, and texture data for a single 3D mesh entity.
///
/// Handles the full pipeline from OBJ loading to GPU upload readiness: vertex
/// deduplication, AABB computation, and CPU-side buffer storage. The actual
/// device-local buffers live in @c VulkanRHI's global vertex and index buffers;
/// this class only holds staging data and metadata until @c clearVertices() /
/// @c clearIndices() are called after the GPU upload.
///
/// Instances are stored by value inside @c VulkanRHI::meshes. Each instance
/// owns exactly one @c VulkanTexture which may internally share GPU resources
/// with other instances via @c VulkanTexture's @c shared_ptr-based cache.
///
/// @note Holds a non-owning pointer to @c VulkanRHI — the RHI must outlive all
///       meshes it owns.
class Mesh
{
public:
    // Loading

    /// @brief Loads an OBJ file and optionally a texture from disk.
    ///
    /// Parses the OBJ via @c ObjLoader, builds the vertex array, computes the
    /// object-space AABB, and loads the texture via @c VulkanTexture::loadTexture()
    /// if @p textureFilepath is non-empty. CPU attribute data is freed immediately
    /// after the vertex array is built. @c m_vertices and @c m_indices remain
    /// populated until @c VulkanRHI::uploadSingleMesh() calls @c clearVertices()
    /// and @c clearIndices().
    ///
    /// @param instance         Non-owning pointer to the owning @c VulkanRHI.
    /// @param filepath         Path to the source OBJ file.
    /// @param textureFilepath  Path to the albedo texture. Pass empty to skip.
    /// @param transform        Initial model-to-world transform (default: identity).
    void load(VulkanRHI*                   instance,
              const std::filesystem::path& filepath,
              const std::filesystem::path& textureFilepath = L"",
              glm::mat4                    transform       = glm::mat4(1.0f));

    void loadObj(const std::filesystem::path& objPath);

    void loadGLTF(const std::filesystem::path& objPath);

    // Texture

    /// @brief Loads or replaces the mesh's texture at runtime.
    ///
    /// Delegates to @c VulkanTexture::loadTexture(). If the texture path is
    /// already cached, the existing bindless slot is reused. The @c GPUMeshData
    /// entry is updated on the next @c refreshMeshData() call.
    ///
    /// @param filepath  Path to the new texture file.
    /// @param mipmaps   When @c true, generates a full mip chain.
    void setTexture(const std::filesystem::path& filepath, bool mipmaps = true);

    [[nodiscard]] bool hasTexture() const noexcept { return m_texture.hasTexture(); }
    [[nodiscard]] bool hasMipmaps() const noexcept { return m_texture.hasMipmaps(); }

    /// @brief Returns a non-owning pointer to the mesh's @c VulkanTexture.
    [[nodiscard]] VulkanTexture* texture() noexcept { return &m_texture; }

    // Transform

    /// @brief Rebuilds and returns the model-to-world matrix from the ECS
    ///        @c ECS::Transform (position, rotation quaternion, scale).
    [[nodiscard]] glm::mat4& getTransform();

    /// @brief Overwrites the world-space position column of @c m_transform.
    void setPosition(glm::vec3 position);

    /// @brief Rebuilds @c m_transform as T * R(eulerDegrees) * S, preserving
    ///        the existing translation and scale.
    void setRotation(glm::vec3 eulerDegrees);

    /// @brief Rescales the rotation columns of @c m_transform by @p scale,
    ///        leaving the translation column unchanged.
    void setScale(glm::vec3 scale);

    /// @brief Directly replaces the model-to-world transform matrix.
    void setTransform(glm::mat4 transform);

    // Geometry accessors

    /// @brief Returns the number of indices (valid even after @c clearIndices()).
    [[nodiscard]] int32_t getNumIndices()  const noexcept { return m_numIndices;  }

    /// @brief Returns the number of vertices (valid even after @c clearVertices()).
    [[nodiscard]] int32_t getNumVertices() const noexcept { return m_numVertices; }

    /// @brief Returns the byte size of the vertex data for staging buffer allocation.
    [[nodiscard]] vk::DeviceSize getVertexBufferSize() const noexcept
        { return static_cast<vk::DeviceSize>(m_numVertices) * sizeof(Vertex); }

    /// @brief Returns the byte size of the index data for staging buffer allocation.
    [[nodiscard]] vk::DeviceSize getIndexBufferSize() const noexcept
        { return static_cast<vk::DeviceSize>(m_numIndices) * sizeof(uint32_t); }

    /// @brief Returns the CPU-side vertex array. Valid until @c clearVertices().
    [[nodiscard]] const std::vector<Vertex>&   getVertices() const noexcept { return m_vertices; }

    /// @brief Returns the CPU-side index array. Valid until @c clearIndices().
    [[nodiscard]] const std::vector<uint32_t>& getIndices()  const noexcept { return m_indices;  }

    /// @brief Returns the object-space AABB minimum corner.
    [[nodiscard]] glm::vec3 getAABBMin() const noexcept { return m_aabbMin; }

    /// @brief Returns the object-space AABB maximum corner.
    [[nodiscard]] glm::vec3 getAABBMax() const noexcept { return m_aabbMax; }

    /// @brief Returns the canonical path of the OBJ file this mesh was loaded from.
    [[nodiscard]] const std::filesystem::path& objPath() const noexcept { return m_objPath; }

    /// @brief Frees the CPU-side vertex vector after GPU upload.
    void clearVertices() { m_vertices.clear(); m_vertices.shrink_to_fit(); }

    /// @brief Frees the CPU-side index vector after GPU upload.
    void clearIndices()  { m_indices.clear();  m_indices.shrink_to_fit();  }

    // ECS / editor state

    ECS::Transform transform; ///< ECS transform component (position, rotation, scale).
    ECS::PhysicsComponent   physics;   ///< ECS physics component (motion type, layers, body).
    std::string        name = "Unknown"; ///< Name shown in the scene hierarchy.
    ECS::EntityID      id   = std::numeric_limits<ECS::EntityID>::max(); ///< ECS entity ID.

private:
    VulkanRHI* pRhi = nullptr; ///< Non-owning pointer to the parent RHI context.

    std::vector<Vertex>   m_vertices; ///< CPU-side vertex data; freed after GPU upload.
    std::vector<uint32_t> m_indices;  ///< CPU-side index data; freed after GPU upload.

    glm::vec3 m_aabbMin{}; ///< Object-space AABB minimum, computed during @c load().
    glm::vec3 m_aabbMax{}; ///< Object-space AABB maximum, computed during @c load().

    int32_t m_numIndices  = 0; ///< Cached index  count; valid after @c clearIndices().
    int32_t m_numVertices = 0; ///< Cached vertex count; valid after @c clearVertices().

    glm::mat4             m_transform{}; ///< Cached model-to-world matrix, rebuilt by @c getTransform().
    std::filesystem::path m_objPath;     ///< Canonical OBJ file path used as the @c MeshCache key.

    VulkanTexture m_texture; ///< Albedo texture; may share GPU resources via the texture cache.
};

} // Namespace gcep::rhi::vulkan
