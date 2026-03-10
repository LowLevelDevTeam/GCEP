#include "MeshCache.hpp"

namespace gcep::rhi
{

GeometryRegion* MeshCache::find(const std::filesystem::path& path)
{
    auto it = m_cache.find(canonical(path));
    return it != m_cache.end() ? &it->second : nullptr;
}

GeometryRegion& MeshCache::insert(const std::filesystem::path& path, GeometryRegion region)
{
    region.refCount = 1;
    return m_cache.emplace(canonical(path), region).first->second;
}

GeometryRegion& MeshCache::addRef(const std::filesystem::path& path)
{
    auto& r = m_cache.at(canonical(path));
    ++r.refCount;
    return r;
}

bool MeshCache::release(const std::filesystem::path& path)
{
    auto key = canonical(path);
    auto it  = m_cache.find(key);
    if (it == m_cache.end()) return false;

    if (--it->second.refCount == 0)
    {
        m_cache.erase(it);
        return true;
    }
    return false;
}

void MeshCache::patchAfterCompaction(uint32_t removedVertexOffset,
                                     uint32_t removedFirstIndex,
                                     uint32_t removedVertexCount,
                                     uint32_t removedIndexCount)
{
    for (auto& [key, region] : m_cache)
    {
        if (region.vertexOffset >= removedVertexOffset + removedVertexCount)
            region.vertexOffset -= removedVertexCount;
        if (region.firstIndex >= removedFirstIndex + removedIndexCount)
            region.firstIndex -= removedIndexCount;
    }
}

const std::unordered_map<std::string, GeometryRegion>& MeshCache::all() const
{
    return m_cache;
}

std::string MeshCache::canonical(const std::filesystem::path& p)
{
    std::error_code ec;
    auto c = std::filesystem::canonical(p, ec);
    return ec ? p.string() : c.string();
}

} // Namespace gcep::rhi
