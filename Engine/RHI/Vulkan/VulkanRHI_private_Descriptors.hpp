#pragma once
// Private descriptor-related declarations — included only inside VulkanRHI.
// Scene UBO (set 2) - bindless textures (set 1) - mesh-data SSBO (set 0)

// Scene UBO
void createSceneUBOLayout();
void createSceneUBOPool();
void createSceneUBOBuffers();
void createSceneUBOSets();
void updateSceneUBOSets();

// Bindless texture array
void createBindlessLayout();
void createBindlessPool();
void createBindlessSet();

// Mesh data SSBO
void createMeshDataDescriptors();
void updateMeshDataSet();

// Members

// Scene UBO - set 2, binding 0
std::vector<vk::raii::Buffer>        m_uniformBuffers;
std::vector<vk::raii::DeviceMemory>  m_uniformBuffersMemory;
std::vector<void*>                   m_uniformBuffersMapped; ///< Persistently mapped.
vk::raii::DescriptorSetLayout        m_UBOLayout = nullptr;
vk::raii::DescriptorPool             m_UBOPool   = nullptr;
std::vector<vk::raii::DescriptorSet> m_UBOSets;

// Bindless texture array - set 1, binding 0
vk::raii::DescriptorSetLayout m_bindlessLayout = nullptr;
vk::raii::DescriptorPool      m_bindlessPool   = nullptr;
vk::raii::DescriptorSet       m_bindlessSet    = nullptr;

// Mesh data SSBO - set 0, binding 0
vk::raii::DescriptorSetLayout m_meshDataDescriptorSetLayout = nullptr;
vk::raii::DescriptorPool      m_meshDataDescriptorPool      = nullptr;
vk::raii::DescriptorSet       m_meshDataDescriptorSet       = nullptr;
vk::raii::Buffer              m_meshDataSSBO                = nullptr;
vk::raii::DeviceMemory        m_meshDataSSBOMemory          = nullptr;
void*                         m_meshDataSSBOMapped          = nullptr; ///< Never unmapped.
