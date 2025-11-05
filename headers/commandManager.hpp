#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <array>

extern const int MAX_FRAMES_IN_FLIGHT;
struct QueueFamilyIndices;

class CommandManager{
public:
    std::vector<VkCommandBuffer> commandBuffers;        // freed with commandPool
    VkCommandPool graphicsCommandPool;  // needed for mem Manager, change later

    CommandManager(const QueueFamilyIndices& familyIndices, const VkDevice& device);
    ~CommandManager();
private:

    // vulkan handles
    VkDevice deviceHandle;

    void createCommandPool(const QueueFamilyIndices& familyIndices);
    void createCommandBuffers();
};