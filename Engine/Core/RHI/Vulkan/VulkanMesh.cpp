#include "VulkanMesh.hpp"

#include <RHI/ObjParser/ObjParser.hpp>
#include <RHI/Vulkan/VulkanRHI.hpp>

// STL
#include <iostream>
#include <unordered_map>

namespace gcep::rhi::vulkan
{

void VulkanMesh::loadMesh(VulkanRHI* instance, const std::filesystem::path& filepath, const std::filesystem::path& textureFilepath, glm::mat4 transform)
{
    id = entityID++;
    pRhi = instance;
    m_transform = transform;

    if(!textureFilepath.empty())
    {
        m_texture.loadTexture(pRhi, textureFilepath);
    }

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
            vertex.normal = {n[0], n[1], n[2] };
        }
        else
        {
            vertex.normal = {1.0f, 1.0f, 1.0f };
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

    m_numVertices = static_cast<int32_t>(m_vertices.size());
    m_numIndices  = static_cast<int32_t>(m_indices.size());

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

    m_aabbMin = glm::vec3( std::numeric_limits<float>::max());
    m_aabbMax = glm::vec3(-std::numeric_limits<float>::max());

    for (const auto& vertex : m_vertices)
    {
        m_aabbMin = glm::min(m_aabbMin, vertex.pos);
        m_aabbMax = glm::max(m_aabbMax, vertex.pos);
    }
}
glm::mat4& VulkanMesh::getTransform() {
    const float c3 = glm::cos(transform.rotation.z);
    const float s3 = glm::sin(transform.rotation.z);
    const float c2 = glm::cos(transform.rotation.x);
    const float s2 = glm::sin(transform.rotation.x);
    const float c1 = glm::cos(transform.rotation.y);
    const float s1 = glm::sin(transform.rotation.y);
    m_transform = {
        {
            transform.scale.x * (c1 * c3 + s1 * s2 * s3),
            transform.scale.x * (c2 * s3),
            transform.scale.x * (c1 * s2 * s3 - c3 * s1),
            0.0f,
        },
        {
            transform.scale.y * (c3 * s1 * s2 - c1 * s3),
            transform.scale.y * (c2 * c3),
            transform.scale.y * (c1 * c3 * s2 + s1 * s3),
            0.0f,
        },
        {
            transform.scale.z * (c2 * s1),
            transform.scale.z * (-s2),
            transform.scale.z * (c1 * c2),
            0.0f,
        },
        {transform.position.x, transform.position.y, transform.position.z, 1.0f}
    };
    return m_transform;
}

void VulkanMesh::setTexture(const std::filesystem::path& filepath, bool mipmaps)
{
    m_texture.loadTexture(pRhi, filepath, mipmaps);
}

void VulkanMesh::setPosition(glm::vec3 pos)
{
    m_transform[3] = glm::vec4(pos, 1.0f);
}

void VulkanMesh::setRotation(glm::vec3 eulerDegrees)
{
    glm::vec3 translation = glm::vec3(m_transform[3]);
    glm::vec3 scale =
    {
        glm::length(glm::vec3(m_transform[0])),
        glm::length(glm::vec3(m_transform[1])),
        glm::length(glm::vec3(m_transform[2]))
    };

    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(eulerDegrees.x), {1,0,0});
    rot           = glm::rotate(rot,             glm::radians(eulerDegrees.y), {0,1,0});
    rot           = glm::rotate(rot,             glm::radians(eulerDegrees.z), {0,0,1});

    m_transform = glm::translate(glm::mat4(1.0f), translation)
                  * rot
                  * glm::scale(glm::mat4(1.0f), scale);
}

void VulkanMesh::setScale(glm::vec3 scale)
{
    m_transform[0] = glm::vec4(glm::normalize(glm::vec3(m_transform[0])) * scale.x, 0.0f);
    m_transform[1] = glm::vec4(glm::normalize(glm::vec3(m_transform[1])) * scale.y, 0.0f);
    m_transform[2] = glm::vec4(glm::normalize(glm::vec3(m_transform[2])) * scale.z, 0.0f);
}

void VulkanMesh::setTransform(glm::mat4 transform)
{
    m_transform = transform;
}

} // Namespace gcep::rhi::vulkan