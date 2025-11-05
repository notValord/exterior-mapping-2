#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "imgui.h"

#include <iostream>
#include <vector>

extern const int MAX_FRAMES_IN_FLIGHT;

struct AttachementsFormats;
struct PhysicalDeviceInstance;
struct QueueFamilyIndices;

class ImguiProxy {
public:
    ImguiProxy(const AttachementsFormats& imageFormats, const std::vector<VkImageView>& swapChainImageViews,
     const PhysicalDeviceInstance& physicalDeviceInstance, GLFWwindow* window, const VkQueue& graphicsQueue, const QueueFamilyIndices& familyIndices,
     VkExtent2D& swapChainExtent);
    ~ImguiProxy();

    void rebuildUI(float fps, uint32_t camNum);
    void recreateFramebuffers(const std::vector<VkImageView>& swapChainImageViews, const VkExtent2D& swapChainExtent);
    VkCommandBuffer recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex);
private:
    VkDescriptorPool descriptorPool;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // Vulkan handles
    VkDevice deviceHandle;
    VkExtent2D& swapChainExtentHandle;

    void createDescriptorPool();
    void createRenderPass(const AttachementsFormats& imageFormats);
    void createFramebuffers(const std::vector<VkImageView>& swapChainImageViews);
    void createCommandBuffers(const QueueFamilyIndices& familyIndices);

    void drawUI(float fps, uint32_t camNum);
};