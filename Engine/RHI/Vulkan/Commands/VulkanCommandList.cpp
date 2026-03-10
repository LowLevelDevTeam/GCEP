#include "VulkanCommandList.hpp"

namespace gcep::rhi::vulkan
{

void VulkanCommandList::beginRecording()
{
    vk::CommandBufferBeginInfo bi{};
    bi.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    m_cmd.begin(bi);
}

void VulkanCommandList::endRecording()
{
    m_cmd.end();
}

} // Namespace gcep::rhi::vulkan
