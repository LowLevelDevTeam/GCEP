#include "VulkanRHI.hpp"

// Externals
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// STL
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "Engine/Core/RHI/ObjParser/ObjParser.hpp"

namespace gcep::rhi::vulkan
{

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto* rhi = static_cast<VulkanRHI*>(glfwGetWindowUserPointer(window));
    if (rhi)
    {
        rhi->setFramebufferResized(true);
    }
}

VulkanRHI::VulkanRHI(const gcep::rhi::SwapchainDesc& desc) : m_swapchainDesc(desc) {}

void VulkanRHI::setWindow(GLFWwindow* window)
{
    m_swapchainDesc.nativeWindowHandle = window;
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void VulkanRHI::initRHI()
{
    m_device.createVulkanDevice(m_swapchainDesc);

    vk::CommandPoolCreateInfo createInfo{};
    createInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    createInfo.queueFamilyIndex = m_device.queueFamily();
    m_uploadCommandPool         = vk::raii::CommandPool(m_device.rawDevice(), createInfo);

    initSceneResources();
}

void VulkanRHI::initSceneResources()
{
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
}

void VulkanRHI::recreateSwapchain()
{
    m_framebufferResized = false;
    auto* win = static_cast<GLFWwindow*>(m_swapchainDesc.nativeWindowHandle);
    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    while (w == 0 || h == 0)
    {
        glfwGetFramebufferSize(win, &w, &h);
        glfwWaitEvents();
    }
    m_device.recreateSwapchain(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
}

ImGui_ImplVulkan_InitInfo VulkanRHI::getInitInfo()
{
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 },
    };
    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCI.maxSets       = 1000;
    poolCI.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolCI.pPoolSizes    = poolSizes;

    if (vkCreateDescriptorPool(*m_device.rawDevice(), &poolCI, nullptr, &m_descriptorPoolImGui) != VK_SUCCESS)
    {
        throw std::runtime_error("[Vulkan_RHI] Failed to create ImGui descriptor pool");
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance       = *m_device.rawInstance();
    initInfo.PhysicalDevice = *m_device.rawPhysDevice();
    initInfo.Device         = *m_device.rawDevice();
    initInfo.QueueFamily    = m_device.queueFamily();
    initInfo.Queue          = *m_device.rawQueue();
    initInfo.DescriptorPool = m_descriptorPoolImGui;
    initInfo.PipelineCache  = VK_NULL_HANDLE;
    initInfo.MinImageCount  = m_device.swapchainImageCount();
    initInfo.ImageCount     = m_device.swapchainImageCount();
    initInfo.Allocator      = nullptr;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    m_swapchainFormat = m_device.swapchainFormat();
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = reinterpret_cast<VkFormat*>(&m_swapchainFormat);
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat   = static_cast<VkFormat>(findDepthFormat());
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    return initInfo;
}

void VulkanRHI::cleanup()
{
    m_device.waitIdle();

    if (m_imguiTextureDescriptor != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(m_imguiTextureDescriptor);
        m_imguiTextureDescriptor = VK_NULL_HANDLE;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPoolImGui != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(*m_device.rawDevice(), m_descriptorPoolImGui, nullptr);
        m_descriptorPoolImGui = VK_NULL_HANDLE;
    }
}

void VulkanRHI::drawFrame()
{
    if (m_framebufferResized)
    {
        recreateSwapchain();
    }

    if (!m_device.beginFrame()) return;

    updateUniformBuffer();

    const bool offscreenReady =
        (*m_offscreenImage != VK_NULL_HANDLE) &&
        (*m_offscreenImageView != VK_NULL_HANDLE) &&
        (*m_offscreenDepthImage != VK_NULL_HANDLE) &&
        (*m_offscreenDepthImageView != VK_NULL_HANDLE);

    if (offscreenReady)
    {
        recordOffscreenCommandBuffer();
    }

    ImGui::Render();
    recordImGuiCommandBuffer();

    m_device.endFrame();
}

void VulkanRHI::updateEditorInfo(ImVec4& clearColor)
{
    m_clearColor = clearColor;
}

void VulkanRHI::requestOffscreenResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return;
    if (width == m_offscreenExtent.width && height == m_offscreenExtent.height) return;

    m_offscreenResizePending  = true;
    m_pendingOffscreenWidth   = width;
    m_pendingOffscreenHeight  = height;
}

