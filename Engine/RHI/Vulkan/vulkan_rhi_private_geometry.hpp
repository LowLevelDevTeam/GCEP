#pragma once
// Private geometry-related declarations — included only inside VulkanRHI.
// Global vertex/index buffers · mesh upload · GPU-driven indirect · compaction.

// Methods
void initMeshBuffers();
int  uploadSingleMesh(uint32_t meshIndex);
void updateMeshMetadata(uint32_t meshIndex);
void updateMeshDataSSBO();
void refreshMeshData();
void compactBuffer(vk::raii::Buffer& buffer,
                   vk::DeviceSize    removeByteOffset,
                   vk::DeviceSize    removeByteSize,
                   vk::DeviceSize&   totalBytesFilled);
void compactGeometry(uint32_t removedVertexOffset, uint32_t removedFirstIndex,
                     uint32_t removedVertexCount,  uint32_t removedIndexCount);

// Members

static constexpr vk::DeviceSize MAX_VERTEX_BUFFER_BYTES = 128 * 1024 * 1024;
static constexpr vk::DeviceSize MAX_INDEX_BUFFER_BYTES  =  64 * 1024 * 1024;

vk::raii::Buffer       m_globalVertexBuffer       = nullptr;
vk::raii::DeviceMemory m_globalVertexBufferMemory = nullptr;
vk::raii::Buffer       m_globalIndexBuffer        = nullptr;
vk::raii::DeviceMemory m_globalIndexBufferMemory  = nullptr;

vk::DeviceSize m_vertexBytesFilled = 0;
vk::DeviceSize m_indexBytesFilled  = 0;
uint32_t       m_totalVertexCount  = 0;
uint32_t       m_totalIndexCount   = 0;

// Indirect draw buffer + culling
vk::raii::Buffer       m_indirectBuffer       = nullptr;
vk::raii::DeviceMemory m_indirectBufferMemory = nullptr;
void*                  m_indirectBufferMapped   = nullptr;
vk::DeviceSize         m_indirectBufferCapacity = 0;

vk::raii::Buffer       m_drawCountBuffer       = nullptr;
vk::raii::DeviceMemory m_drawCountBufferMemory = nullptr;
void*                  m_drawCountMapped = nullptr;
uint32_t               m_drawCount       = 0;   ///< Lags by one frame — display only.

// CPU mirrors
std::vector<GPUMeshData>                    m_meshData;
std::vector<vk::DrawIndexedIndirectCommand> m_indirectCommands;
std::vector<Mesh>                           meshes;
MeshCache                                   m_meshCache;
std::unordered_map<std::string, TextureCacheEntry> m_textureCache;
