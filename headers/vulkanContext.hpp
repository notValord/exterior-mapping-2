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

/**
 * @struct DeviceSurface
 * @brief Groups a physical device and surface for Vulkan operations.
 */
struct DeviceSurface {
    const VkPhysicalDevice physicalDevice;
    const VkSurfaceKHR surface;
};

/**
 * @struct PhysicalDeviceInstance
 * @brief Groups Vulkan instance, physical device, and logical device handles.
 */
struct PhysicalDeviceInstance {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
};

/**
 * @struct QueueFamilyIndices
 * @brief Stores indices of supported Vulkan queue families.
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;
    std::optional<uint32_t> computeFamily;

    /**
     * @brief Checks if all required queue families are available.
     * @return true if graphics, present, and compute families are found.
     */
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
    }
};

/**
 * @class VulkanContext
 * @brief Manages Vulkan instance, device selection, and core Vulkan objects.
 * 
 * Handles the complete Vulkan initialization sequence: instance creation, debug messenger setup,
 * surface creation, physical device selection, and logical device creation. Provides access
 * to Vulkan handles and device properties needed by other components.
 */
class VulkanContext {
public:
    VkDevice device;
    VkQueue graphicsQueue;              // Queues are cleaned automatically with the device
    VkQueue presentQueue;
    VkQueue computeQueue;               // Not used at the moment but may be useful for async compute in the future

    QueueFamilyIndices familyIndices;

    /**
     * @brief Initializes the Vulkan context for the given GLFW window.
     * @param window GLFW window to create Vulkan surface for.
     * @throws std::runtime_error if Vulkan initialization fails at any step.
     */
    VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    DeviceSurface getDeviceSurfaceHandle();
    PhysicalDeviceInstance getPhysicalDeviceInstance();
    VkPhysicalDeviceMemoryProperties getMemoryProperties();
    VkPhysicalDeviceProperties getDeviceProperties();

private:
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;    // Implicitly destroyed with instance
    VkSurfaceKHR surface;

    const std::vector<const char*> validationLayers = {  // Required validation layers.
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> deviceExtensions = {  // Required device extensions.
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    /**
     * @brief Debug callback function for Vulkan validation layers.
     * @param messageSeverity Severity level of the message.
     * @param messageType Type of the debug message.
     * @param pCallbackData Callback data containing the message.
     * @param pUserData User data pointer (unused).
     * @return VK_FALSE to indicate the message should not be aborted.
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                                                         const VkDebugUtilsMessengerCallbackDataEXT*, void*);
    
    /**
     * @brief Creates the debug utils messenger extension object.
     * @param instance Vulkan instance.
     * @param pCreateInfo Creation info for the messenger.
     * @param pAllocator Allocation callbacks (unused).
     * @param pDebugMessenger Pointer to store the created messenger.
     * @return VK_SUCCESS on success, or extension error.
     */
    static VkResult createDebugUtilsMessengerEXT(const VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks* , VkDebugUtilsMessengerEXT*);
    static void destroyDebugUtilsMessengerEXT(const VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);
    static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT&);
    
    /**
     * @brief Sets up the debug messenger if validation layers are enabled.
     * @throws std::runtime_error if debug messenger creation fails.
     */
    void setupDebugMessenger();

    /**
     * @brief Checks if requested validation layers are supported.
     * @return true if all validation layers are available.
     */
    bool checkValidationSupport();
    
    /**
     * @brief Checks if required instance extensions are supported.
     * @param requiredExtensions List of extension names to check.
     */
    static void checkExtensionSupport(const std::vector<const char*>&);
    
    /**
     * @brief Gets the list of required Vulkan instance extensions.
     * Includes GLFW-required extensions and debug utils if validation is enabled.
     * @return Vector of extension name strings.
     */
    std::vector<const char*> getRequiredExtensions();
    

    
    /**
     * @brief Creates the Vulkan instance.
     * @throws std::runtime_error if instance creation fails.
     */
    void createInstance();

    /**
     * @brief Creates the window surface for the given GLFW window.
     * @param window GLFW window to create surface for.
     * @throws std::runtime_error if surface creation fails.
     */
    void createSurface(GLFWwindow* window);

    /**
     * @brief Checks if a physical device is suitable for the application.
     * @param physicalDevice Device to check.
     * @param indices Queue family indices for the device.
     * @return true if device meets all requirements.
     */
    bool isDeviceSuitable(const VkPhysicalDevice, const QueueFamilyIndices&);
    
    /**
     * @brief Rates the suitability of a physical device.
     * @param device Device to rate.
     * @param indices Queue family indices for the device.
     * @return Suitability score (higher is better).
     */
    static unsigned int rateDeviceSuitability(const VkPhysicalDevice, const QueueFamilyIndices&);
    
    /**
     * @brief Checks if a device supports all required extensions.
     * @param physicalDevice Device to check.
     * @return true if all required extensions are supported.
     */
    bool checkDeviceExtensionSupport(const VkPhysicalDevice);
    
    /**
     * @brief Selects the most suitable physical device.
     * @throws std::runtime_error if no suitable device is found.
     */
    void pickPhysicalDevice();

    /**
     * @brief Creates the logical Vulkan device and retrieves queue handles.
     * @throws std::runtime_error if device creation fails.
     */
    void createLogicalDevice();
};