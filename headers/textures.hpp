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

class Texture {
public:
    VkImageView textureImageView;
    
    Texture(const std::string& textureFile, const VkDevice device, MemoryManager& memManager);
    ~Texture();

private:
    VkImage textureImage;
    VmaAllocation textureImageMemory;

    VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;

    // Vulakn handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void createTextureImage(const std::string& texturePath);
    void createTextureImageView();
};

class TexturesManager{
public:
    Texture cubeTexture;
    Sampler cubeSampler;

    TexturesManager(const std::string& cubeTextureFile, const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop);
    ~TexturesManager();

    TextureSamplerView getSamplerView();
private:
    const VkPhysicalDeviceProperties& properties;
};