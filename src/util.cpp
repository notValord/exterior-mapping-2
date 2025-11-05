#include "util.hpp"

VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, const VkDevice& deviceHandle) {
    VkImageViewCreateInfo imageViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format
    };
    imageViewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCI.subresourceRange.aspectMask = aspectFlags;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(deviceHandle, &imageViewCI, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image views!");
    }

    return imageView;
}

VkCommandBuffer beginSingleTimeCommands(const VkDevice& deviceHandle, const VkCommandPool& commandPoolHandle) {
    VkCommandBufferAllocateInfo commandBufferAI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPoolHandle,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(deviceHandle, &commandBufferAI, &commandBuffer);

    VkCommandBufferBeginInfo commandBufferBI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBI) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin a command buffer!");
    }

    return commandBuffer;
}

void endSingleTimeCommands(VkCommandBuffer commandBuffer, const VkQueue queueHandle, const VkDevice& deviceHandle, const VkCommandPool& commandPoolHandle) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end recording a command buffer!");
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    vkQueueSubmit(queueHandle, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queueHandle);

    vkFreeCommandBuffers(deviceHandle, commandPoolHandle, 1, &commandBuffer);
}