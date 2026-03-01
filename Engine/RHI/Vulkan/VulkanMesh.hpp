#pragma once

#include <Engine/RHI/Vulkan/VulkanRHIDataTypes.hpp>
#include <Engine/RHI/Vulkan/VulkanTexture.hpp>

// Externals
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

// STL
#include <cstdint>
#include <filesystem>
#include <vector>

#include <ECS/headers/entity_component.hpp>

namespace gcep::rhi::vulkan
{
static uint32_t entityID = 0;
class VulkanRHI;
struct TransformComponent {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
};
/// @brief Manages the CPU-side geometry and transform data for a single 3D mesh.
///
/// Handles the full pipeline from loading an OBJ file off disk to making geometry
/// available for GPU upload: vertex deduplication, AABB computation, and CPU-side
/// buffer storage. The actual device-local buffers live in @c VulkanRHI's global
/// vertex and index buffers; this class only holds the staging data and metadata.
///
/// Transform manipulation is decomposed into @c setPosition(), @c setRotation(),
/// and @c setScale() helpers that operate directly on @c m_transform, preserving
/// the other components where possible.
///
/// Typical usage:
/// @code
/// VulkanMesh mesh;
/// mesh.loadMesh(rhi, "assets/model.obj", "assets/albedo.png", glm::mat4(1.0f));
/// // mesh.getVertices() / mesh.getIndices() are ready for upload via VulkanRHI::initMeshBuffers()
/// @endcode
///
/// @note The mesh holds a non-owning pointer to the @c VulkanRHI instance that
///       created it; the RHI must therefore outlive any @c VulkanMesh it owns.
class VulkanMesh
{
public:
    /// @brief Loads an OBJ file and an optional texture from disk.
    ///
    /// Parses the OBJ via @c ObjLoader, deduplicates vertices using a hash map,
    /// computes the object-space AABB from the deduplicated vertex set, stores the
    /// results in @c m_vertices / @c m_indices / @c m_numVertices / @c m_numIndices,
    /// and loads the texture via @c VulkanTexture::loadTexture() if @p textureFilepath
    /// is non-empty.
    ///
    /// CPU-side OBJ attribute data (@c attrib.vertices, @c texcoords, @c normals,
    /// @c modelIndices) is freed immediately after the vertex array is built.
    /// @c m_vertices and @c m_indices remain populated until @c VulkanRHI::initMeshBuffers()
    /// calls @c clearVertices() / @c clearIndices() after copying them to the global buffer.
    ///
    /// @param instance          Non-owning pointer to the owning @c VulkanRHI context.
    /// @param filepath          Path to the source OBJ file.
    /// @param textureFilepath   Path to the albedo texture. Pass an empty path to skip.
    /// @param transform         Initial model-to-world transform (default: identity).
    void loadMesh(VulkanRHI* instance,
                  const std::filesystem::path& filepath,
                  const std::filesystem::path& textureFilepath   = L"",
                  glm::mat4                    transform         = glm::mat4(1.0f));

    bool hasTexture() const noexcept { return m_texture.hasTexture(); }
    bool hasMipmaps() const noexcept { return m_texture.hasMipmaps(); }

public:
    TransformComponent transform;
    std::string name = "Unknown";
    ECS::EntityID id = std::numeric_limits<ECS::EntityID>::max();

public:
    // Accessors

    /// @brief Returns the total number of indices (cached before @c clearIndices()).
    [[nodiscard]] const int32_t getNumIndices()  const noexcept { return m_numIndices;  }

    /// @brief Returns the total number of vertices (cached before @c clearVertices()).
    [[nodiscard]] const int32_t getNumVertices() const noexcept { return m_numVertices; }

    /// @brief Returns a const pointer to the mesh's texture.
    ///
    /// Valid for the lifetime of this mesh. The texture is registered in the
    /// bindless array during @c loadMesh(); use @c getBindlessIndex() on the
    /// returned texture to look up its shader slot.
    [[nodiscard]] VulkanTexture* texture() { return &m_texture; }

    /// @brief Returns the current model-to-world transform matrix.
    [[nodiscard]] glm::mat4& getTransform();

    /// @brief Returns the byte size of the vertex data (for staging buffer allocation).
    [[nodiscard]] const vk::DeviceSize getVertexBufferSize() const noexcept { return m_numVertices * sizeof(Vertex);   }

    /// @brief Returns the byte size of the index data (for staging buffer allocation).
    [[nodiscard]] const vk::DeviceSize getIndexBufferSize()  const noexcept { return m_numIndices  * sizeof(uint32_t); }

