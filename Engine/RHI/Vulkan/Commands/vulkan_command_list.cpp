#include "vulkan_command_list.hpp"

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
} // namespace gcep::rhi::vulkan
