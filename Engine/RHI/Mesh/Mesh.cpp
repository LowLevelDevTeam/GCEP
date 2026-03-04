#include "Mesh.hpp"

#include <Engine/ObjParser/ObjParser.hpp>
#include <Engine/RHI/Vulkan/VulkanRHI.hpp>
#include <Log/Log.hpp>

// STL
#include <unordered_map>

namespace gcep::rhi::vulkan
{

void Mesh::load(VulkanRHI* instance, const std::filesystem::path& filepath, const std::filesystem::path& textureFilepath, glm::mat4 transform)
{
    if (name == "Unknown")
    {
        name = "Unknown " + std::to_string(id);
    }

    pRhi        = instance;
    m_transform = transform;
    m_objPath   = filepath;

    if(!textureFilepath.empty())
    {
        m_texture.loadTexture(pRhi, textureFilepath);
    }

    static constexpr bool deduplication = false;
    auto [attrib, modelIndices] = objParser::ObjLoader::loadObj(filepath);

    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    m_vertices.reserve(modelIndices.size());
    m_indices.reserve(modelIndices.size());

    const float* verts     = attrib.vertices.data();
    const float* texcoords = attrib.texcoords.data();
    const float* normals   = attrib.normals.data();

    for (const auto& idx : modelIndices)
    {
        Vertex vertex{};
        const float* vp = verts + 3 * idx.vertex_index;
        vertex.pos = {vp[0], vp[1], vp[2] };

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
            vertex.normal = { n[0], n[1], n[2] };
        }
        else
        {
            vertex.normal = { 0.0f, 1.0f, 0.0f };
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

    Log::info("Loaded mesh " + filepath.filename().string());
}

glm::mat4& Mesh::getTransform()
{
    glm::quat q(
        transform.rotation.w,
        transform.rotation.x,
        transform.rotation.y,
        transform.rotation.z
    );

    glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(transform.position.x, transform.position.y, transform.position.z));
    glm::mat4 R = glm::mat4_cast(q);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(transform.scale.x, transform.scale.y, transform.scale.z));

    m_transform = T * R * S;
    return m_transform;
}

void Mesh::setTexture(const std::filesystem::path& filepath, bool mipmaps)
{
    m_texture.loadTexture(pRhi, filepath, mipmaps);
}

void Mesh::setPosition(glm::vec3 pos)
{
    m_transform[3] = glm::vec4(pos, 1.0f);
}

void Mesh::setRotation(glm::vec3 eulerDegrees)
{
    glm::vec3 translation = glm::vec3(m_transform[3]);
    glm::vec3 scale =
    {
        glm::length(glm::vec3(m_transform[0])),
        glm::length(glm::vec3(m_transform[1])),
        glm::length(glm::vec3(m_transform[2]))
    };

    glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(eulerDegrees.x), {1,0,0});
    R           = glm::rotate(R,               glm::radians(eulerDegrees.y), {0,1,0});
    R           = glm::rotate(R,               glm::radians(eulerDegrees.z), {0,0,1});

    m_transform = glm::translate(glm::mat4(1.0f), translation)
                  * R
                  * glm::scale(glm::mat4(1.0f), scale);
}

void Mesh::setScale(glm::vec3 scale)
{
    m_transform[0] = glm::vec4(glm::normalize(glm::vec3(m_transform[0])) * scale.x, 0.0f);
    m_transform[1] = glm::vec4(glm::normalize(glm::vec3(m_transform[1])) * scale.y, 0.0f);
    m_transform[2] = glm::vec4(glm::normalize(glm::vec3(m_transform[2])) * scale.z, 0.0f);
}

void Mesh::setTransform(glm::mat4 t)
{
    m_transform = t;
}

} // namespace gcep::rhi::vulkan