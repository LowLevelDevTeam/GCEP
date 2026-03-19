#pragma once

// Externals
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <ktxvulkan.h>

// STL
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <unordered_map>

namespace gcep::rhi::vulkan
{
    class VulkanRHI;
    struct TextureData
    {
        vk::raii::Image        image         = nullptr;
        vk::raii::DeviceMemory imageMemory   = nullptr;
        vk::raii::ImageView    imageView     = nullptr;
        vk::raii::Sampler      sampler       = nullptr;
        uint32_t               bindlessIndex = 0;
        uint32_t               mipLevels     = 1;
        uint32_t               width         = 0;
        uint32_t               height        = 0;
    };

    struct TextureCacheEntry
    {
        std::shared_ptr<TextureData> data;
    };

    /// @brief Global atomic slot allocator for the bindless texture array.
    ///
    /// Incremented once per @c VulkanTexture::loadTexture() call. Thread-safe
    /// via @c std::atomic fetch-add. The value is used as the @c dstArrayElement
    /// when writing into the bindless descriptor set (set 1, binding 0).
    static std::atomic<uint32_t> m_nextTextureSlot = 1; // slot 0 reserved as "no texture"

    /// @brief Manages the lifetime and GPU resources of a 2D Vulkan texture.
    ///
    /// Handles the full pipeline from loading an image file off disk to registering
    /// it in the global bindless descriptor array: staging upload, optional mip-chain
    /// generation, image view creation, sampler configuration, and bindless slot
    /// registration.
    ///
    /// Typical usage:
    /// @code
    /// VulkanTexture tex;
    /// tex.loadTexture(rhi, "assets/albedo.png", true);
    /// uint32_t slot = tex.getBindlessIndex(); // pass to GPUMeshData::textureIndex
    /// @endcode
    ///
    /// @note The texture holds a non-owning pointer to the @c VulkanRHI instance that
    ///       created it; the RHI must therefore outlive any @c VulkanTexture it owns.
    class VulkanTexture
    {
    public:
        /// @brief Loads an image from disk and registers it in the global bindless array.
        ///
        /// Internally allocates a host-visible staging buffer, copies pixel data into
        /// it, transitions the image layout to @c eTransferDstOptimal, submits a
        /// buffer-to-image copy, and — when @p mipmaps is @c true — generates a full
        /// mip chain via @c generateMipmaps(). The final layout is
        /// @c eShaderReadOnlyOptimal. Finally calls @c registerBindless() to write
        /// the image view + sampler into the bindless descriptor set at an atomically
        /// allocated slot.
        ///
        /// @param instance   Non-owning pointer to the owning @c VulkanRHI context.
        /// @param filepath   Path to the source image (any format supported by stb_image).
        /// @param mipmaps    When @c true, generates a full mip chain down to 1×1.
        ///                   When @c false, the texture has a single mip level.
        /// @throws std::runtime_error if the file cannot be opened or decoded.
        void loadTexture(VulkanRHI* instance, const std::filesystem::path& filepath, bool mipmaps = true);

        /// @brief Dynamically adjusts the minimum LOD bias of the sampler.
        ///
        /// Waits for the device to be idle, recreates the @c VkSampler with @p lodLevel
        /// as the @c minLod clamp, and re-registers the texture in the bindless set at
        /// its existing slot. Useful for visualising individual mip levels or implementing
        /// LOD-fade effects at runtime.
        ///
        /// Only has an effect if the texture was loaded with @c mipmaps = @c true and
        /// @c loadTexture() has completed successfully.
        ///
        /// @param lodLevel  Desired minimum LOD value in [0, @c getMipLevels()).
        ///                  Clamped to the valid range internally.
        void setLodLevel(float lodLevel);

        bool hasTexture() const noexcept { return m_hasTexture; }
        bool hasMipmaps() const noexcept { return m_hasMipmaps; }

    public:
        // Accessors

        /// @brief Returns the underlying @c VkImage handle.
        [[nodiscard]] vk::Image         getImage()         noexcept { return m_data->image; }

        /// @brief Returns the @c VkImageView covering all mip levels and the single array layer.
        [[nodiscard]] vk::ImageView     getImageView()     noexcept { return m_data->imageView;     }

        /// @brief Returns the @c VkSampler configured for this texture.
        [[nodiscard]] vk::Sampler       getSampler()       noexcept { return m_data->sampler;       }

        /// @brief Returns the number of mip levels present in the image.
        [[nodiscard]] uint32_t  getMipLevels() noexcept { return m_data->mipLevels; }

        /// @brief Returns the width of mip level 0 in texels.
        [[nodiscard]] uint32_t getWidth()     noexcept { return m_data->width;     }

        /// @brief Returns the height of mip level 0 in texels.
        [[nodiscard]] uint32_t getHeight()    noexcept { return m_data->height;    }

        /// @brief Returns the slot index of this texture in the global bindless array.
        ///
        /// This value should be stored in @c GPUMeshData::textureIndex and uploaded
        /// to the GPU so the vertex and fragment shaders can index into the bindless
        /// array with @c textures[bindlessIndex].
        ///
        /// @returns The assigned slot, or 0 if the texture has not been loaded.
        [[nodiscard]] uint32_t getBindlessIndex() const noexcept { return m_hasTexture ? m_bindlessIndex : 0; }

    private:
        /// @brief Loads pixel data from @p filepath into a device-local @c VkImage.
        ///
        /// Allocates the image via @c createImage(), uploads through a staging buffer
        /// with @c copyBufferToImage(), and either calls @c generateMipmaps() or leaves
        /// the image in @c eTransferDstOptimal (which @c generateMipmaps will transition
        /// away from) depending on @c m_hasMipmaps. When mipmaps are disabled the image
        /// stays in @c eTransferDstOptimal until an explicit transition is required.
        ///
        /// @param filepath  Path to the source image file.
        void createTexture(const std::filesystem::path& filepath);

