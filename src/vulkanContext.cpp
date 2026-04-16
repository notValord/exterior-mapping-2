#include <vulkanContext.hpp>
#include <swapchain.hpp>
#include <util.hpp>

#include <bitset>

/**
 * @brief Finds queue families that support graphics, present, transfer, and compute operations.
 * @param physicalDevice The physical device to query.
 * @param surface The surface used for presentation support checks.
 * @return Queue family indices for the requested queue types.
 */
static const QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    int i = 0;
    for (const auto& queueFamily: queueFamilyProperties) {
        // std::cout << std::bitset<8>(queueFamily.queueFlags) << std::endl;    // debug families available on device
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // check for dedicated transfer queue
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            indices.transferFamily = i;
        }

        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete() && indices.transferFamily.has_value()){
            break;
        }

        i++;
    }

    return indices;
}

/**
 * @brief Prints compute-related limits for the selected Vulkan physical device.
 * @param device The physical device to query.
 */
static void printGPUData(const VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceLimits limits = deviceProperties.limits;
    std::cout << "  Maximum workgroups per dispatch: " << limits.maxComputeWorkGroupCount[0] << ", " << limits.maxComputeWorkGroupCount[1] << ", " << limits.maxComputeWorkGroupCount[2] << ", " << "\n";
    std::cout << "  Maximum threads per workgroup: " << limits.maxComputeWorkGroupInvocations << "\n";
    std::cout << "  Maximum threads per dimension: " << limits.maxComputeWorkGroupSize[0] << ", " << limits.maxComputeWorkGroupSize[1] << ", " << limits.maxComputeWorkGroupSize[2] << ", " << "\n";
    std::cout << "  Maximum shared memory per workgroup: " << limits.maxComputeSharedMemorySize << "\n";
    if (limits.timestampPeriod == 0)
    {
        throw std::runtime_error("The selected device does not support timestamp queries!");
    }
    else {
        std::cout << "The timestamp queries supported" << std::endl;
    }
    // if (!limits.timestampComputeAndGraphics)
    // {
    //     throw std::runtime_error{"The selected graphics queue family does not support timestamp queries!"};
    // }
}

/**
 * @brief Prints subgroup properties for the selected Vulkan physical device.
 * @param device The physical device to query.
 */
static void printSubgroupProperties(const VkPhysicalDevice device) {
    VkPhysicalDeviceSubgroupProperties subgroupProperties;
    subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    subgroupProperties.pNext = NULL;

    VkPhysicalDeviceProperties2 physicalDeviceProperties;
    physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physicalDeviceProperties.pNext = &subgroupProperties;

    vkGetPhysicalDeviceProperties2(device, &physicalDeviceProperties);

    std::cout << "Subgroup size: " << subgroupProperties.subgroupSize << std::endl;
    std::cout << "Supported stages: " << std::bitset<8>(subgroupProperties.supportedStages) << std::endl;;
    std::cout << "Supported operations: " << std::bitset<12>(subgroupProperties.supportedOperations) << std::endl;
}



VulkanContext::VulkanContext(GLFWwindow* window) {
    createInstance();
    setupDebugMessenger();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
}

VulkanContext::~VulkanContext() {
    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);

    if (enableValidationLayers) {
        destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(instance, nullptr);
}

DeviceSurface VulkanContext::getDeviceSurfaceHandle() {
    return DeviceSurface{physicalDevice, surface};
}

PhysicalDeviceInstance VulkanContext::getPhysicalDeviceInstance() {
    return PhysicalDeviceInstance{instance, physicalDevice, device};
}

VkPhysicalDeviceMemoryProperties VulkanContext::getMemoryProperties() {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    return memProperties;
}

VkPhysicalDeviceProperties VulkanContext::getDeviceProperties() {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    return properties;
}



VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
    ) {
    if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        std::cerr << "Possible mistake: " << pCallbackData->pMessage << std::endl;
    }
    else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        std::cerr << "Non-optimal use: " << pCallbackData->pMessage << std::endl;
    }
    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    }

    return VK_FALSE;
}

VkResult VulkanContext::createDebugUtilsMessengerEXT(const VkInstance instance, 
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    // Function vkCreateDebugUtilsMessengerEXT is an extension function and is not automatically loaded
    // We have to look up the address of the function using vkGetInstanceProcAddr
    
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    else {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }    
}

void VulkanContext::destroyDebugUtilsMessengerEXT(const VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
    else {
        std::cerr << "Couldn't destroy the DebugsMessenger" << std::endl;
    }
}

void VulkanContext::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerCI){
    messengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | 
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerCI.pfnUserCallback = debugCallback;
    messengerCI.pUserData = nullptr;
    messengerCI.flags = 0;
}

void VulkanContext::setupDebugMessenger() {
    if (!enableValidationLayers) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT messengerCI{};
    populateDebugMessengerCreateInfo(messengerCI);

    if (createDebugUtilsMessengerEXT(instance, &messengerCI, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }

}



bool VulkanContext::checkValidationSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool found = false;

        for (const auto& supportedLayer : availableLayers){
            if (strcmp(layerName, supportedLayer.layerName) == 0){
                found = true;
                break;
            }
        }

        if (!found){
            return false;
        }
    }
    std::cout << "Validation layers are supproted!" << std::endl;
    return true;
}

