#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

class MemoryManager;

/**
 * @struct TextureSamplerView
 * @brief Bundles a Vulkan sampler and image view for texture binding.
 */
struct TextureSamplerView {
    VkSampler textureSampler;
    VkImageView textureImageView;
};

/**
 * @class Sampler
 * @brief Manages Vulkan sampler objects used for texture filtering and addressing.
 */
class Sampler{
public:
    /**
     * @brief Constructs a sampler with the given addressing mode and physical device properties.
     * @param device Logical Vulkan device.
     * @param prop Physical device properties used to determine sampler capabilities.
     * @param addressMode Addressing mode for out-of-bounds texture coordinates (default: REPEAT).
     */
    Sampler(const VkDevice device, const VkPhysicalDeviceProperties& prop, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
    ~Sampler();

    VkSampler getSampler();

protected:
    VkSampler textureSampler;

private:
     // Vulkan handles
    VkDevice deviceHandle;

    /**
     * @brief Creates the Vulkan sampler with the specified addressing mode.
     * @param prop Physical device properties for capability checking.
     * @param addressMode Addressing mode for texture wrapping.
     */
    void createTextureSampler(const VkPhysicalDeviceProperties& prop, VkSamplerAddressMode addressMode);
};

/**
 * @class Texture
 * @brief Manages a single Vulkan texture image loaded from disk.
 */
class Texture {
public:
    VkImageView textureImageView;
    
    /**
     * @brief Loads a texture image from file and creates the Vulkan image resources.
     * @param textureFile Path to the image file to load.
     * @param device Logical Vulkan device.
     * @param memManager Reference to the MemoryManager for GPU allocation.
     */
    Texture(const std::string& textureFile, const VkDevice device, MemoryManager& memManager);
    ~Texture();

    bool isTransparent() {
        return channelCount == 4;       // expected RGBA, else no alpha channel
    }

private:
    VkImage textureImage;
    VmaAllocation textureImageMemory;

    int channelCount;
    VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;  // Fixed format for all textures (SRGB color space).

    // Vulkan handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    /**
     * @brief Loads image data from disk and creates the Vulkan image on the GPU.
     * @param texturePath Path to the image file.
     */
    void createTextureImage(const std::string& texturePath);
    void createTextureImageView();
};

/**
 * @class TexturesManager
 * @brief Aggregates texture and sampler resources for the application.
 *
 * Provides a centralized manager for cube map textures and their associated samplers.
 * Currently utilized only for a single cube texture; can be extended for additional textures.
 */
class TexturesManager{
public:
    Texture cubeTexture;
    Sampler cubeSampler;

    /**
     * @brief Initializes the texture manager with a cube map texture.
     * @param cubeTextureFile Path to the cube texture image file.
     * @param device Logical Vulkan device.
     * @param memManager Reference to the MemoryManager for GPU allocation.
     * @param prop Physical device properties for sampler capability checking.
     */
    TexturesManager(const std::string& cubeTextureFile, const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop);
    ~TexturesManager();

    TextureSamplerView getSamplerView();
private:
    const VkPhysicalDeviceProperties& properties;           // unused
};