void VulkanRHI::processPendingOffscreenResize()
{
    if (m_offscreenResizePending)
    {
        m_offscreenResizePending = false;
        resizeOffscreenResources(m_pendingOffscreenWidth, m_pendingOffscreenHeight);
    }
}

void VulkanRHI::createOffscreenResources(uint32_t width, uint32_t height)
{
    m_offscreenExtent.width = width;
    m_offscreenExtent.height = height;

    const uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    // Color image
    createImage(
        width, height, mipLevels,
        m_device.swapchainFormat(),
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_offscreenImage, m_offscreenImageMemory
    );

    m_offscreenImageView = createImageView(m_offscreenImage, m_device.swapchainFormat(), vk::ImageAspectFlagBits::eColor, mipLevels);

    // Depth image
    const vk::Format depthFormat = findDepthFormat();
    createImage(
        width, height, 1,
        depthFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_offscreenDepthImage, m_offscreenDepthImageMemory
    );
    m_offscreenDepthImageView = createImageView(m_offscreenDepthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter     = vk::Filter::eLinear;
    samplerInfo.minFilter     = vk::Filter::eLinear;
    samplerInfo.mipmapMode    = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU  = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV  = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW  = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.minLod        = 0.0f;
    samplerInfo.maxLod        = vk::LodClampNone;
    samplerInfo.maxAnisotropy = 1.0f;

    m_offscreenSampler = vk::raii::Sampler(m_device.rawDevice(), samplerInfo);

    {
        auto cmd = beginSingleTimeCommands();
        vk::ImageMemoryBarrier2 barrier{};
        barrier.srcStageMask  = vk::PipelineStageFlagBits2::eTopOfPipe;
        barrier.srcAccessMask = vk::AccessFlagBits2::eNone;
        barrier.dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
        barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        barrier.oldLayout     = vk::ImageLayout::eUndefined;
        barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.image         = m_offscreenImage;
        barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 };
        vk::DependencyInfo depInfo{};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers    = &barrier;
        cmd->pipelineBarrier2(depInfo);
        endSingleTimeCommands(*cmd);
    }

    m_imguiTextureDescriptor = ImGui_ImplVulkan_AddTexture(
        *m_offscreenSampler,
        *m_offscreenImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void VulkanRHI::resizeOffscreenResources(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return;
    if (width == m_offscreenExtent.width && height == m_offscreenExtent.height) return;

    m_device.waitIdle();

    if (m_imguiTextureDescriptor != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(m_imguiTextureDescriptor);
        m_imguiTextureDescriptor = VK_NULL_HANDLE;
    }

    // Clear old resources
    m_offscreenImageView        = nullptr;
    m_offscreenImage            = nullptr;
    m_offscreenImageMemory      = nullptr;
    m_offscreenSampler          = nullptr;
    m_offscreenDepthImageView   = nullptr;
    m_offscreenDepthImage       = nullptr;
    m_offscreenDepthImageMemory = nullptr;

    // Recreate with new size
    createOffscreenResources(width, height);
}

void VulkanRHI::createTextureImage()
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("TestTextures/viking_room.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        throw std::runtime_error("[Vulkan_RHI] Failed to load texture: TestTextures/viking_room.png");
    }

    vk::DeviceSize imageSize = texWidth * texHeight * 4;
    m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    vk::raii::Buffer       stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    createImage(
        static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight),
        m_mipLevels,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_textureImage, m_textureImageMemory
    );

    transitionImageLayout(m_textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, m_mipLevels);
    copyBufferToImage(stagingBuffer, m_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    generateMipmaps(m_textureImage, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, m_mipLevels);
}

void VulkanRHI::createTextureImageView()
{
    m_textureImageView = createImageView(m_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, m_mipLevels);
}

void VulkanRHI::createTextureSampler()
{
    const auto properties = m_device.rawPhysDevice().getProperties();

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter        = vk::Filter::eLinear;
    samplerInfo.minFilter        = vk::Filter::eLinear;
    samplerInfo.mipmapMode       = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW     = vk::SamplerAddressMode::eRepeat;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.anisotropyEnable = vk::True;
    samplerInfo.maxAnisotropy    = properties.limits.maxSamplerAnisotropy;
    samplerInfo.compareEnable    = vk::False;
    samplerInfo.compareOp        = vk::CompareOp::eAlways;
    samplerInfo.minLod           = 0.0f; // static_cast<float>(m_mipLevels); // to tweak between 0 and m_mipLevels
    samplerInfo.maxLod           = vk::LodClampNone;

    m_textureSampler = vk::raii::Sampler(m_device.rawDevice(), samplerInfo);
}

void VulkanRHI::loadModel()
{
    static constexpr bool deduplication = true;
    const std::filesystem::path filepath = "TestTextures/viking_room.obj";
    auto [attrib, modelIndices] = objParser::ObjLoader::loadObj(filepath);

    std::unordered_map<Vertex, uint32_t> uniqueVertices;
    auto start_timer = std::chrono::high_resolution_clock::now();

    vertices.reserve(modelIndices.size());
    indices.reserve(modelIndices.size());

    const float* verts     = attrib.vertices.data();
    const float* texcoords = attrib.texcoords.data();
    const float* normals   = attrib.normals.data();

    for (const auto& idx : modelIndices)
    {
        Vertex vertex{};
        const float* v = verts + 3 * idx.vertex_index;
        vertex.pos = { v[0], v[1], v[2] };

        if (idx.texcoord_index != UINT32_MAX)
        {
            const float* tc = texcoords + 2 * idx.texcoord_index;
            vertex.texCoord = { tc[0], 1.0f - tc[1] };
        }
        else
        {
            vertex.texCoord = { 0.0f, 0.0f };
        }

        if (idx.normal_index != UINT32_MAX)
        {
            const float* n = normals + 3 * idx.normal_index;
            vertex.color = { n[0], n[1], n[2] };
        }
        else
        {
            vertex.color = { 1.0f, 1.0f, 1.0f };
        }

        if constexpr (deduplication)
        {
            auto [it, inserted] = uniqueVertices.try_emplace(vertex, static_cast<uint32_t>(vertices.size()));

            if (inserted)
            {
                vertices.push_back(vertex);
            }

            indices.push_back(it->second);
        }
        else
        {
            vertices.push_back(vertex);
            indices.push_back(indices.size());
        }
    }

    numIndices = static_cast<uint32_t>(indices.size());

    if constexpr (deduplication)
    {
        auto end_timer = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_timer - start_timer);
        std::cout << "Deduplicated: " << modelIndices.size() << " original vertices -> " << vertices.size()
                  << " unique vertices (" << (100.0f * vertices.size() / modelIndices.size()) << "% were unique vertices)\n";
        std::cout << "Deduplication took " << duration.count() << "ms." << '\n';
    }
}

