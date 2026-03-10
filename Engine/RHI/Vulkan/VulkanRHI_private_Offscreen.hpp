#pragma once
// Private offscreen render target declarations — included only inside VulkanRHI.

// Methods
void createOffscreenResources(uint32_t width, uint32_t height);
void resizeOffscreenResources(uint32_t width, uint32_t height);

// Members
vk::Extent2D           m_offscreenExtent{};
vk::raii::Image        m_offscreenImage            = nullptr;
vk::raii::DeviceMemory m_offscreenImageMemory      = nullptr;
vk::raii::ImageView    m_offscreenImageView        = nullptr;
vk::raii::Image        m_offscreenDepthImage       = nullptr;
vk::raii::DeviceMemory m_offscreenDepthImageMemory = nullptr;
vk::raii::ImageView    m_offscreenDepthImageView   = nullptr;
vk::raii::Sampler      m_offscreenSampler          = nullptr;

/// Registered via ImGui_ImplVulkan_AddTexture; displayed by ImGui::Image().
VkDescriptorSet        m_imguiTextureDescriptor    = VK_NULL_HANDLE;

bool     m_offscreenResizePending = false;
uint32_t m_pendingOffscreenWidth  = 0;
uint32_t m_pendingOffscreenHeight = 0;
