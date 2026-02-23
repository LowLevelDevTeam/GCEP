#pragma once

// Externals
#include <vulkan/vulkan_raii.hpp>

// STL
#include <cstdint>
#include <filesystem>

namespace gcep::rhi::vulkan {

class VulkanRHI;

/// @brief Manages the lifetime and GPU resources of a 2D Vulkan texture.
///
/// Handles the full pipeline from loading an image file off disk to making it
/// available for sampling in a shader: staging upload, optional mip-chain
/// generation, image view creation, and sampler configuration.
///
/// Typical usage:
/// @code
/// VulkanTexture tex;
/// tex.loadTexture(this, "assets/albedo.png", true);
/// // tex.getSampler() / tex.getImageView() are now ready to be written into a descriptor set
/// @endcode
///
/// @note The texture holds a non-owning pointer to the @c VulkanRHI instance that
///       created it; the RHI must therefore outlive any @c VulkanTexture it owns.
class VulkanTexture
{
public:
    // Public functions

    /// @brief Loads an image from disk and uploads it to a device-local @c VkImage.
    ///
    /// Internally allocates a host-visible staging buffer, copies pixel data into
    /// it, transitions the image layout to @c eTransferDstOptimal, submits a
    /// buffer-to-image copy, and — when @p mipmaps is @c true — generates a full
    /// mip chain via @c generateMipmaps(). The final layout is
    /// @c eShaderReadOnlyOptimal.
    ///
    /// @param instance   Non-owning pointer to the owning @c VulkanRHI context.
    /// @param filepath   Path to the source image (any format supported by stb_image).
    /// @param mipmaps    When @c true, generates a full mip chain down to 1×1.
    ///                   When @c false, the texture has a single mip level.
    /// @throws std::runtime_error if the file cannot be opened or the image
    ///         format is unsupported.
    void loadTexture(VulkanRHI* instance, const std::filesystem::path& filepath, bool mipmaps = true);

    /// @brief Dynamically adjusts the minimum LOD bias of the sampler.
    ///
    /// Recreates the @c VkSampler with @p lodLevel as the @c minLod clamp, which
    /// forces the hardware to sample from mip levels >= @p lodLevel. Useful for
    /// visualising individual mip levels or implementing LOD-fade effects at
    /// runtime.
    ///
    /// @param lodLevel  Desired minimum LOD value in the range [0, @c getMipLevels()).
    ///                  Values outside this range are clamped by the sampler.
    void setLodLevel(float lodLevel);

public:
    // Accessors

    /// @brief Returns the underlying @c VkImage handle.
    [[nodiscard]] vk::Image         getImage()         noexcept { return m_image;         }

    /// @brief Returns the @c VkImageView covering all mip levels and the single array layer.
    [[nodiscard]] vk::ImageView     getImageView()     noexcept { return m_imageView;     }

    /// @brief Returns the @c VkSampler configured for this texture.
    [[nodiscard]] vk::Sampler       getSampler()       noexcept { return m_sampler;       }

    /// @brief Returns the @c VkSampler configured for this texture.
    [[nodiscard]] vk::DescriptorSet getDescriptorSet() noexcept { return m_descriptorSet; }

    /// @brief Returns the number of mip levels present in the image.
    [[nodiscard]] uint32_t          getMipLevels()     noexcept { return m_mipLevels;        }

    /// @brief Returns the width of mip level 0 in texels.
    [[nodiscard]] uint32_t          getWidth()         noexcept { return m_width;            }

    /// @brief Returns the height of mip level 0 in texels.
    [[nodiscard]] uint32_t          getHeight()        noexcept { return m_height;           }

private:
    // Private functions

    /// @brief Loads pixel data from @p filepath into a device-local @c VkImage.
    ///
    /// Allocates the image via @c createImage(), uploads through a staging buffer
    /// with @c copyBufferToImage(), and either calls @c generateMipmaps() or
    /// performs a direct layout transition to @c eShaderReadOnlyOptimal depending
    /// on @c m_hasMipmaps.
    ///
    /// @param filepath  Path to the source image file.
    void createTexture(const std::filesystem::path& filepath);

