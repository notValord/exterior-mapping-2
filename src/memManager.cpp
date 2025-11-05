#define VMA_IMPLEMENTATION
#include "memManager.hpp"
#include "vulkanContext.hpp"
#include "util.hpp"

MemoryManager::MemoryManager(const PhysicalDeviceInstance& phyDevInst, const VkCommandPool& transferPool, const VkQueue& transferQueue)
    : deviceHandle(phyDevInst.device), transferPoolHandle(transferPool), transferQueueHandle(transferQueue) {
    VmaAllocatorCreateInfo allocatorInfo = {
        .physicalDevice = phyDevInst.physicalDevice,
        .device = phyDevInst.device,
        .instance = phyDevInst.instance,
        .vulkanApiVersion = VK_API_VERSION_1_0
    };

    vmaCreateAllocator(&allocatorInfo, &allocator);
}

MemoryManager::~MemoryManager(){
    vmaDestroyAllocator(allocator);
}

void MemoryManager::createImage(int width, int height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImage& image,
    VmaMemoryUsage vmaUsage, VmaAllocation& allocation) {
    VkImageCreateInfo imageCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},       // width, height, depth
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocationCI{
        .usage = vmaUsage,
    };

    vmaCreateImage(allocator, &imageCI, &allocationCI, &image, &allocation, nullptr);
}

void MemoryManager::destroyImage(VkImage& image, VmaAllocation& allocation) {
    vmaDestroyImage(allocator, image, allocation);
}

static bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void MemoryManager::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(deviceHandle, transferPoolHandle);

    VkImageSubresourceRange subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        subresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencilComponent(format)){
            subresource.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    // used for transition image layouts
    VkImageMemoryBarrier imageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,     // TODO
        .dstAccessMask = 0,     // TODO
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,     // used when transfering ownership between queue families
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresource,
    };

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else {
        throw std::runtime_error("Unsuported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1 , &imageMemoryBarrier);

    endSingleTimeCommands(commandBuffer, transferQueueHandle, deviceHandle, transferPoolHandle);
}

void MemoryManager::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(deviceHandle, transferPoolHandle);

    VkImageSubresourceLayers imageSubresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    VkBufferImageCopy bufferImageCopy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,

        .imageSubresource = imageSubresource,
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };
    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

    endSingleTimeCommands(commandBuffer, transferQueueHandle, deviceHandle, transferPoolHandle);
}

VmaAllocationInfo MemoryManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkBuffer &buffer,
     VmaMemoryUsage vmaUsage, VmaAllocation& allocation, void* data) {
    VkBufferCreateInfo bufferCI{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = sharingMode,
    };

    VmaAllocationCreateInfo allocationCI = {
        .usage = vmaUsage,
    };
    if (usage == VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
        allocationCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                  VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocationInfo allocInfo;
    vmaCreateBuffer(allocator, &bufferCI, &allocationCI, &buffer, &allocation, &allocInfo);

    if (data) {
        populateBuffer(allocation, data, size, !(allocInfo.memoryType & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    }

    return allocInfo;
}

void MemoryManager::populateBuffer(VmaAllocation& allocation, void* data, VkDeviceSize bufferSize, bool flush) {
    void* mappedPtr;
    vmaMapMemory(allocator, allocation, &mappedPtr);
    if (mappedPtr == nullptr) {
        throw std::runtime_error("Failed to populate buffer!");
    }
    memcpy(mappedPtr, data, bufferSize);

    // Flush if not coherent
    if (flush) {
        vmaFlushAllocation(allocator, allocation, 0, bufferSize);
    }

    vmaUnmapMemory(allocator, allocation);
}

void MemoryManager::destroyBuffer(VkBuffer& buffer, VmaAllocation& allocation) {
    vmaDestroyBuffer(allocator, buffer, allocation);
}

void MemoryManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(deviceHandle, transferPoolHandle);

    VkBufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer, transferQueueHandle, deviceHandle, transferPoolHandle);
}