#pragma once

// Externals
#include <vulkan/vulkan_raii.hpp>

// STL
#include <cstdint>
#include <filesystem>
#include <vector>

namespace gcep::rhi::vulkan
{

struct Vertex;
class VulkanRHI;

/// @brief Manages the lifetime and GPU resources of a 3D mesh.
///
/// Handles the full pipeline from loading an OBJ file off disk to making it
/// available for rendering: vertex deduplication, staging upload, and
/// device-local vertex/index buffer creation.
///
/// Typical usage:
/// @code
/// VulkanMesh mesh;
/// mesh.loadMesh(this, "assets/model.obj");
/// // mesh.getVertexBuffer() / mesh.getIndexBuffer() are now ready to be bound
/// @endcode
///
/// @note The mesh holds a non-owning pointer to the @c VulkanRHI instance that
///       created it; the RHI must therefore outlive any @c VulkanMesh it owns.
class VulkanMesh
{
public:
    /// @brief Loads an OBJ file from disk and uploads geometry to device-local buffers.
    ///
    /// Parses the OBJ file via @c ObjLoader, deduplicates vertices using a hash map,
    /// then uploads the resulting data to device-local @c VkBuffer objects via
    /// @c createVertexBuffer() and @c createIndexBuffer(). Intermediate CPU-side
    /// vectors are freed after the upload is complete.
    ///
    /// @param instance  Non-owning pointer to the owning @c VulkanRHI context.
    /// @param filepath  Path to the source OBJ file.
    void loadMesh(VulkanRHI* instance, const std::filesystem::path& filepath);

public:
    /// @brief Returns the underlying @c VkBuffer handle for vertex data.
    [[nodiscard]] vk::Buffer getVertexBuffer() noexcept { return *m_vertexBuffer; }

    /// @brief Returns the underlying @c VkBuffer handle for index data.
    [[nodiscard]] vk::Buffer getIndexBuffer()  noexcept { return *m_indexBuffer;  }

    /// @brief Returns the total number of indices in the index buffer.
    [[nodiscard]] const uint32_t getNumIndices() const noexcept { return m_numIndices; }

private:
    /// @brief Creates a device-local vertex buffer and uploads vertex data via a staging buffer.
    ///
    /// Allocates a host-visible staging buffer, copies @c m_vertices into it, then
    /// creates a device-local @c VkBuffer with @c eVertexBuffer usage and submits a
    /// buffer copy. The CPU-side @c m_vertices vector is cleared and freed after upload.
    void createVertexBuffer();

    /// @brief Creates a device-local index buffer and uploads index data via a staging buffer.
    ///
    /// Allocates a host-visible staging buffer, copies @c m_indices into it, then
    /// creates a device-local @c VkBuffer with @c eIndexBuffer usage and submits a
    /// buffer copy. The CPU-side @c m_indices vector is cleared and freed after upload.
    void createIndexBuffer();

private:
    VulkanRHI* pRhi; ///< Non-owning pointer to the parent RHI context.

    std::vector<Vertex>   m_vertices; ///< CPU-side vertex data; freed after GPU upload.
    std::vector<uint32_t> m_indices;  ///< CPU-side index data; freed after GPU upload.

    uint32_t m_numIndices = 0; ///< Total number of indices in the index buffer.

    vk::raii::Buffer       m_vertexBuffer       = nullptr; ///< Device-local vertex buffer.
    vk::raii::Buffer       m_indexBuffer        = nullptr; ///< Device-local index buffer.
    vk::raii::DeviceMemory m_vertexBufferMemory = nullptr; ///< Memory backing @c m_vertexBuffer.
    vk::raii::DeviceMemory m_indexBufferMemory  = nullptr; ///< Memory backing @c m_indexBuffer.
};

} // Namespace gcep::rhi::vulkan