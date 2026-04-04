#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>

extern const size_t MAX_FRAMES_IN_FLIGHT;

/**
 * @class SyncManager
 * @brief Manages Vulkan synchronization primitives for frame rendering.
 */
class SyncManager{
public:
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> imageFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    /**
     * @brief Creates synchronization primitives for each frame in flight.
     * @param device The Vulkan logical device.
     * @throws std::runtime_error if semaphore or fence creation fails.
     */
    SyncManager(const VkDevice device);
    ~SyncManager();

private:
    // Vulkan handles
    VkDevice deviceHandle;

    /**
     * @brief Creates a Vulkan semaphore.
     * @param semaphore Reference to the semaphore to create.
     * @throws std::runtime_error if semaphore creation fails.
     */
    void createSemaphore(VkSemaphore& semaphore);
    
    /**
     * @brief Creates a Vulkan fence with specified signaling state.
     * @param fence Reference to the fence to create.
     * @param signaled Whether the fence should be created in signaled state.
     * @throws std::runtime_error if fence creation fails.
     */
    void createFence(VkFence& fence, VkFenceCreateFlagBits signaled);
};