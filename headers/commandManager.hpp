#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// system includes
#include <vector>
#include <iostream>
#include <array>

extern const size_t MAX_FRAMES_IN_FLIGHT;
struct QueueFamilyIndices;

class CommandManager{
public:
    // maybe have specilized commandpool for different queues
    std::vector<VkCommandBuffer> commandBuffers;        // Freed with commandPool
    std::vector<VkCommandBuffer> offlineCommandBuffers;

    CommandManager(const QueueFamilyIndices& familyIndices, const VkDevice device);
    ~CommandManager();

    VkCommandPool getTransferCommandPool();

private:
    VkCommandPool graphicsCommandPool;

    // Vulkan handles
    VkDevice deviceHandle;

    void createCommandPool(const QueueFamilyIndices& familyIndices);
    void createCommandBuffers();
};