void VulkanRHI::createVertexBuffer()
{
    const vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    vk::raii::Buffer       stagingBuffer       = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;

    createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory
    );

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    stagingBufferMemory.unmapMemory();
    vertices.clear(); vertices.shrink_to_fit();

    createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_vertexBuffer, m_vertexBufferMemory
    );

    copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
}

void VulkanRHI::createIndexBuffer()
{
    const vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    vk::raii::Buffer       stagingBuffer       = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;
    createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory
    );

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    std::memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    stagingBufferMemory.unmapMemory();
    indices.clear(); indices.shrink_to_fit();

    createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_indexBuffer, m_indexBufferMemory
    );

    copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
}

void VulkanRHI::createUniformBuffers()
{
    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();
    m_uniformBuffersMapped.clear();

    const vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vk::raii::Buffer       buffer       = nullptr;
        vk::raii::DeviceMemory bufferMemory = nullptr;

        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            buffer, bufferMemory
        );

        m_uniformBuffers.emplace_back(std::move(buffer));
        m_uniformBuffersMemory.emplace_back(std::move(bufferMemory));
        m_uniformBuffersMapped.emplace_back(m_uniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
}

void VulkanRHI::createDescriptorSetLayout()
{
    std::array bindings =
    {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
    };
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());

    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(m_device.rawDevice(), layoutInfo);
}

