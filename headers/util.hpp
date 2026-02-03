#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

struct SwapChainSupportDetails;

VkImageView createImageView(VkImage& image, VkFormat format, VkImageAspectFlags aspectFlags, const VkDevice deviceHandle, uint32_t layerCnt = 1);

VkCommandBuffer beginSingleTimeCommands(const VkDevice deviceHandle, const VkCommandPool commandPoolHandle);
void endSingleTimeCommands(VkCommandBuffer commandBuffer, const VkQueue queueHandle, const VkDevice deviceHandle, const VkCommandPool commandPoolHandle);

void beginCommandBuffer(VkCommandBuffer commandBuffer);
void submitCommandBuffer(VkCommandBuffer commandBuffer);

SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice device, const VkSurfaceKHR surface);