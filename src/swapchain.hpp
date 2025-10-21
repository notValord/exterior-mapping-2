#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <array>

// VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);        // TODO

struct DeviceSurface;
struct QueueFamilyIndices;


struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};
SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& device, const VkSurfaceKHR& surface);

class SwapChain{
public:
    SwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices, const VkDevice& device, GLFWwindow* window);
    ~SwapChain();

    // void recreateSwapChain();

private:
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;   // automatically cleaned up with swapChain
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    VkDevice device = VK_NULL_HANDLE;
    GLFWwindow* window = nullptr;

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void createSwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices);
    void createImageViews();
    // void createFramebuffers();

    void cleanupSwapchain();
};