        /// @brief Creates a 2D color @c VkImageView for @c m_image covering all mip levels.
        ///
        /// Stores the result in @c m_imageView.
        void createTextureView();

        /// @brief Creates and stores a @c VkSampler in @c m_sampler.
        ///
        /// Configures linear mag/min filtering, linear mipmap mode, @c eRepeat address
        /// mode on all axes, anisotropic filtering up to @c maxSamplerAnisotropy, no
        /// compare op, and LOD range [@p minLod, @c vk::LodClampNone].
        ///
        /// @param minLod  Minimum LOD level accessible to the sampler (default 0).
        ///                Clamped to [0, @c m_mipLevels] internally.
        void createTextureSampler(float minLod = 0.0f);

        /// @brief Creates a @c VkImage and binds device memory satisfying @p properties.
        ///
        /// Uses the dimensions and mip level count already stored in @c m_width,
        /// @c m_height, and @c m_mipLevels.
        ///
        /// @param tiling      @c eOptimal for device-local images.
        /// @param usage       Usage flags (typically @c eSampled | @c eTransferSrc | @c eTransferDst).
        /// @param properties  Required memory property flags (e.g. @c eDeviceLocal).
        void createImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

        /// @brief Creates a 2D @c VkImageView for @c m_image.
        ///
        /// @param aspectFlags  e.g. @c eColor.
        /// @returns A new @c vk::raii::ImageView covering all @c m_mipLevels.
        [[nodiscard]] vk::raii::ImageView createImageView(vk::ImageAspectFlags aspectFlags);

        /// @brief Creates a @c VkBuffer and binds device memory satisfying @p properties.
        ///
        /// @param size          Buffer size in bytes.
        /// @param usage         Usage flags.
        /// @param properties    Required memory property flags.
        /// @param buffer        [out] Receives the created @c vk::raii::Buffer.
        /// @param bufferMemory  [out] Receives the bound @c vk::raii::DeviceMemory.
        void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                          vk::MemoryPropertyFlags properties,
                          vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);

        /// @brief Copies pixel data from a staging buffer into @c m_image.
        ///
        /// Assumes the image is already in @c eTransferDstOptimal layout. Records and
        /// immediately submits a single-time command buffer via
        /// @c VulkanRHI::beginSingleTimeCommands() / @c endSingleTimeCommands().
        ///
        /// @param buffer  Staging buffer containing packed row-major RGBA pixel data.
        void copyBufferToImage(const vk::raii::Buffer& buffer);

        /// @brief Generates a full mip chain using repeated @c blitImage calls.
        ///
        /// For each mip level @c i from 1 to @c m_mipLevels - 1:
        ///   - Transitions level @c i-1 from @c eTransferDstOptimal to @c eTransferSrcOptimal.
        ///   - Blits level @c i-1 into level @c i with linear filtering.
        ///   - Transitions level @c i-1 to @c eShaderReadOnlyOptimal.
        /// The final level is transitioned to @c eShaderReadOnlyOptimal at the end.
        /// All barriers and blits are recorded into a single single-time command buffer.
        ///
        /// @throws std::runtime_error if @c m_format does not support
        ///         @c eSampledImageFilterLinear in optimal tiling.
        void generateMipmaps(bool isKtx = false, ktxTexture* texture = nullptr);

        /// @brief Transitions @c m_image layout using a single-time command buffer.
        ///
        /// Supports two transitions:
        ///   - @c eUndefined → @c eTransferDstOptimal
        ///   - @c eTransferDstOptimal → @c eShaderReadOnlyOptimal
        ///
        /// @param oldLayout  Current layout.
        /// @param newLayout  Target layout.
        /// @throws std::invalid_argument for unsupported layout pairs.
        void transitionImageLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

        /// @brief Writes this texture's image view and sampler into the global bindless
        ///        descriptor set at @p slot.
        ///
        /// Updates @c m_bindlessIndex, constructs a @c VkDescriptorImageInfo with
        /// @c eShaderReadOnlyOptimal layout, and calls @c vkUpdateDescriptorSets on
        /// @c VulkanRHI::m_bindlessSet. Safe to call multiple times on the same slot
        /// (e.g. when the sampler is recreated by @c setLodLevel()).
        ///
        /// @param slot  Target element index in the bindless array.
        /// @returns @p slot (for convenience).
        uint32_t registerBindless(uint32_t slot);

        /// @brief Atomically increments and returns the next free slot in the bindless array.
        ///
        /// @par Thread safety
        ///   Thread-safe via @c std::atomic::fetch_add. Each call consumes one slot
        ///   permanently; slots are never freed.
        ///
        /// @returns The allocated slot index.
        [[nodiscard]] static uint32_t allocateTextureSlot() { return m_nextTextureSlot.fetch_add(1); }

    private:
        VulkanRHI* pRhi; ///< Non-owning pointer to the parent RHI context.

        std::shared_ptr<TextureData> m_data;
        vk::Format                   m_format = vk::Format::eR8G8B8A8Srgb; ///< Texel format (fixed at RGBA8 sRGB).
        uint32_t                     m_bindlessIndex = 0;

        bool m_hasMipmaps = true;  ///< Whether a full mip chain was requested on load.
        bool m_hasTexture = false; ///< Set to @c true after @c loadTexture() completes.

        VulkanTexture* m_sharedOwner = nullptr;
    };
} // namespace gcep::rhi::vulkan
