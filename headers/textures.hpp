#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

class MemoryManager;

struct TextureSamplerView {
    VkSampler textureSampler;
    VkImageView textureImageView;
};

class Sampler{
public:
    Sampler(const VkDevice device, const VkPhysicalDeviceProperties& prop, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
    ~Sampler();

    VkSampler getSampler();

protected:
    VkSampler textureSampler;

private:
     // Vulakn handles
    VkDevice deviceHandle;

    void createTextureSampler(const VkPhysicalDeviceProperties& prop, VkSamplerAddressMode addressMode);
};

class Texture : protected Sampler{
public:
    Texture(const std::string& textureFile, const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
    ~Texture();

    TextureSamplerView getSamplerView();

private:
    VkImage textureImage;
    VmaAllocation textureImageMemory;
    VkImageView textureImageView;

    VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;

    // Vulakn handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void createTextureImage(const std::string& texturePath);
    void createTextureImageView();
};

class TexturesManager{
public:
    Texture modelTexture;
    Texture cubeTexture;
    Sampler offlineSampler;

    TexturesManager(const std::string& textureFile, const std::string& cubeTextureFile, const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop);
    ~TexturesManager();

private:
    const VkPhysicalDeviceProperties& properties;
};