#include "VulkanMesh.hpp"

#include <RHI/ObjParser/ObjParser.hpp>
#include <RHI/Vulkan/VulkanRHI.hpp>

// STL
#include <iostream>
#include <unordered_map>

namespace gcep::rhi::vulkan
{

void VulkanMesh::loadMesh(VulkanRHI* instance, const std::filesystem::path& filepath)
{
    pRhi = instance;
    static constexpr bool deduplication = true;
    auto [attrib, modelIndices] = objParser::ObjLoader::loadObj(filepath);

    std::unordered_map<Vertex, uint32_t> uniqueVertices;
    auto start_timer = std::chrono::high_resolution_clock::now();

    m_vertices.reserve(modelIndices.size());
    m_indices.reserve(modelIndices.size());

    const float* verts     = attrib.vertices.data();
    const float* texcoords = attrib.texcoords.data();
    const float* normals   = attrib.normals.data();

    for (const auto& idx : modelIndices)
    {
        Vertex vertex{};
        const float* v = verts + 3 * idx.vertex_index;
        vertex.pos = { v[0], v[1], v[2] };

        if (idx.texcoord_index != UINT32_MAX)
        {
            const float* tc = texcoords + 2 * idx.texcoord_index;
            vertex.texCoord = { tc[0], 1.0f - tc[1] };
        }
        else
        {
            vertex.texCoord = { 0.0f, 0.0f };
        }

        if (idx.normal_index != UINT32_MAX)
        {
            const float* n = normals + 3 * idx.normal_index;
            vertex.color = { n[0], n[1], n[2] };
        }
        else
        {
            vertex.color = { 1.0f, 1.0f, 1.0f };
        }

        if constexpr (deduplication)
        {
            auto [it, inserted] = uniqueVertices.try_emplace(vertex, static_cast<uint32_t>(m_vertices.size()));

            if (inserted)
            {
                m_vertices.push_back(vertex);
            }

            m_indices.push_back(it->second);
        }
        else
        {
            m_vertices.emplace_back(vertex);
            m_indices.emplace_back(m_indices.size());
        }
    }

    m_numIndices = static_cast<uint32_t>(m_indices.size());

    if constexpr (deduplication)
    {
        auto end_timer = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_timer - start_timer);
        std::cout << "Deduplicated: " << modelIndices.size() << " original vertices -> " << m_vertices.size()
                  << " unique vertices (" << (100.0f * m_vertices.size() / modelIndices.size()) << "% were unique vertices)\n";
        std::cout << "Deduplication took " << duration.count() << "ms." << '\n';
    }

    attrib.vertices.clear();  attrib.vertices.shrink_to_fit();
    attrib.texcoords.clear(); attrib.texcoords.shrink_to_fit();
    attrib.normals.clear();   attrib.normals.shrink_to_fit();

    modelIndices.clear(); modelIndices.shrink_to_fit();

    uniqueVertices.clear();

    createVertexBuffer();
    createIndexBuffer();
}

void VulkanMesh::createVertexBuffer()
{
    const vk::DeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();

    vk::raii::Buffer       stagingBuffer       = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;

    pRhi->createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory
    );

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    std::memcpy(data, m_vertices.data(), static_cast<size_t>(bufferSize));
    stagingBufferMemory.unmapMemory();
    m_vertices.clear(); m_vertices.shrink_to_fit();

    pRhi->createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_vertexBuffer, m_vertexBufferMemory
    );

    pRhi->copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
}

void VulkanMesh::createIndexBuffer()
{
    const vk::DeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();

    vk::raii::Buffer       stagingBuffer       = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;
    pRhi->createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory
    );

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    std::memcpy(data, m_indices.data(), static_cast<size_t>(bufferSize));
    stagingBufferMemory.unmapMemory();
    m_indices.clear(); m_indices.shrink_to_fit();

    pRhi->createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_indexBuffer, m_indexBufferMemory
    );

    pRhi->copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
}

} // Namespace gcep::rhi::vulkan