#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <optional>
#include <set>
#include <map>
#include <bitset>

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

struct DeviceSurface {
    const VkPhysicalDevice physicalDevice;
    const VkSurfaceKHR surface;
};

struct PhysicalDeviceInstance {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;
    std::optional<uint32_t> computeFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
    }
};

class VulkanContext {
public:
    VkDevice device;
    VkQueue graphicsQueue;              // queues are cleaned with the device
    VkQueue presentQueue;
    VkQueue computeQueue;       // cannot be used right not

    QueueFamilyIndices familyIndices;

    VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    DeviceSurface getDeviceSurfaceHandle();
    PhysicalDeviceInstance getPhysicalDeviceInstance();
    VkPhysicalDeviceMemoryProperties getMemoryProperties();
    VkPhysicalDeviceProperties getDeviceProperties();
private:
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;    // is implicitly destroyed with instance
    VkSurfaceKHR surface;

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                                                         const VkDebugUtilsMessengerCallbackDataEXT*, void*);
    static VkResult createDebugUtilsMessengerEXT(const VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks* , VkDebugUtilsMessengerEXT*);
    static void destroyDebugUtilsMessengerEXT(const VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);
    static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT&);
    void setupDebugMessenger();

    bool checkValidationSupport();
    static void checkExtensionSupport(const std::vector<const char*>&);
    std::vector<const char*> getRequiredExtensions();
    void createInstance();

    void createSurface(GLFWwindow* window);

    bool isDeviceSuitable(const VkPhysicalDevice, const QueueFamilyIndices&);
    static unsigned int rateDeviceSuitability(const VkPhysicalDevice, const QueueFamilyIndices&);
    bool checkDeviceExtensionSupport(const VkPhysicalDevice);
    void pickPhysicalDevice();

    void createLogicalDevice();
};