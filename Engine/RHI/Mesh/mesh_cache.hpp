#pragma once

// STL
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace gcep::rhi
{
    /// @brief Describes a single contiguous geometry region inside the global
    ///        vertex and index buffers, shared by all mesh entities that loaded
    ///        the same OBJ file.
    struct GeometryRegion
    {
        uint32_t vertexOffset = 0; ///< First vertex index inside the global vertex buffer.
        uint32_t firstIndex   = 0; ///< First index inside the global index buffer.
        uint32_t vertexCount  = 0; ///< Number of vertices in this region.
        uint32_t indexCount   = 0; ///< Number of indices in this region.
        uint32_t refCount     = 0; ///< Number of mesh entities currently sharing this region.
    };

    /// @brief Maps canonical OBJ paths to their uploaded @c GeometryRegion descriptors.
    ///
    /// Lives inside @c VulkanRHI as a plain member. Ref-counted so GPU memory is
    /// only reclaimed when the last entity referencing a given OBJ is destroyed.
    class MeshCache
    {
    public:
        /// @brief Returns a pointer to the cached region for @p path, or @c nullptr
        ///        if this path has not been uploaded yet.
        [[nodiscard]] GeometryRegion* find(const std::filesystem::path& path);

        /// @brief Registers a newly uploaded region for @p path with @c refCount = 1.
        ///
        /// @param path    Path to the source OBJ file.
        /// @param region  Descriptor containing offsets and vertex/index counts.
        /// @returns A reference to the newly inserted entry.
        GeometryRegion& insert(const std::filesystem::path& path, GeometryRegion region);

        /// @brief Increments the reference count for @p path and returns the region.
        /// @pre @p path must already be present in the cache.
        GeometryRegion& addRef(const std::filesystem::path& path);

        /// @brief Decrements the reference count for @p path, erasing the entry at zero.
        /// @returns @c true if the count hit zero and the caller must compact the GPU buffers.
        bool release(const std::filesystem::path& path);

        /// @brief Patches all cached offsets after a compaction closed a hole in the buffers.
        ///
        /// Regions starting at or after @p removedVertexOffset + @p removedVertexCount are
        /// shifted down by @p removedVertexCount (and analogously for indices).
        void patchAfterCompaction(uint32_t removedVertexOffset, uint32_t removedFirstIndex,
                                  uint32_t removedVertexCount,  uint32_t removedIndexCount);

        /// @brief Removes all cached entries (call when the GPU geometry buffers are reset).
        void clear() { m_cache.clear(); }

        /// @brief Returns a read-only view of all entries, keyed by canonical path string.
        [[nodiscard]] const std::unordered_map<std::string, GeometryRegion>& all() const;

    private:
        [[nodiscard]] static std::string canonical(const std::filesystem::path& p);

        std::unordered_map<std::string, GeometryRegion> m_cache;
    };
} // namespace gcep::rhi
