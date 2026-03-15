#include "vulkan_rhi.hpp"

// Internals
#include <Log/log.hpp>
#include <Maths/Utils/vector3_convertor.hpp>

// STL
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace gcep::rhi::vulkan
{
    // ============================================================================
    // Private - mesh / geometry buffers
    // ============================================================================

    void VulkanRHI::initMeshBuffers()
    {
        createBuffer(MAX_VERTEX_BUFFER_BYTES,
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            m_globalVertexBuffer, m_globalVertexBufferMemory
        );

        createBuffer(MAX_INDEX_BUFFER_BYTES,
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            m_globalIndexBuffer, m_globalIndexBufferMemory
        );

        const vk::DeviceSize meshDataSize = sizeof(GPUMeshData) * MAX_MESHES;
        createBuffer(meshDataSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            m_meshDataSSBO, m_meshDataSSBOMemory
        );
        m_meshDataSSBOMapped = m_meshDataSSBOMemory.mapMemory(0, meshDataSize);

        m_indirectBufferCapacity = sizeof(vk::DrawIndexedIndirectCommand) * MAX_MESHES;
        createBuffer(m_indirectBufferCapacity,
            vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            m_indirectBuffer, m_indirectBufferMemory
        );
        m_indirectBufferMapped = m_indirectBufferMemory.mapMemory(0, m_indirectBufferCapacity);

        for (uint32_t i = 0; i < static_cast<uint32_t>(meshes.size()); ++i)
            uploadSingleMesh(i);
    }

    int VulkanRHI::uploadSingleMesh(uint32_t meshIndex)
    {
        assert(meshIndex < MAX_MESHES && "Exceeded MAX_MESHES");

        auto& mesh = meshes[meshIndex];

        GeometryRegion* cached    = m_meshCache.find(mesh.objPath());
        bool geometryShared       = (cached != nullptr);
        uint32_t vertexOffset     = 0;
        uint32_t firstIndex       = 0;

        if (geometryShared)
        {
            GeometryRegion& r = m_meshCache.addRef(mesh.objPath());
            vertexOffset      = r.vertexOffset;
            firstIndex        = r.firstIndex;

            mesh.clearVertices();
            mesh.clearIndices();
        }
        else
        {
            const vk::DeviceSize vbSize = mesh.getVertexBufferSize();
            const vk::DeviceSize ibSize = mesh.getIndexBufferSize();

            if(ibSize == 0 || vbSize == 0)
            {
                return -1;
            }

            assert(m_vertexBytesFilled + vbSize <= MAX_VERTEX_BUFFER_BYTES && "Vertex buffer overflow");
            assert(m_indexBytesFilled  + ibSize <= MAX_INDEX_BUFFER_BYTES  && "Index buffer overflow");

            {
                vk::raii::Buffer       staging({});
                vk::raii::DeviceMemory stagingMem({});
                createBuffer(vbSize,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                    staging, stagingMem
                );

                void* ptr = stagingMem.mapMemory(0, vbSize);
                std::memcpy(ptr, mesh.getVertices().data(), vbSize);
                stagingMem.unmapMemory();

                auto cmd = beginSingleTimeCommands();
                vk::BufferCopy copy{};
                copy.srcOffset = 0;
                copy.dstOffset = m_vertexBytesFilled;
                copy.size      = vbSize;
                cmd->copyBuffer(*staging, *m_globalVertexBuffer, { copy });
                endSingleTimeCommands(*cmd);
            }

            {
                vk::raii::Buffer       staging({});
                vk::raii::DeviceMemory stagingMem({});
                createBuffer(ibSize,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                    staging, stagingMem
                );

                void* ptr = stagingMem.mapMemory(0, ibSize);
                std::memcpy(ptr, mesh.getIndices().data(), ibSize);
                stagingMem.unmapMemory();

                auto cmd = beginSingleTimeCommands();
                vk::BufferCopy copy{};
                copy.srcOffset = 0;
                copy.dstOffset = m_indexBytesFilled;
                copy.size      = ibSize;
                cmd->copyBuffer(*staging, *m_globalIndexBuffer, { copy });
                endSingleTimeCommands(*cmd);
            }

            vertexOffset = m_totalVertexCount;
            firstIndex   = m_totalIndexCount;

            GeometryRegion region{};
            region.vertexOffset = vertexOffset;
            region.firstIndex   = firstIndex;
            region.vertexCount  = static_cast<uint32_t>(mesh.getNumVertices());
            region.indexCount   = static_cast<uint32_t>(mesh.getNumIndices());
            m_meshCache.insert(mesh.objPath(), region);

            m_vertexBytesFilled += vbSize;
            m_indexBytesFilled  += ibSize;
            m_totalVertexCount  += mesh.getNumVertices();
            m_totalIndexCount   += mesh.getNumIndices();

            mesh.clearVertices();
            mesh.clearIndices();
        }

        GPUMeshData data{};
        data.firstIndex   = firstIndex;
        data.indexCount   = mesh.getNumIndices();
        data.vertexOffset = vertexOffset;
        data.textureIndex = mesh.texture()->getBindlessIndex();
        data.transform    = mesh.getTransform();
        data.aabbMin      = mesh.getAABBMin();
        data.aabbMax      = mesh.getAABBMax();

        if (meshIndex < m_meshData.size())
            m_meshData[meshIndex] = data;
        else
            m_meshData.emplace_back(data);

        static_cast<GPUMeshData*>(m_meshDataSSBOMapped)[meshIndex] = data;

        vk::DrawIndexedIndirectCommand drawCmd{};
        drawCmd.indexCount    = data.indexCount;
        drawCmd.instanceCount = 1;
        drawCmd.firstIndex    = data.firstIndex;
        drawCmd.vertexOffset  = static_cast<int32_t>(data.vertexOffset);
        drawCmd.firstInstance = meshIndex;

        if (meshIndex < m_indirectCommands.size())
            m_indirectCommands[meshIndex] = drawCmd;
        else
            m_indirectCommands.emplace_back(drawCmd);

        static_cast<vk::DrawIndexedIndirectCommand*>(m_indirectBufferMapped)[meshIndex] = drawCmd;

        return 0;
    }

    void VulkanRHI::updateMeshMetadata(uint32_t meshIndex)
    {
        assert(meshIndex < m_meshData.size() && "Mesh index out of range");

        auto& mesh = meshes[meshIndex];
        auto& data = m_meshData[meshIndex];

        data.textureIndex = mesh.texture()->getBindlessIndex();
        data.transform    = mesh.getTransform();
        data.aabbMin      = mesh.getAABBMin();
        data.aabbMax      = mesh.getAABBMax();

        static_cast<GPUMeshData*>(m_meshDataSSBOMapped)[meshIndex] = data;
    }

    void VulkanRHI::updateMeshDataSSBO()
    {
        const vk::DeviceSize size = sizeof(GPUMeshData) * m_meshData.size();
        std::memcpy(m_meshDataSSBOMapped, m_meshData.data(), size);
    }

    void VulkanRHI::createMeshDataDescriptors()
    {
        vk::DescriptorSetLayoutBinding binding{};
        binding.binding         = 0;
        binding.descriptorType  = vk::DescriptorType::eStorageBuffer;
        binding.descriptorCount = 1;
        binding.stageFlags      = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &binding;
        m_meshDataDescriptorSetLayout = vk::raii::DescriptorSetLayout(m_device.rawDevice(), layoutInfo);

        vk::DescriptorPoolSize poolSize{};
        poolSize.type            = vk::DescriptorType::eStorageBuffer;
        poolSize.descriptorCount = 1;

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.maxSets       = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        m_meshDataDescriptorPool = vk::raii::DescriptorPool(m_device.rawDevice(), poolInfo);

        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool     = *m_meshDataDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &(*m_meshDataDescriptorSetLayout);
        m_meshDataDescriptorSet = std::move(m_device.rawDevice().allocateDescriptorSets(allocInfo)[0]);

        updateMeshDataSet();
    }

    void VulkanRHI::updateMeshDataSet()
    {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = *m_meshDataSSBO;
        bufferInfo.offset = 0;
        bufferInfo.range  = vk::WholeSize;

        vk::WriteDescriptorSet write{};
        write.dstSet          = *m_meshDataDescriptorSet;
        write.dstBinding      = 0;
        write.descriptorCount = 1;
        write.descriptorType  = vk::DescriptorType::eStorageBuffer;
        write.pBufferInfo     = &bufferInfo;

        m_device.rawDevice().updateDescriptorSets({ write }, {});
    }

    void VulkanRHI::refreshMeshData()
    {
        for (uint32_t i = 0; i < meshes.size(); ++i)
            updateMeshMetadata(i);
        updateMeshDataSSBO();
    }


    // ============================================================================
    // Private - compaction
    // ============================================================================

    void VulkanRHI::compactBuffer(vk::raii::Buffer& buffer,
                                  vk::DeviceSize    removeByteOffset,
                                  vk::DeviceSize    removeByteSize,
                                  vk::DeviceSize&   totalBytesFilled)
    {
        const vk::DeviceSize tailSize = totalBytesFilled - removeByteOffset - removeByteSize;
        if (tailSize == 0)
        {
            totalBytesFilled -= removeByteSize;
            return;
        }

        vk::raii::Buffer       scratch({});
        vk::raii::DeviceMemory scratchMem({});
        createBuffer(tailSize,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            scratch, scratchMem
        );

        {
            auto cmd = beginSingleTimeCommands();
            cmd->copyBuffer(*buffer, *scratch, vk::BufferCopy{ removeByteOffset + removeByteSize, 0, tailSize });
            endSingleTimeCommands(*cmd);
        }

        {
            auto cmd = beginSingleTimeCommands();
            cmd->copyBuffer(*scratch, *buffer, vk::BufferCopy{ 0, removeByteOffset, tailSize });
            endSingleTimeCommands(*cmd);
        }

        totalBytesFilled -= removeByteSize;
    }

    void VulkanRHI::compactGeometry(uint32_t removedVertexOffset, uint32_t removedFirstIndex,
                                    uint32_t removedVertexCount,  uint32_t removedIndexCount)
    {
        compactBuffer(m_globalVertexBuffer,
            removedVertexOffset * sizeof(Vertex),
            removedVertexCount  * sizeof(Vertex),
            m_vertexBytesFilled
        );

        compactBuffer(m_globalIndexBuffer,
            removedFirstIndex  * sizeof(uint32_t),
            removedIndexCount  * sizeof(uint32_t),
            m_indexBytesFilled
        );

        m_totalVertexCount -= removedVertexCount;
        m_totalIndexCount  -= removedIndexCount;

        m_meshCache.patchAfterCompaction(
            removedVertexOffset, removedFirstIndex,
            removedVertexCount,  removedIndexCount
        );

        for (uint32_t i = 0; i < static_cast<uint32_t>(meshes.size()); ++i)
        {
            auto& data   = m_meshData[i];
            auto& cmd    = m_indirectCommands[i];
            bool patched = false;

            if (data.vertexOffset >= removedVertexOffset + removedVertexCount)
            {
                data.vertexOffset -= removedVertexCount;
                cmd.vertexOffset   = static_cast<int32_t>(data.vertexOffset);
                patched            = true;
            }
            if (data.firstIndex >= removedFirstIndex + removedIndexCount)
            {
                data.firstIndex -= removedIndexCount;
                cmd.firstIndex   = data.firstIndex;
                patched          = true;
            }

            if (patched)
            {
                static_cast<GPUMeshData*>(m_meshDataSSBOMapped)[i]                      = data;
                static_cast<vk::DrawIndexedIndirectCommand*>(m_indirectBufferMapped)[i] = cmd;
            }
        }
    }
} // namespace gcep::rhi::vulkan