void VulkanRHI::createDescriptorPool()
{
    std::array poolSizes =
    {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT),
    };
    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    m_descriptorPool = vk::raii::DescriptorPool(m_device.rawDevice(), poolInfo);
}

void VulkanRHI::createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *m_descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool     = *m_descriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts        = layouts.data();

    m_descriptorSets.clear();
    m_descriptorSets = m_device.rawDevice().allocateDescriptorSets(allocInfo);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = *m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(UniformBufferObject);

        vk::DescriptorImageInfo imageInfo{};
        imageInfo.sampler     = m_textureSampler;
        imageInfo.imageView   = m_textureImageView;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::WriteDescriptorSet uboWrite{};
        uboWrite.dstSet          = m_descriptorSets[i];
        uboWrite.dstBinding      = 0;
        uboWrite.descriptorCount = 1;
        uboWrite.descriptorType  = vk::DescriptorType::eUniformBuffer;
        uboWrite.pBufferInfo     = &bufferInfo;

        vk::WriteDescriptorSet texWrite{};
        texWrite.dstSet          = m_descriptorSets[i];
        texWrite.dstBinding      = 1;
        texWrite.descriptorCount = 1;
        texWrite.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
        texWrite.pImageInfo      = &imageInfo;

        m_device.rawDevice().updateDescriptorSets({ uboWrite, texWrite }, {});
    }
}

void VulkanRHI::updateUniformBuffer()
{
    const auto currentTime = std::chrono::high_resolution_clock::now();
    const float time = std::chrono::duration<float>(currentTime - m_startTime).count();

    const float aspect = (m_offscreenExtent.width > 0 && m_offscreenExtent.height > 0)
        ? static_cast<float>(m_offscreenExtent.width) / static_cast<float>(m_offscreenExtent.height)
        : 1.0f;

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj  = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        std::memcpy(m_uniformBuffersMapped[i], &ubo, sizeof(ubo));
}

