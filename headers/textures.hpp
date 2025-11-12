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

class Textures{
public:
    Textures(const std::string& textureFile, const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop);
    ~Textures();

    TextureSamplerView getSamplerView();

private:
    VkImage textureImage;
    VmaAllocation textureImageMemory;
    VkImageView textureImageView;

    VkSampler textureSampler;

    VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;

    // Vulakn handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    const VkPhysicalDeviceProperties& properties;

    void createTextureImage(const std::string& texturePath);
    void createTextureImageView();
    void createTextureSampler();
};