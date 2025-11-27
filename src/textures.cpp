#include <textures.hpp>
#include <util.hpp>
#include <memManager.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

TexturesManager::TexturesManager(const std::string& textureFile, const std::string& cubeTextureFile, const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop)
    : properties(prop), modelTexture(textureFile, device, memManager, prop), cubeTexture(cubeTextureFile, device, memManager, prop, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE),
    offlineSampler(device, prop) {
    ;
}

TexturesManager::~TexturesManager() {
    ;
}

Texture::Texture(const std::string& textureFile, const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop, VkSamplerAddressMode addressMode)
    : Sampler(device, prop, addressMode),  deviceHandle(device), memManager(memManager) {
    createTextureImage(textureFile);
    createTextureImageView();
}

Texture::~Texture() {
    vkDestroyImageView(deviceHandle, textureImageView, nullptr);
    memManager.destroyImage(textureImage, textureImageMemory);
}

TextureSamplerView Texture::getSamplerView() {
    return TextureSamplerView{textureSampler, textureImageView};
}

void Texture::createTextureImage(const std::string& texturePath) {
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    // using STBI_rgb_aplha forces the image to be loaded rgba for the consistency even though the format does not have to have one
    VkDeviceSize imageSize = texWidth * texHeight * 4;      // the texChannels contains the number of channels of the original format, doesn't have to be 4

    if (!pixels) {
        throw std::runtime_error("Failed to sload texture image!");
    }

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;

    memManager.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBufferMemory, pixels);

    stbi_image_free(pixels);

    memManager.createImage(texWidth, texHeight, textureFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, textureImage, VMA_MEMORY_USAGE_GPU_ONLY, textureImageMemory);
    
    memManager.transitionImageLayout(textureImage, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    memManager.copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    memManager.transitionImageLayout(textureImage, textureFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}

void Texture::createTextureImageView() {
    textureImageView = createImageView(textureImage, textureFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);
}

Sampler::Sampler(const VkDevice device, const VkPhysicalDeviceProperties& prop, VkSamplerAddressMode addressMode)
    : deviceHandle(device) {
    createTextureSampler(prop, addressMode);
}

Sampler::~Sampler() {
    vkDestroySampler(deviceHandle, textureSampler, nullptr);
}

void Sampler::createTextureSampler(const VkPhysicalDeviceProperties& properties, VkSamplerAddressMode addressMode) {
    VkSamplerCreateInfo samplerCI{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = addressMode,
        .addressModeV = addressMode,
        .addressModeW = addressMode,
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

VkSampler Sampler::getSampler() {
    return textureSampler;
}