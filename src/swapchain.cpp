#include <swapchain.hpp>
#include <vulkanContext.hpp>
#include <util.hpp>
#include <memManager.hpp>

/**
 * @brief Finds a supported Vulkan format from a prioritized candidate list.
 *
 * Checks candidate formats against the physical device's format properties for
 * the requested tiling and usage features.
 * @param physicalDevice Physical device used for format queries.
 * @param candidates Ordered list of candidate formats to test.
 * @param tiling Desired image tiling mode.
 * @param features Required format features.
 * @return First supported VkFormat matching the requirements.
 * @throws std::runtime_error if no candidate format is supported.
 */
static VkFormat findSupportedFormat(const VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find a supported format!");
}

/**
 * @brief Selects a depth format supported by the physical device.
 *
 * Uses a small set of common depth/stencil formats and prefers a 32-bit depth-only format.
 * @param physicalDevice Physical device used for format queries.
 * @return Supported VkFormat for depth attachment use.
 */
static VkFormat findDepthFormat(const VkPhysicalDevice physicalDevice) {
    std::vector<VkFormat> depthCandidateFormats = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    return findSupportedFormat(physicalDevice, depthCandidateFormats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

SwapChain::SwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices, const VkDevice device, GLFWwindow* window, MemoryManager& manager)
    : deviceHandle(device), windowHandle(window), memManager(manager) {
    createSwapChain(deviceSurfaceHandle, indices);
    createImageViews();
    depthFormat = findDepthFormat(deviceSurfaceHandle.physicalDevice);
    createDepthResources();
    createGTBufferResources();
}

SwapChain::~SwapChain() {
    destroyGTBufferResources();
    cleanupSwapchain();
}

AttachementsFormats SwapChain::getAttachementsFormats() {
    return AttachementsFormats{swapChainImageFormat, depthFormat};
}

void SwapChain::transitionToFinalLayout(MemoryManager& memManager, uint32_t imageindex) {
    memManager.transitionImageLayout(swapChainImages[imageindex], swapChainImageFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB and availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    
    return VK_PRESENT_MODE_FIFO_KHR;        // is guaranteed to be available
}

VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // resolution of the swap chain images, almost always equal to resolution of window in pixels
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    else {  // screen coordinates do not correspond to the pixels
        int width, height;
        glfwGetFramebufferSize(windowHandle, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void SwapChain::createSwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(deviceSurfaceHandle.physicalDevice, deviceSurfaceHandle.surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapchainCI{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = deviceSurfaceHandle.surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,      // amount of layers each image consists of
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .preTransform = swapChainSupport.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    //  handlinging swap images across multiple queues
    if (indices.graphicsFamily != indices.presentFamily) {
        swapchainCI.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCI.queueFamilyIndexCount = 2;
        swapchainCI.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCI.queueFamilyIndexCount = 0;      // optional
        swapchainCI.pQueueFamilyIndices = nullptr;  // optional
    }

    if (vkCreateSwapchainKHR(deviceHandle, &swapchainCI, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }
    swapChainExtent = extent;
    swapChainImageFormat = surfaceFormat.format;

    vkGetSwapchainImagesKHR(deviceHandle, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(deviceHandle, swapChain, &imageCount, swapChainImages.data());
}

void SwapChain::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);
    }
}

void SwapChain::createFramebuffers(const VkRenderPass renderPass) {
    swapChainFramebuffers.resize(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachements = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferCI{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = static_cast<uint32_t>(attachements.size()),
            .pAttachments = attachements.data(),
            .width = swapChainExtent.width,
            .height = swapChainExtent.height,
            .layers = 1
        };

        if (vkCreateFramebuffer(deviceHandle, &framebufferCI, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create frambuffers!");
        }
    }
}

void SwapChain::createDepthResources() {
    memManager.createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImage,
        VMA_MEMORY_USAGE_GPU_ONLY, depthImageMemory);

    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, deviceHandle);

    // doesn't need explicit transition as it is taken care of by renderpass
    // doesn't work for some reason, probably something witht he renderpass transitioning the image as well causing the command pools to fail
    // transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void SwapChain::createGTBufferResources() {
    destroyGTBufferResources();

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(swapChainExtent.width) *
                             static_cast<VkDeviceSize>(swapChainExtent.height) *
                             4;

    VmaAllocationInfo allocInfo{};
    allocInfo = memManager.createBuffer(imageSize,
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VK_SHARING_MODE_EXCLUSIVE,
                            groundTruthBuffer,
                            VMA_MEMORY_USAGE_GPU_TO_CPU,
                            groundTruthBufferMemory,
                            nullptr,
                            true);

    GTBufferMapped = allocInfo.pMappedData;
}

void SwapChain::saveGTImage(uint32_t imageIndex, VkImageLayout currLayout) {
    createGTBufferResources();

    memManager.transitionImageLayout(swapChainImages[imageIndex],
                                     swapChainImageFormat,
                                     currLayout,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    memManager.copyImageToBuffer(swapChainImages[imageIndex],
                                 swapChainImageFormat,
                                 groundTruthBuffer,
                                 swapChainExtent.width,
                                 swapChainExtent.height);
    memManager.transitionImageLayout(swapChainImages[imageIndex],
                                     swapChainImageFormat,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     currLayout);
    groundTruthExtent = swapChainExtent;
    GTBufferValid = true;

    std::cout << "Ground truth image saved captured!" << std::endl;
}

void SwapChain::createCompareBuffer(VkBuffer& compareBuffer,
                                    VmaAllocation& compareBufferMemory,
                                    void*& compareBufferMapped,
                                    uint32_t imageIndex,
                                    VkImageLayout currLayout) {
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(swapChainExtent.width) *
                             static_cast<VkDeviceSize>(swapChainExtent.height) *
                             4;

    VmaAllocationInfo allocInfo{};
    allocInfo = memManager.createBuffer(imageSize,
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VK_SHARING_MODE_EXCLUSIVE,
                            compareBuffer,
                            VMA_MEMORY_USAGE_GPU_TO_CPU,
                            compareBufferMemory,
                            nullptr,
                            true);

    compareBufferMapped = allocInfo.pMappedData;
    
    memManager.transitionImageLayout(swapChainImages[imageIndex],
                                     swapChainImageFormat,
                                     currLayout,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    memManager.copyImageToBuffer(swapChainImages[imageIndex],
                                 swapChainImageFormat,
                                 compareBuffer,
                                 swapChainExtent.width,
                                 swapChainExtent.height);
    memManager.transitionImageLayout(swapChainImages[imageIndex],
                                     swapChainImageFormat,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     currLayout);
}

float SwapChain::renderPrecision(uint32_t imageIndex, VkImageLayout currLayout, uint32_t compareFlag) {
    if (GTBufferMapped == nullptr) {
        throw std::runtime_error("Ground truth buffer is not mapped!");
    }
    if (swapChainExtent.width != groundTruthExtent.width || swapChainExtent.height != groundTruthExtent.height) {
        throw std::runtime_error("Invalid swap chain extent!");
    }

    VkBuffer compResultBuffer = VK_NULL_HANDLE;
    VmaAllocation compResultBufferMemory = VK_NULL_HANDLE;
    void* compResultBufferMapped = nullptr;

    createCompareBuffer(compResultBuffer, compResultBufferMemory, compResultBufferMapped, imageIndex, currLayout);
    

    float precision_val = 0.0f;
    if (compareFlag == 0) {         // MSE
        precision_val = calculateMSE(static_cast<uint8_t*>(GTBufferMapped), static_cast<uint8_t*>(compResultBufferMapped), swapChainExtent.width, swapChainExtent.height);
    }
    else if (compareFlag == 1) {    // SIMM
        precision_val = calculateSSIM(static_cast<uint8_t*>(GTBufferMapped), static_cast<uint8_t*>(compResultBufferMapped), swapChainExtent.width, swapChainExtent.height);
    }
    else {
        memManager.destroyBuffer(compResultBuffer, compResultBufferMemory);
        throw std::runtime_error("Invalid compare method flag!");
    }

    memManager.destroyBuffer(compResultBuffer, compResultBufferMemory);

    return precision_val;
}

bool SwapChain::isGTvalid() {
    return GTBufferValid;
}

void SwapChain::recreateSwapChain(const DeviceSurface& deviceSurfaceHandle, const QueueFamilyIndices& indices, const VkRenderPass renderPass) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(windowHandle, &width, &height);
    if (width == 0 || height == 0) {
        glfwGetFramebufferSize(windowHandle, &width, &height);
        glfwWaitEvents();
    }
    
    vkDeviceWaitIdle(deviceHandle);

    cleanupSwapchain();

    createSwapChain(deviceSurfaceHandle, indices);
    createImageViews();
    createDepthResources();
    createFramebuffers(renderPass);
}

void SwapChain::destroyGTBufferResources() {
    if (groundTruthBuffer != VK_NULL_HANDLE) {
        memManager.destroyBuffer(groundTruthBuffer, groundTruthBufferMemory);
        groundTruthBuffer = VK_NULL_HANDLE;
        groundTruthBufferMemory = VK_NULL_HANDLE;
        GTBufferMapped = nullptr;
        GTBufferValid = false;
    }
}

void SwapChain::cleanupSwapchain() {
    for (auto framebuffer: swapChainFramebuffers){
        vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);
    }

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(deviceHandle, imageView, nullptr);
    }

    vkDestroySwapchainKHR(deviceHandle, swapChain, nullptr);

    memManager.destroyImage(depthImage, depthImageMemory);
    vkDestroyImageView(deviceHandle, depthImageView, nullptr);
}

void SwapChain::saveImageToPNG(uint32_t imageIndex, VkImageLayout currLayout, const std::string& filename) {
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    VkDeviceSize imageSize = swapChainExtent.width * swapChainExtent.height * 4;        // 4 channeles

    memManager.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_GPU_TO_CPU, stagingBufferMemory);

    memManager.transitionImageLayout(swapChainImages[imageIndex], swapChainImageFormat, currLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    memManager.copyImageToBuffer(swapChainImages[imageIndex], swapChainImageFormat, stagingBuffer, swapChainExtent.width, swapChainExtent.height);
    memManager.transitionImageLayout(swapChainImages[imageIndex], swapChainImageFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, currLayout);

    memManager.saveImage(stagingBufferMemory, swapChainImageFormat, SaveImageFormat::PNG, "../debug/" + filename, swapChainExtent.width, swapChainExtent.height, 1);

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}