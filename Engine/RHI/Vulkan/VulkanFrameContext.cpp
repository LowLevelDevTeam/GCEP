#include "VulkanFrameContext.hpp"

namespace gcep::rhi::vulkan
{

VulkanFrameContext::VulkanFrameContext(const vk::raii::Device& device, uint32_t queueFamily):
    commandPool(device, vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eTransient,
        queueFamily
    }),
    commandBuffer(std::move(
        vk::raii::CommandBuffers(device, vk::CommandBufferAllocateInfo{
            *commandPool,
            vk::CommandBufferLevel::ePrimary,
            1
        }).front())),
    inFlightFence(device, vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled }),
    presentCompleteSemaphore (device, vk::SemaphoreCreateInfo{})
{
    commandList = std::make_unique<VulkanCommandList>(*commandBuffer);
}

void VulkanFrameContext::waitAndReset(const vk::raii::Device& device)
{
    const vk::Result result = device.waitForFences(*inFlightFence, vk::True, UINT64_MAX);
    if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("VulkanFrameContext::waitAndReset: fence wait failed: " + vk::to_string(result));
    }

    device.resetFences(*inFlightFence);
    commandPool.reset(vk::CommandPoolResetFlags{});
}

} // Namespace gcep::rhi::vulkan
