#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, const VkDevice& deviceHandle);

class MemoryManager{
public:
    MemoryManager(const VkDevice& device, const VkCommandPool& tranferPool, const VkQueue& transferQueue, const VkPhysicalDeviceMemoryProperties& properties);
    ~MemoryManager();

    void createImage(int width, int height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImage& image, VkMemoryPropertyFlags properties, VkDeviceMemory &imageMemory);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkBuffer &buffer,
                    VkMemoryPropertyFlags memProperties, VkDeviceMemory &bufferMemory);
private:
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Vulkan handles
    VkDevice deviceHandle;
    VkCommandPool transferPoolHandle;
    VkQueue transferQueueHandle;

    VkPhysicalDeviceMemoryProperties memProperties;
};