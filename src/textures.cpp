#include "textures.hpp"
#include "util.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Textures::Textures(const std::string& textureFile, const VkDevice& device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop)
    : deviceHandle(device), memManager(memManager), properties(prop) {
    createTextureImage(textureFile);
    createTextureImageView();
    createTextureSampler();
}

Textures::~Textures() {
    vkDestroySampler(deviceHandle, textureSampler, nullptr);
    vkDestroyImageView(deviceHandle, textureImageView, nullptr);
    vkDestroyImage(deviceHandle, textureImage, nullptr);
    vkFreeMemory(deviceHandle, textureImageMemory, nullptr);
}

TextureSamplerView Textures::getSamplerView() {
    return TextureSamplerView{textureSampler, textureImageView};
}

void Textures::createTextureImage(const std::string& texturePath) {
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    // using STBI_rgb_aplha forces the image to be loaded rgba for the consistency even though the format does not have to have one
    VkDeviceSize imageSize = texWidth * texHeight * 4;      // the texChannels contains the number of channels of the original format, doesn't have to be 4

    if (!pixels) {
        throw std::runtime_error("Failed to sload texture image!");
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    memManager.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferMemory);

    void *data;
    vkMapMemory(deviceHandle, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(deviceHandle, stagingBufferMemory);

    stbi_image_free(pixels);

    memManager.createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, textureImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImageMemory);
    
    memManager.transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    memManager.copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    memManager.transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(deviceHandle, stagingBuffer, nullptr);
    vkFreeMemory(deviceHandle, stagingBufferMemory, nullptr);
}

void Textures::createTextureImageView() {
    textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);
}

void Textures::createTextureSampler() {
    VkSamplerCreateInfo samplerCI{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,    // range 0-1
    };

    if (vkCreateSampler(deviceHandle, &samplerCI, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a sampler!");
    }
}