void VulkanContext::checkExtensionSupport(const std::vector<const char*>& requiredExtensions) {
    // Gets only the supported extensions
    uint32_t extensionCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);     // request the number of extensions
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    // Check for support
    for (const auto& requiredExtension: requiredExtensions) {
        bool found = false;
        for (const auto& supportedExtension: availableExtensions) {
            if (std::strcmp(supportedExtension.extensionName, requiredExtension) == 0){
                found = true;
                break;
            }
        }

        if (found) {
            std::cout << "Supported extension: " << requiredExtension << std::endl;
        }
        else {
            std::cerr << "NOT supported extension: " << requiredExtension << std::endl;
        }
    }
}

std::vector<const char*> VulkanContext::getRequiredExtensions() {
    // Gets required extensions for glfw
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions+glfwExtensionCount);
    if (enableValidationLayers){
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    checkExtensionSupport(extensions);      // not needed, we could check essential extentions
    return extensions;
}



void VulkanContext::createInstance(){
    VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Exterior Mapping 2",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_1
        };

        std::vector<const char*> extensions = getRequiredExtensions();

        VkInstanceCreateInfo instanceCI{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        // for instance create and destroy functions validation must be done explicitly using pNext variable
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{}; 

        if (enableValidationLayers && checkValidationSupport() == false){
            throw std::runtime_error("Validation layers are not supported");
        }
        else if (enableValidationLayers) {
            populateDebugMessengerCreateInfo(debugCreateInfo);

            instanceCI.pNext = &debugCreateInfo;
            instanceCI.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            instanceCI.ppEnabledLayerNames = validationLayers.data();
        }

        if (vkCreateInstance(&instanceCI, nullptr, &instance) != VK_SUCCESS){
            throw std::runtime_error("Failed to create an instance!");
        }
}

void VulkanContext::createSurface(GLFWwindow* window) {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}

bool VulkanContext::isDeviceSuitable(const VkPhysicalDevice physicalDevice, const QueueFamilyIndices& indices) {
    bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails SwapChainSupport = querySwapChainSupport(physicalDevice, surface);
        swapChainAdequate = !SwapChainSupport.formats.empty() && !SwapChainSupport.presentModes.empty(); 
    }

    VkPhysicalDeviceFeatures supportedFeatures{};
    vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;        // we will settle with any GPU with Vulkan support and graphics family
}

bool VulkanContext::checkDeviceExtensionSupport(const VkPhysicalDevice physicalDevice) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension: availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

unsigned int VulkanContext::rateDeviceSuitability(const VkPhysicalDevice device, const QueueFamilyIndices& indices) {
    unsigned int score = 0;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    if (indices.transferFamily.has_value()){
        score += 10;
    }

    return score;
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0){
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::multimap<unsigned int, VkPhysicalDevice> candidates;

    for (const auto& device: devices){
        QueueFamilyIndices indices = findQueueFamilies(device, surface);
        if (isDeviceSuitable(device, indices)) {
            unsigned int score = rateDeviceSuitability(device, indices);
            candidates.insert(std::make_pair(score, device));
        }
    }

    if (candidates.size() == 0) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
    else {
        physicalDevice = candidates.rbegin()->second;
        familyIndices = findQueueFamilies(physicalDevice, surface);

        VkPhysicalDeviceProperties deviceProperties;    // I dont like this
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        std::cout << "Picked device: " << deviceProperties.deviceName << std::endl;

        printGPUData(physicalDevice);
        printSubgroupProperties(physicalDevice);
    }
}

void VulkanContext::createLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> deviceQueueCIs;
    // using a single queue for all families, use multiple queues in the future
    std::set<uint32_t> uniqueQueueFamilies = {familyIndices.graphicsFamily.value(), familyIndices.presentFamily.value()};
    float queuePriority = 1.0f;

    for (uint32_t queueFamilyIndex: uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo deviceQueueCI{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority      // required even for a single queue
        };
        deviceQueueCIs.push_back(deviceQueueCI);
    }

    VkPhysicalDeviceFeatures deviceFeatures{
        .samplerAnisotropy = VK_TRUE,
        .fragmentStoresAndAtomics = VK_TRUE         // create metadata in fragment shader
    };
    VkDeviceCreateInfo deviceCI{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCIs.size()),
        .pQueueCreateInfos = deviceQueueCIs.data(),
        // enableLayerCount and ppEnabledLayerNames are ignored by up to date implementations
        // as there is no distinction between instance and device specific layers anymore
        // but its good idea to set them up anyway to be compatible with older implementations
        .enabledLayerCount = 0,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    if (enableValidationLayers) {
        deviceCI.enabledLayerCount = validationLayers.size();
        deviceCI.ppEnabledLayerNames = validationLayers.data();
    }
    
    if (vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(device, familyIndices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, familyIndices.presentFamily.value(), 0, &presentQueue);
    vkGetDeviceQueue(device, familyIndices.computeFamily.value(), 0, &computeQueue);    // for the time being its the same as graphics
}