void VulkanRHI::createGraphicsPipeline()
{
    auto shaderCode = readShader("Base_Shader.spv");
    vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo{};
    vertexShaderStageInfo.stage     = vk::ShaderStageFlagBits::eVertex;
    vertexShaderStageInfo.module    = shaderModule;
    vertexShaderStageInfo.pName     = "vertMain";

    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo{};
    fragmentShaderStageInfo.stage     = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderStageInfo.module    = shaderModule;
    fragmentShaderStageInfo.pName     = "fragMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageInfo, fragmentShaderStageInfo};

    std::vector dynamicStates =
    {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.depthClampEnable        = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode             = vk::PolygonMode::eFill;
    rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace               = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable         = vk::False;
    rasterizer.depthBiasSlopeFactor    = 1.0f;
    rasterizer.lineWidth               = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable  = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable       = vk::True;
    depthStencil.depthWriteEnable      = vk::True;
    depthStencil.depthCompareOp        = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable     = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable    = vk::False;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable   = vk::False;
    colorBlending.logicOp         = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &*m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    m_pipelineLayout = vk::raii::PipelineLayout(m_device.rawDevice(), pipelineLayoutInfo);

    m_swapchainFormat = m_device.swapchainFormat();
    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapchainFormat;
    pipelineRenderingCreateInfo.depthAttachmentFormat   = findDepthFormat();

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pNext               = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = nullptr;

    m_graphicsPipeline = vk::raii::Pipeline(m_device.rawDevice(), nullptr, pipelineInfo);
}

void VulkanRHI::recordOffscreenCommandBuffer()
{
    const vk::CommandBuffer cmd = m_device.currentCommandBuffer();

    const uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_offscreenExtent.width, m_offscreenExtent.height)))) + 1;

    vk::ImageMemoryBarrier2 colorBarrier{};
    colorBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
    colorBarrier.srcAccessMask = vk::AccessFlagBits2::eShaderRead;
    colorBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    colorBarrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    colorBarrier.oldLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    colorBarrier.newLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    colorBarrier.image         = m_offscreenImage;
    colorBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 };

    vk::ImageMemoryBarrier2 depthBarrier{};
    depthBarrier.srcStageMask  = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    depthBarrier.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    depthBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    depthBarrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    depthBarrier.oldLayout     = vk::ImageLayout::eUndefined;
    depthBarrier.newLayout     = vk::ImageLayout::eDepthAttachmentOptimal;
    depthBarrier.image         = m_offscreenDepthImage;
    depthBarrier.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 };

    std::array barriers = { colorBarrier, depthBarrier };
    vk::DependencyInfo depInfo{};
    depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    depInfo.pImageMemoryBarriers    = barriers.data();
    cmd.pipelineBarrier2(depInfo);

    // Begin dynamic rendering
    const vk::ClearValue clearColor = vk::ClearColorValue(m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w);
    const vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.imageView   = m_offscreenImageView;
    colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachmentInfo.clearValue  = clearColor;

    vk::RenderingAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.imageView   = m_offscreenDepthImageView;
    depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    depthAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eDontCare;
    depthAttachmentInfo.clearValue  = clearDepth;

    vk::Rect2D renderArea{};
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent = m_offscreenExtent;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea               = renderArea;
    renderingInfo.layerCount               = 1;
    renderingInfo.colorAttachmentCount     = 1;
    renderingInfo.pColorAttachments        = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment         = &depthAttachmentInfo;

    cmd.beginRendering(renderingInfo);

    // Draw scene
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);
    cmd.setViewport(0, vk::Viewport(
        0.0f, 0.0f,
        static_cast<float>(m_offscreenExtent.width),
        static_cast<float>(m_offscreenExtent.height),
        0.0f, 1.0f)
    );
    cmd.setScissor(0, vk::Rect2D({ 0, 0 }, m_offscreenExtent));

    cmd.bindVertexBuffers(0, *m_vertexBuffer, {0});
    cmd.bindIndexBuffer(*m_indexBuffer, 0, vk::IndexType::eUint32);

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipelineLayout, 0, *m_descriptorSets[0], nullptr);

    cmd.drawIndexed(numIndices, 1, 0, 0, 0);
    cmd.endRendering();

    vk::ImageMemoryBarrier2 toShaderRead{};
    toShaderRead.srcStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    toShaderRead.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    toShaderRead.dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
    toShaderRead.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    toShaderRead.oldLayout     = vk::ImageLayout::eColorAttachmentOptimal;
    toShaderRead.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    toShaderRead.image         = *m_offscreenImage;
    toShaderRead.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 };

    vk::DependencyInfo dep2{};
    dep2.imageMemoryBarrierCount = 1;
    dep2.pImageMemoryBarriers    = &toShaderRead;
    cmd.pipelineBarrier2(dep2);
}

