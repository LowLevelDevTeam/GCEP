#include "mesh.hpp"

// Internals
#include "Engine/ObjParser/obj_parser.hpp"
#include <Engine/RHI/Vulkan/vulkan_rhi.hpp>
#include <Log/log.hpp>

// Externals
#ifndef TINYGLTF_IMPLEMENTATION
    #define TINYGLTF_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
    #define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <tiny_gltf.h>

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

        std::unordered_map<Vertex, uint32_t> uniqueVertices;

        if (filepath.extension().string() == ".obj") {
            loadObj(filepath);
        }
        else if (filepath.extension().string() == ".gltf")
        {
            loadGLTF(filepath);
        }
        else
        {
            return;
        }
        if(m_vertices.empty() || m_indices.empty())
        {
            return;
        }

        m_numVertices = static_cast<int32_t>(m_vertices.size());
        m_numIndices  = static_cast<int32_t>(m_indices.size());

        m_aabbMin = glm::vec3( std::numeric_limits<float>::max());
        m_aabbMax = glm::vec3(-std::numeric_limits<float>::max());

        for (const auto& vertex : m_vertices)
        {
            m_aabbMin = glm::min(m_aabbMin, vertex.pos);
            m_aabbMax = glm::max(m_aabbMax, vertex.pos);
        }

        Log::info("Loaded mesh " + filepath.filename().string());
    }

    void Mesh::loadObj(const std::filesystem::path& filepath) {
        auto [attrib, modelIndices] = objParser::loadObj(filepath);

        if(attrib.vertices.empty() || modelIndices.empty())
            return;

        static constexpr bool deduplication = false;

        m_vertices.reserve(modelIndices.size());
        m_indices.reserve(modelIndices.size());

        const float* verts     = attrib.vertices.data();
        const float* texcoords = attrib.texcoords.data();
        const float* normals   = attrib.normals.data();

        std::unordered_map<Vertex, uint32_t> uniqueVertices;
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

        attrib.vertices.clear();  attrib.vertices.shrink_to_fit();
        attrib.texcoords.clear(); attrib.texcoords.shrink_to_fit();
        attrib.normals.clear();   attrib.normals.shrink_to_fit();

        modelIndices.clear(); modelIndices.shrink_to_fit();
    }

    void Mesh::loadGLTF(const std::filesystem::path& objPath)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, objPath.string());

        if (!warn.empty()) {
            std::cout << "glTF warning: " << warn << std::endl;
        }

        if (!err.empty()) {
            std::cout << "glTF error: " << err << std::endl;
        }

        if (!ret) {
            throw std::runtime_error("Failed to load glTF model");
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices;
        for (const auto& mesh : model.meshes) {
            for (const auto& primitive : mesh.primitives) {
                // Get indices
                const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

                // Get vertex positions
                const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
                const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

                // Get texture coordinates if available
                bool hasTexCoords = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();
                const tinygltf::Accessor* texCoordAccessor = nullptr;
                const tinygltf::BufferView* texCoordBufferView = nullptr;
                const tinygltf::Buffer* texCoordBuffer = nullptr;

                if (hasTexCoords) {
                    texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
                    texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
                    texCoordBuffer = &model.buffers[texCoordBufferView->buffer];
                }

                // Process vertices
                for (size_t i = 0; i < posAccessor.count; i++) {
                    Vertex vertex{};

                    // Get position
                    const float* pos = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * 12]);
                    vertex.pos = {pos[0], pos[1], pos[2]};

                    // Get texture coordinates if available
                    if (hasTexCoords) {
                        const float* texCoord = reinterpret_cast<const float*>(&texCoordBuffer->data[texCoordBufferView->byteOffset + texCoordAccessor->byteOffset + i * 8]);
                        vertex.texCoord = {texCoord[0], 1.0f - texCoord[1]};
                    } else {
                        vertex.texCoord = {0.0f, 0.0f};
                    }

                    // Set default color
                    vertex.normal = {1.0f, 1.0f, 1.0f};

                    // Add vertex if unique
                    if (!uniqueVertices.contains(vertex)) {
                        uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
                        m_vertices.push_back(vertex);
                    }
                }

                // Process indices
                const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

                // Handle different index component types
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(indexData);
                    for (size_t i = 0; i < indexAccessor.count; i++) {
                        Vertex vertex = m_vertices[indices16[i]];
                        m_indices.push_back(uniqueVertices[vertex]);
                    }
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(indexData);
                    for (size_t i = 0; i < indexAccessor.count; i++) {
                        Vertex vertex = m_vertices[indices32[i]];
                        m_indices.push_back(uniqueVertices[vertex]);
                    }
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* indices8 = reinterpret_cast<const uint8_t*>(indexData);
                    for (size_t i = 0; i < indexAccessor.count; i++) {
                        Vertex vertex = m_vertices[indices8[i]];
                        m_indices.push_back(uniqueVertices[vertex]);
                    }
                }
            }
        }
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
