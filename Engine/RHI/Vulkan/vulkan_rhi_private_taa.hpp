#pragma once
// Private TAA declarations - included only inside VulkanRHI.
// Motion vectors - history - resolve - halton jitter - unjittered matrix cache.

// Methods
void createTAAResources(uint32_t width, uint32_t height);
void destroyTAAResources();
void recordTAAPass();
[[nodiscard]] static glm::vec2 haltonJitter(uint32_t frameIndex,
                                            uint32_t width, uint32_t height);

// Members

float    m_taaBlendAlpha = 0.10f; ///< Current-frame blend weight.
uint32_t m_taaFrameIndex = 0;     ///< Monotonically increasing; drives halton index.

// Motion-vector render target (written by scene pass as attachment 1)
vk::raii::Image        m_motionImage         = nullptr;
vk::raii::DeviceMemory m_motionImageMemory   = nullptr;
vk::raii::ImageView    m_motionImageView      = nullptr;
vk::Format             m_motionVectorsFormat  = vk::Format::eR16G16Sfloat;

// History buffer - previous resolved frame
vk::raii::Image        m_taaHistoryImage       = nullptr;
vk::raii::DeviceMemory m_taaHistoryImageMemory = nullptr;
vk::raii::ImageView    m_taaHistoryImageView   = nullptr;

// Resolved output written by TAA compute, copied back to history each frame
vk::raii::Image        m_taaResolvedImage       = nullptr;
vk::raii::DeviceMemory m_taaResolvedImageMemory = nullptr;
vk::raii::ImageView    m_taaResolvedImageView   = nullptr;

vk::raii::Sampler      m_taaSampler = nullptr; ///< Bilinear, shared by resolve shader.

// TAA compute pipeline + descriptor set
vk::raii::DescriptorSetLayout m_taaSetLayout      = nullptr;
vk::raii::DescriptorPool      m_taaPool           = nullptr;
vk::raii::DescriptorSet       m_taaSet            = nullptr;
vk::raii::PipelineLayout      m_taaPipelineLayout = nullptr;
vk::raii::Pipeline            m_taaPipeline       = nullptr;

/// Previous-frame unjittered VP - motion vector reference (no jitter noise).
glm::mat4 m_prevViewProj       = glm::mat4(1.0f);

/// Current-frame unjittered VP - used by all passes except the main scene draw.
glm::mat4 m_unjitteredViewProj = glm::mat4(1.0f);