    /// @brief Returns a const reference to the CPU-side vertex array.
    ///
    /// Valid until @c clearVertices() is called by @c VulkanRHI::initMeshBuffers().
    [[nodiscard]] const std::vector<Vertex>&   getVertices() const noexcept { return m_vertices; }

    /// @brief Returns a const reference to the CPU-side index array.
    ///
    /// Valid until @c clearIndices() is called by @c VulkanRHI::initMeshBuffers().
    [[nodiscard]] const std::vector<uint32_t>& getIndices()  const noexcept { return m_indices;  }

    /// @brief Returns the object-space AABB minimum corner.
    ///
    /// Computed over all deduplicated vertices during @c loadMesh().
    /// Used by the GPU culling compute shader to build the world-space AABB.
    [[nodiscard]] glm::vec3 getAABBMin() const noexcept { return m_aabbMin; }

    /// @brief Returns the object-space AABB maximum corner.
    ///
    /// Computed over all deduplicated vertices during @c loadMesh().
    /// Used by the GPU culling compute shader to build the world-space AABB.
    [[nodiscard]] glm::vec3 getAABBMax() const noexcept { return m_aabbMax; }

    /// @brief Frees the CPU-side vertex vector and releases its memory.
    ///
    /// Called by @c VulkanRHI::initMeshBuffers() after the data has been copied
    /// to the global staging buffer. @c getNumVertices() remains valid.
    void clearVertices() { m_vertices.clear(); m_vertices.shrink_to_fit(); }

    /// @brief Frees the CPU-side index vector and releases its memory.
    ///
    /// Called by @c VulkanRHI::initMeshBuffers() after the data has been copied
    /// to the global staging buffer. @c getNumIndices() remains valid.
    void clearIndices()  { m_indices.clear();  m_indices.shrink_to_fit();  }

    // Transform setters

    /// @brief Sets the world-space position of the mesh.
    ///
    /// Writes directly into column 3 of @c m_transform, preserving the
    /// rotation and scale encoded in columns 0–2.
    ///
    /// @param position  New world-space position.
    void setPosition(glm::vec3 position);

    /// @brief Sets the world-space orientation of the mesh from Euler angles.
    ///
    /// Extracts the current translation and per-axis scale from @c m_transform,
    /// constructs a new rotation matrix from @p degrees (applied X -> Y -> Z),
    /// then rebuilds @c m_transform as @c T * R * S.
    ///
    /// @param degrees  Euler angles in degrees (pitch, yaw, roll).
    void setRotation(glm::vec3 degrees);

    /// @brief Sets the per-axis scale of the mesh.
    ///
    /// Normalizes the existing rotation columns of @c m_transform and
    /// rescales them by @p scale, leaving the translation column unchanged.
    ///
    /// @param scale  Per-axis scale factors.
    void setScale(glm::vec3 scale);

    /// @brief Directly replaces the model-to-world transform matrix.
    ///
    /// Use when you need to set an arbitrary transform that cannot be expressed
    /// as a combination of @c setPosition / @c setRotation / @c setScale.
    ///
    /// @param transform  New model-to-world matrix.
    void setTransform(glm::mat4 transform);

    /// @brief Loads or replaces the mesh's texture at runtime.
    ///
    /// Delegates to @c VulkanTexture::loadTexture(). The new texture is
    /// registered in the bindless array and the mesh's @c GPUMeshData entry
    /// will reflect the new index on the next @c refreshMeshData() call.
    ///
    /// @param filepath  Path to the new texture file.
    /// @param mipmaps   When @c true, generates a full mip chain.
    void setTexture(const std::filesystem::path& filepath, bool mipmaps = true);

private:
    VulkanRHI* pRhi; ///< Non-owning pointer to the parent RHI context.

    std::vector<Vertex>   m_vertices; ///< CPU-side vertex data; freed after GPU upload.
    std::vector<uint32_t> m_indices;  ///< CPU-side index data; freed after GPU upload.

    glm::vec3 m_aabbMin; ///< Object-space AABB minimum, computed during @c loadMesh().
    glm::vec3 m_aabbMax; ///< Object-space AABB maximum, computed during @c loadMesh().

    int32_t m_numIndices  = 0; ///< Cached index count; valid even after @c clearIndices().
    int32_t m_numVertices = 0; ///< Cached vertex count; valid even after @c clearVertices().

    glm::mat4 m_transform; ///< Current model-to-world transform.

    VulkanTexture m_texture; ///< Albedo texture registered in the bindless array.
};

} // Namespace gcep::rhi::vulkan