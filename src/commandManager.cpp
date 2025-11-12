#include "commandManager.hpp"
#include "vulkanContext.hpp"

CommandManager::CommandManager(const QueueFamilyIndices& familyIndices, const VkDevice device)
    : deviceHandle(device) {
    createCommandPool(familyIndices);
    createCommandBuffers();
}

CommandManager::~CommandManager() {
    vkDestroyCommandPool(deviceHandle, graphicsCommandPool, nullptr);
}

VkCommandPool CommandManager::getTransferCommandPool() {
    return graphicsCommandPool;
}

void CommandManager::createCommandPool(const QueueFamilyIndices& familyIndices) {

    VkCommandPoolCreateInfo graphicsCommandPoolCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = familyIndices.graphicsFamily.value()
    };

    if (vkCreateCommandPool(deviceHandle, &graphicsCommandPoolCI, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool!");
    }
}

void CommandManager::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    offlineCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo commandBufferAI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = graphicsCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())
    };

    if (vkAllocateCommandBuffers(deviceHandle, &commandBufferAI, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }

    if (vkAllocateCommandBuffers(deviceHandle, &commandBufferAI, offlineCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}