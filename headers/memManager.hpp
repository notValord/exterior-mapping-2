#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

#include "vk_mem_alloc.h"

struct PhysicalDeviceInstance;

class MemoryManager{
public:
    MemoryManager(const PhysicalDeviceInstance& phyDevInst, const VkCommandPool& transferPool, const VkQueue& transferQueue);
    ~MemoryManager();

    void createImage(int width, int height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImage& image,
             VmaMemoryUsage vmaUsage, VmaAllocation& allocation);
    void destroyImage(VkImage& image, VmaAllocation& allocation);

    VmaAllocationInfo createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkBuffer &buffer,
            VmaMemoryUsage vmaUsage, VmaAllocation& allocation, void* data = nullptr);
    void populateBuffer(VmaAllocation& allocation, void* data, VkDeviceSize bufferSize, bool flush);
    void destroyBuffer(VkBuffer& buffer, VmaAllocation& allocation);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
private:
    VmaAllocator allocator;

    // Vulkan handles
    VkDevice deviceHandle;
    VkCommandPool transferPoolHandle;
    VkQueue transferQueueHandle;
};