    /// @brief Creates a 2D color @c VkImageView for @c m_image covering all mip levels.
    ///
    /// Stores the result in @c m_imageView.
    void createTextureView();

    /// @brief Creates and stores a @c VkSampler in @c m_sampler.
    ///
    /// Configures linear filtering, anisotropic filtering (when supported),
    /// @c eClampToEdge address mode, and the mip LOD range
    /// [@p minLod, @c m_mipLevels].
    ///
    /// @param minLod  Minimum LOD level accessible to the sampler (default 0).
    void createTextureSampler(float minLod = 0.0f);

    /// @brief Creates a @c VkImage and binds device memory satisfying @p properties.
    ///
    /// @param tiling       @c eOptimal for device-local images; @c eLinear for host reads.
    /// @param usage        Usage flags (sampled, color attachment, transfer, etc.).
    /// @param properties   Required memory property flags (e.g. @c eDeviceLocal).
    void createImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

    /// @brief Creates a 2D @c VkImageView for the given image.
    ///
    /// @param aspectFlags  e.g. @c eColor or @c eDepth.
    /// @returns A new @c vk::raii::ImageView.
    [[nodiscard]] vk::raii::ImageView createImageView(vk::ImageAspectFlags aspectFlags);

    /// @brief Creates a @c VkBuffer and binds device memory satisfying @p properties.
    ///
    /// @param size          Buffer size in bytes.
    /// @param usage         Usage flags (vertex, index, uniform, transfer, etc.).
    /// @param properties    Required memory property flags.
    /// @param buffer        [out] Receives the created @c vk::raii::Buffer.
    /// @param bufferMemory  [out] Receives the bound @c vk::raii::DeviceMemory.
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties,
                      vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);

    /// @brief Copies pixel data from a staging buffer into a @c VkImage.
    ///
    /// Assumes the image is already in @c eTransferDstOptimal layout. Records and
    /// immediately submits a single-time command buffer.
    ///
    /// @param buffer  Staging buffer containing packed row-major pixel data.
    void copyBufferToImage(const vk::raii::Buffer& buffer);

    /// @brief Generates a full mip chain for @p image using repeated @c blitImage calls.
    ///
    /// Each mip level is transitioned @c eTransferDstOptimal -> @c eTransferSrcOptimal
    /// before being used as the blit source, then transitioned to
    /// @c eShaderReadOnlyOptimal. The final level is transitioned at the end.
    ///
    /// @param format     Must support @c eSampledImageFilterLinear in optimal tiling.
    /// @throws std::runtime_error if @p format does not support linear blitting.
    void generateMipmaps();

    /// @brief Transitions a @c VkImage layout using a single-time command buffer.
    ///
    /// Supports two transitions only:
    ///   - @c eUndefined -> @c eTransferDstOptimal
    ///   - @c eTransferDstOptimal -> @c eShaderReadOnlyOptimal
    ///
    /// @param oldLayout  Current layout.
    /// @param newLayout  Target layout.
    /// @throws std::invalid_argument for unsupported layout pairs.
    void transitionImageLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

    void createDescriptorSet();

private:
    VulkanRHI* pRhi; ///< Non-owning pointer to the parent RHI context.

    // Members
    vk::raii::Image         m_image         = nullptr; ///< Device-local texture image.
    vk::raii::DeviceMemory  m_imageMemory   = nullptr; ///< Memory backing @c m_image.
    vk::raii::ImageView     m_imageView     = nullptr; ///< View covering all mip levels.
    vk::raii::Sampler       m_sampler       = nullptr; ///< Sampler for shader access.
    vk::raii::DescriptorSet m_descriptorSet = nullptr; ///<
    vk::Format              m_format        = vk::Format::eR8G8B8A8Srgb; ///< Texel format.

    uint32_t m_width;               ///< Texel width of mip level 0.
    uint32_t m_height;              ///< Texel height of mip level 0.
    uint32_t m_mipLevels = 1;       ///< Total number of mip levels.

    bool m_first = true;

    bool m_hasMipmaps = true;  ///< Whether a full mip chain was requested on load.
    bool m_hasTexture = false; ///< Whether @c loadTexture() has completed successfully.
};

} // Namespace gcep::rhi::vulkan