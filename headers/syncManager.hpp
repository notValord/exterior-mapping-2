#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>

extern const int MAX_FRAMES_IN_FLIGHT;

class SyncManager{
public:
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> imageFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    SyncManager(const VkDevice& device);
    ~SyncManager();
private:
    // vulkan handles
    VkDevice deviceHandle;

    void createSemaphore(VkSemaphore& semaphore);
    void createFence(VkFence& fence, VkFenceCreateFlagBits signaled);
};