void VulkanRHI::recordImGuiCommandBuffer()
{
    const vk::CommandBuffer cmd       = m_device.currentCommandBuffer();
    const vk::Image         swapImage = m_device.currentSwapchainImage();

    transitionImageInFrame(swapImage,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits2::eNone, vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor, 1
    );

    const uint32_t      imageIdx = m_device.currentImageIndex();
    const vk::ImageView swapView = *m_device.swapchainImageViews()[imageIdx];

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView   = swapView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue  = vk::ClearColorValue(m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea.offset.x      = 0;
    renderingInfo.renderArea.offset.y      = 0;
    renderingInfo.renderArea.extent.width  = m_device.swapchainExtent().width;
    renderingInfo.renderArea.extent.height = m_device.swapchainExtent().height;
    renderingInfo.layerCount               = 1;
    renderingInfo.colorAttachmentCount     = 1;
    renderingInfo.pColorAttachments        = &colorAttachment;

    cmd.beginRendering(renderingInfo);

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

    cmd.endRendering();

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    transitionImageInFrame(swapImage,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor, 1
    );
}

void VulkanRHI::createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                            vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                            vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
{
    vk::ImageCreateInfo createInfo{};
    createInfo.imageType     = vk::ImageType::e2D;
    createInfo.format        = format;
    createInfo.extent.width  = width;
    createInfo.extent.height = height;
    createInfo.extent.depth  = 1;
    createInfo.mipLevels     = mipLevels;
    createInfo.arrayLayers   = 1;
    createInfo.samples       = vk::SampleCountFlagBits::e1;
    createInfo.tiling        = tiling;
    createInfo.usage         = usage;
    createInfo.sharingMode   = vk::SharingMode::eExclusive;

    image = vk::raii::Image(m_device.rawDevice(), createInfo);
    const auto memoryRequirements = image.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

    imageMemory = vk::raii::DeviceMemory(m_device.rawDevice(), allocInfo);
    image.bindMemory(imageMemory, 0);
}

vk::raii::ImageView VulkanRHI::createImageView(vk::raii::Image& image, vk::Format format,
                                               vk::ImageAspectFlags aspectFlags, uint32_t mipLevels)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image            = *image;
    viewInfo.viewType         = vk::ImageViewType::e2D;
    viewInfo.format           = format;
    viewInfo.subresourceRange = {aspectFlags, 0, mipLevels, 0, 1 };

    return vk::raii::ImageView(m_device.rawDevice(), viewInfo);
}

void VulkanRHI::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                             vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufMem)
{
    vk::BufferCreateInfo createInfo{};
    createInfo.size        = size;
    createInfo.usage       = usage;
    createInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = vk::raii::Buffer(m_device.rawDevice(), createInfo);
    const auto memoryRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo(memoryRequirements.size, findMemoryType(memoryRequirements.memoryTypeBits, properties));
    bufMem = vk::raii::DeviceMemory(m_device.rawDevice(), allocInfo);
    buffer.bindMemory(*bufMem, 0);
}

void VulkanRHI::copyBuffer(vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size)
{
    auto cmd = beginSingleTimeCommands();
    cmd->copyBuffer(*src, *dst, vk::BufferCopy(0, 0, size));
    endSingleTimeCommands(*cmd);
}

void VulkanRHI::copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image,
                                  uint32_t width, uint32_t height)
{
    auto cmd = beginSingleTimeCommands();

    vk::BufferImageCopy region{};
    region.imageSubresource  = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    region.imageExtent.width  = width;
    region.imageExtent.height = height;
    region.imageExtent.depth  = 1;
    cmd->copyBufferToImage(*buffer, *image, vk::ImageLayout::eTransferDstOptimal, { region });

    endSingleTimeCommands(*cmd);
}

void VulkanRHI::transitionImageLayout(const vk::raii::Image& image,
                                      vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels)
{
    auto cmd = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout    = oldLayout;
    barrier.newLayout    = newLayout;
    barrier.image        = *image;
    barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 };

    vk::PipelineStageFlags srcStage, dstStage;
    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        srcStage = vk::PipelineStageFlagBits::eTransfer;
        dstStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else { throw std::invalid_argument("Vulkan_RHI::transitionImageLayout: unsupported transition"); }

    cmd->pipelineBarrier(srcStage, dstStage, {}, {}, {}, barrier);
    endSingleTimeCommands(*cmd);
}

void VulkanRHI::transitionImageInFrame(vk::Image image,
                                       vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                       vk::AccessFlags2 srcAccess, vk::AccessFlags2 dstAccess,
                                       vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage,
                                       vk::ImageAspectFlags aspect, uint32_t mipLevels)
{
    vk::ImageMemoryBarrier2 barrier{};
    barrier.srcStageMask        = srcStage;
    barrier.srcAccessMask       = srcAccess;
    barrier.dstStageMask        = dstStage;
    barrier.dstAccessMask       = dstAccess;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image               = image;
    barrier.subresourceRange    = {aspect, 0, mipLevels, 0, 1 };

    vk::DependencyInfo dependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers    = &barrier;

    m_device.currentCommandBuffer().pipelineBarrier2(dependencyInfo);
}

