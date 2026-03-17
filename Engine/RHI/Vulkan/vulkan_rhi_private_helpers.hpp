#pragma once
// Private low-level Vulkan helper declarations - included only inside VulkanRHI.

void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                 vk::Format format, vk::ImageTiling tiling,
                 vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
                 vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);

[[nodiscard]] vk::raii::ImageView createImageView(vk::raii::Image& image,
                                                  vk::Format format,
                                                  vk::ImageAspectFlags aspectFlags,
                                                  uint32_t mipLevels);

void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                  vk::MemoryPropertyFlags properties,
                  vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);

void copyBuffer(vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size);

void transitionImageInFrame(vk::Image image,
                            vk::ImageLayout oldLayout,        vk::ImageLayout newLayout,
                            vk::AccessFlags2 srcAccess,       vk::AccessFlags2 dstAccess,
                            vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage,
                            vk::ImageAspectFlags aspect,      uint32_t mipLevels);

[[nodiscard]] std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands();
void endSingleTimeCommands(vk::raii::CommandBuffer& cmd);

[[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter,
                                      vk::MemoryPropertyFlags props) const;
[[nodiscard]] vk::Format findDepthFormat() const;
[[nodiscard]] vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates,
                                              vk::ImageTiling tiling,
                                              vk::FormatFeatureFlags features) const;

[[nodiscard]] static std::vector<char> readShader(const std::string& fileName);
[[nodiscard]] vk::raii::ShaderModule   createShaderModule(const std::vector<char>& code) const;
