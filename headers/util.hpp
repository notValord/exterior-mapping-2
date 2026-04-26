#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <iostream>

struct SwapChainSupportDetails;

/**
 * @brief Creates a Vulkan image view for an existing image.
 * @param image The Vulkan image to create a view for.
 * @param format The format of the image.
 * @param aspectFlags The aspect flags (color, depth, etc.).
 * @param deviceHandle The logical Vulkan device.
 * @param layerCnt Number of layers (default 1).
 * @param layerID Base layer index (default 0).
 * @return The created VkImageView.
 * @throws std::runtime_error if image view creation fails.
 */
VkImageView createImageView(VkImage& image, VkFormat format, VkImageAspectFlags aspectFlags, const VkDevice deviceHandle, uint32_t layerCnt = 1, uint32_t layerID = 0);

/**
 * @brief Begins recording a single-time command buffer.
 * Should be paired with endSingleTimeCommands().
 * @param deviceHandle The logical Vulkan device.
 * @param commandPoolHandle The command pool to allocate from.
 * @return The allocated command buffer ready for recording.
 * @throws std::runtime_error if command buffer allocation/recording fails.
 */
VkCommandBuffer beginSingleTimeCommands(const VkDevice deviceHandle, const VkCommandPool commandPoolHandle);

/**
 * @brief Ends recording and submits a single-time command buffer.
 * @param commandBuffer The command buffer to end and submit.
 * @param queueHandle The queue to submit to.
 * @param deviceHandle The logical Vulkan device.
 * @param commandPoolHandle The command pool for freeing the buffer.
 * @throws std::runtime_error if command buffer ending fails.
 */
void endSingleTimeCommands(VkCommandBuffer commandBuffer, const VkQueue queueHandle, const VkDevice deviceHandle, const VkCommandPool commandPoolHandle);

/**
 * @brief Begins recording in a provided command buffer.
 * Can provide sequential synchronization of recorded commands in the same buffer.
 * Pair with a submitCommandBuffer() call.
 * @param commandBuffer The command buffer to begin recording into.
 * @throws std::runtime_error if command buffer recording fails.
 */
void beginCommandBuffer(VkCommandBuffer commandBuffer);

/**
 * @brief Ends recording a command buffer (ready for submission).
 * @param commandBuffer The command buffer to end recording.
 * @throws std::runtime_error if ending command buffer recording fails.
 */
void submitCommandBuffer(VkCommandBuffer commandBuffer);

/**
 * @brief Queries swapChain support capabilities for a physical device and surface.
 * @param device The physical Vulkan device.
 * @param surface The surface to query support for.
 * @return SwapChainSupportDetails containing capabilities, formats, and present modes.
 */
SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice deviceHandle, const VkSurfaceKHR surfaceHandle);

float calculateMSE(const uint8_t* gt, const uint8_t* img, uint32_t width, uint32_t height);
float calculateSSIM(const uint8_t* gt, const uint8_t* img, uint32_t width, uint32_t height);