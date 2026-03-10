#pragma once
// Private pipeline declarations — included only inside VulkanRHI.
// Frustum-cull compute · main scene graphics · infinite grid.

// Methods
void createCullPipeline();
void recordCullPass();
[[nodiscard]] FrustumPlanes extractFrustum(const glm::mat4& viewProj) const;

void createGraphicsPipeline();

void createGridPipeline();
void recordGridPass();

// Members

// Frustum-cull compute pipeline
vk::raii::Pipeline            m_cullPipeline       = nullptr;
vk::raii::PipelineLayout      m_cullPipelineLayout = nullptr;
vk::raii::DescriptorSetLayout m_cullSetLayout      = nullptr;
vk::raii::DescriptorPool      m_cullPool           = nullptr;
vk::raii::DescriptorSet       m_cullSet            = nullptr;

// Main scene graphics pipeline
vk::raii::PipelineLayout m_pipelineLayout   = nullptr;
vk::raii::Pipeline       m_graphicsPipeline = nullptr;

// Infinite grid pipeline
vk::raii::PipelineLayout m_gridPipelineLayout = nullptr;
vk::raii::Pipeline       m_gridPipeline       = nullptr;
