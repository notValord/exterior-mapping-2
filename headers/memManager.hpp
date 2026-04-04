#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

#include <vk_mem_alloc.h>

struct PhysicalDeviceInstance;

/**
 * @enum SaveImageFormat
 * @brief Supported output formats for saving image data to disk.
 */
enum class SaveImageFormat {
    PNG,
    HDR,
    EXR
};

/**
 * @class MemoryManager
 * @brief Wraps Vulkan memory allocation and transfer utilities with VMA.
 *
 * Provides helper functions for creating and destroying buffers and images,
 * copying between resources, and saving image contents to disk.
 */
class MemoryManager{
public:
    /**
     * @brief Constructs the memory manager with the physical device and transfer queue.
     * @param phyDevInst Physical device instance containing device and instance handles.
     * @param transferPool Command pool used for copy and transition commands.
     * @param transferQueue Queue used to submit transfer commands.
     */
    MemoryManager(const PhysicalDeviceInstance& phyDevInst, const VkCommandPool transferPool, const VkQueue transferQueue);
    ~MemoryManager();

    void createImage(int width, int height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImage& image,
             VmaMemoryUsage vmaUsage, VmaAllocation& allocation, uint32_t layers = 1);
    void destroyImage(VkImage& image, VmaAllocation& allocation);

    /**
     * @brief Creates a Vulkan buffer with optional initial data upload.
     * @return Allocation info for the created buffer.
     */
    VmaAllocationInfo createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkBuffer& buffer,
            VmaMemoryUsage vmaUsage, VmaAllocation& allocation, void* data = nullptr, bool mapped = false);

    /**
     * @brief Copies host data into a mapped buffer allocation.
     */
    void populateBuffer(VmaAllocation& allocation, void* data, VkDeviceSize bufferSize, bool flush);
    void destroyBuffer(VkBuffer& buffer, VmaAllocation& allocation);

    /**
     * @brief Transitions an image between layouts, optionally using a provided command buffer.
     */
    void transitionImageLayout(VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCnt = 1, 
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE);
    
    void copyBufferToImage(VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height);
    void copyImageToBuffer(VkImage& image, VkFormat imageFormat, VkBuffer& buffer, uint32_t width, uint32_t height, uint32_t layerCount = 1);
    void copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size);
    void copyImage(VkImage& srcImage, VkFormat srcImageFormat, VkImage& dstImage, VkFormat dstImageFormat, VkExtent3D extent, uint32_t layerCount = 1);
    void copyLayeredImage(VkCommandBuffer& commandBuffer, VkImage& srcImage, VkFormat srcImageFormat, VkImage& dstImage, VkFormat dstImageFormat, VkExtent3D extent, uint32_t layerId);

    /**
     * @brief Blits/resamples an image between two image resources.
     */
    void resampleImage(VkCommandBuffer& commandBuffer, VkImage& srcImage, VkImageAspectFlags srcImageAspect, VkImage& dstImage, VkImageAspectFlags dstImageAspect, VkOffset3D scrOffset, VkOffset3D dstOffset, uint32_t layerCount = 1);

    /**
     * @brief Saves the contents of a GPU image allocation to disk.
     */
    void saveImage(VmaAllocation& allocation, VkFormat imageFormat, SaveImageFormat saveFormat, std::string filename, uint32_t width, uint32_t height, uint32_t layerCount, float near = 0.0f, float far = 0.0f);
private:
    VmaAllocator allocator;

    // Vulkan handles
    VkDevice deviceHandle;
    VkCommandPool transferPoolHandle;
    VkQueue transferQueueHandle;
};