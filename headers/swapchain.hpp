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

/**
 * @struct AttachementsFormats
 * @brief Holds the image formats for color and depth attachments.
 */
struct AttachementsFormats {
    VkFormat colorImageFormat;
    VkFormat depthFormat;
};

/**
 * @struct SwapChainSupportDetails
 * @brief Aggregates surface capabilities and format/present mode options.
 *
 * Used during swap chain creation to query available options on a given surface.
 */
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;          // Surface capabilities (min/max image counts, extents).
    std::vector<VkSurfaceFormatKHR> formats;        // Available surface formats and color spaces.
    std::vector<VkPresentModeKHR> presentModes;     // Available presentation modes (FIFO, MAILBOX, etc.).
};

/**
 * @class SwapChain
 * @brief Manages the Vulkan swap chain and associated framebuffers and depth resources.
 *
 * Handles format selection, extent calculation, and recreation on window resize.
 */
class SwapChain{
public:
    VkSwapchainKHR swapChain;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    /**
     * @brief Constructs the swap chain with the specified configuration.
     * @param deviceSurfaceHandle Physical and logical device plus surface.
     * @param indices Queue family indices for graphics and presentation.
     * @param device Logical Vulkan device.
     * @param window GLFW window handle for size queries.
     * @param manager Reference to the MemoryManager for resource allocation.
     */
    SwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices, const VkDevice device, GLFWwindow* window, MemoryManager& manager);
    ~SwapChain();

    /**
     * @brief Creates framebuffers for all swap chain images with the given render pass.
     * @param renderPass The render pass to use for framebuffer creation.
     */
    void createFramebuffers(const VkRenderPass renderPass);

    /**
     * @brief Recreates the entire swap chain (called on window resize).
     * @param deviceSurfaceHandle Physical and logical device plus surface.
     * @param indices Queue family indices.
     * @param renderPass Render pass for new framebuffers.
     */
    void recreateSwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices, const VkRenderPass renderPass);

    /**
     * @brief Returns the color and depth image formats used by this swap chain.
     * @return AttachementsFormats containing color and depth formats.
     */
    AttachementsFormats getAttachementsFormats();

    /**
     * @brief Transitions a swap chain image to its final layout for presentation.
     * @param memManager Reference to the MemoryManager for layout transition.
     * @param imageindex Index of the swap chain image to transition.
     */
    void transitionToFinalLayout(MemoryManager& memManager, uint32_t imageindex);

private:
    VkImage depthImage;
    VmaAllocation depthImageMemory;
    VkImageView depthImageView;

    std::vector<VkImage> swapChainImages;   // automatically cleaned up with swapChain

    VkFormat swapChainImageFormat;
    VkFormat depthFormat;

    // required Vulkan handles
    VkDevice deviceHandle = VK_NULL_HANDLE;
    GLFWwindow* windowHandle = nullptr;
    MemoryManager& memManager;

    /**
     * @brief Selects the preferred swap surface format.
     * @param availableFormats List of available surface formats.
     * @return Preferred format (BGRA SRGB if available, else first available).
     */
    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    /**
     * @brief Selects the preferred presentation mode.
     * @param availablePresentModes List of available presentation modes.
     * @return Preferred mode (MAILBOX if available, else FIFO).
     */
    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    /**
     * @brief Selects the swap chain extent based on surface capabilities and window size.
     * @param capabilities Surface capabilities returned by the physical device.
     * @return Chosen extent respecting min/max limits.
     */
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    /**
     * @brief Creates the Vulkan swap chain object.
     * @param deviceSurfaceHandle Device surface information.
     * @param indices Queue family indices.
     */
    void createSwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices);
    void createImageViews();
    void createDepthResources();

    void cleanupSwapchain();
};