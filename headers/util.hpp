#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, const VkDevice& deviceHandle);

VkCommandBuffer beginSingleTimeCommands(const VkDevice& deviceHandle, const VkCommandPool& commandPoolHandle);
void endSingleTimeCommands(VkCommandBuffer commandBuffer, const VkQueue queueHandle, const VkDevice& deviceHandle, const VkCommandPool& commandPoolHandle);