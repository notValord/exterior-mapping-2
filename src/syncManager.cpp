#include <syncManager.hpp>

SyncManager::SyncManager(const VkDevice device)
    : deviceHandle(device) {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    imageFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createSemaphore(imageAvailableSemaphores[i]);
        createSemaphore(imageFinishedSemaphores[i]);

        createFence(inFlightFences[i], VK_FENCE_CREATE_SIGNALED_BIT);
    }
}

SyncManager::~SyncManager() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(deviceHandle, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(deviceHandle, imageFinishedSemaphores[i], nullptr);
        vkDestroyFence(deviceHandle, inFlightFences[i], nullptr);
    }
}

void SyncManager::createSemaphore(VkSemaphore& semaphore) {
    VkSemaphoreCreateInfo semaphoreCI{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    if (vkCreateSemaphore(deviceHandle, &semaphoreCI, nullptr, &semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphores!");
    }
}

void SyncManager::createFence(VkFence& fence, VkFenceCreateFlagBits signaled) {
    VkFenceCreateInfo fenceCI{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled
    };

    if (vkCreateFence(deviceHandle, &fenceCI, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a fence!");
    }
}