void VulkanRHI::generateMipmaps(vk::raii::Image& image, vk::Format format,
                                int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    // Check linear filtering support
    const auto props = m_device.rawPhysDevice().getFormatProperties(format);
    if (!(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
        throw std::runtime_error("[Vulkan_RHI] generateMipmaps: format does not support linear blitting");

    auto cmd = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.srcQueueFamilyIndex         = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex         = vk::QueueFamilyIgnored;
    barrier.image                       = *image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipW = texWidth, mipH = texHeight;
    for (uint32_t i = 1; i < mipLevels; ++i)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout     = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        cmd->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, barrier
        );

        vk::ImageBlit blit{};
        blit.srcSubresource = { vk::ImageAspectFlagBits::eColor, i - 1, 0, 1 };
        blit.srcOffsets[0]  = vk::Offset3D(0, 0, 0);
        blit.srcOffsets[1]  = vk::Offset3D(mipW, mipH, 1);
        blit.dstSubresource = { vk::ImageAspectFlagBits::eColor, i, 0, 1 };
        blit.dstOffsets[0]  = vk::Offset3D(0, 0, 0);
        blit.dstOffsets[1]  = vk::Offset3D(mipW > 1 ? mipW / 2 : 1, mipH > 1 ? mipH / 2 : 1, 1);
        cmd->blitImage(*image, vk::ImageLayout::eTransferSrcOptimal,
                       *image, vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);

        barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        cmd->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
            {}, {}, {}, barrier
        );

        if (mipW > 1) mipW /= 2;
        if (mipH > 1) mipH /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout     = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    cmd->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
        {}, {}, {}, barrier
    );

    endSingleTimeCommands(*cmd);
}

std::unique_ptr<vk::raii::CommandBuffer> VulkanRHI::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool        = *m_uploadCommandPool;
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    auto cmd = std::make_unique<vk::raii::CommandBuffer>(
        std::move(vk::raii::CommandBuffers(m_device.rawDevice(), allocInfo).front())
    );

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd->begin(beginInfo);
    return cmd;
}

void VulkanRHI::endSingleTimeCommands(vk::raii::CommandBuffer& cmd)
{
    cmd.end();
    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &*cmd;
    m_device.rawQueue().submit(submitInfo, nullptr);
    m_device.rawQueue().waitIdle();
}

uint32_t VulkanRHI::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags props) const
{
    const auto memProps = m_device.rawPhysDevice().getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
    }
    throw std::runtime_error("[Vulkan_RHI] findMemoryType: no suitable type found");
}

vk::Format VulkanRHI::findDepthFormat() const
{
    return findSupportedFormat(
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

vk::Format VulkanRHI::findSupportedFormat(const std::vector<vk::Format>& candidates,
                                          vk::ImageTiling tiling, vk::FormatFeatureFlags features) const
{
    for (const auto fmt : candidates)
    {
        const auto fmtProps = m_device.rawPhysDevice().getFormatProperties(fmt);
        if (tiling == vk::ImageTiling::eLinear  && (fmtProps.linearTilingFeatures  & features) == features) return fmt;
        if (tiling == vk::ImageTiling::eOptimal && (fmtProps.optimalTilingFeatures & features) == features) return fmt;
    }

    throw std::runtime_error("[Vulkan_RHI] findSupportedFormat: no suitable format found");
}

std::vector<char> VulkanRHI::readShader(const std::string& fileName)
{
    const auto path = std::filesystem::current_path() / "Shaders" / fileName;
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("[Vulkan_RHI] Cannot open shader: " + path.string());
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
}

vk::raii::ShaderModule VulkanRHI::createShaderModule(const std::vector<char>& code) const
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    return vk::raii::ShaderModule(m_device.rawDevice(), createInfo);
}

} // Namespace gcep::rhi::vulkan
