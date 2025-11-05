#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <array>

struct DeviceSurface;
struct QueueFamilyIndices;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

class MemoryManager;

struct AttachementsFormats {
    VkFormat colorImageFormat;
    VkFormat depthFormat;
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};
SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& device, const VkSurfaceKHR& surface);

class SwapChain{
public:
    VkSwapchainKHR swapChain;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    SwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices, const VkDevice& device, GLFWwindow* window, MemoryManager& manager);
    ~SwapChain();

    void createFramebuffers(const VkRenderPass& renderPass);
    void recreateSwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices, const VkRenderPass& renderPass);
    float getExtentRatio();
    AttachementsFormats getAttachementsFormats();

    void transitionToFinalLayout(MemoryManager& memManager, uint32_t imageindex);

private:
    std::vector<VkImage> swapChainImages;   // automatically cleaned up with swapChain

    VkImage depthImage;
    VmaAllocation depthImageMemory;
    VkImageView depthImageView;

    VkFormat swapChainImageFormat;
    VkFormat depthFormat;

    // required handles
    VkDevice deviceHandle = VK_NULL_HANDLE;
    GLFWwindow* windowHandle = nullptr;

    MemoryManager& memManager;

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void createSwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices);
    void createImageViews();

    VkFormat findSupportedFormat(const VkPhysicalDevice& physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat(const VkPhysicalDevice& physicalDevice);

    void createDepthResources();

    void cleanupSwapchain();
};