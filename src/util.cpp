#include <util.hpp>
#include <swapchain.hpp>

#include <cmath>
#include <vector>

VkImageView createImageView(VkImage& image, VkFormat format, VkImageAspectFlags aspectFlags, const VkDevice deviceHandle, uint32_t layerCnt, uint32_t layerID) {
    VkImageViewCreateInfo imageViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format
    };
    imageViewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCI.subresourceRange.aspectMask = aspectFlags;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = layerID;
    imageViewCI.subresourceRange.layerCount = layerCnt;

    if (layerCnt > 1) {
        imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }

    VkImageView imageView;
    if (vkCreateImageView(deviceHandle, &imageViewCI, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image views!");
    }

    return imageView;
}

VkCommandBuffer beginSingleTimeCommands(const VkDevice deviceHandle, const VkCommandPool commandPoolHandle) {
    VkCommandBufferAllocateInfo commandBufferAI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPoolHandle,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(deviceHandle, &commandBufferAI, &commandBuffer);

    VkCommandBufferBeginInfo commandBufferBI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBI) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin a command buffer!");
    }

    return commandBuffer;
}

void endSingleTimeCommands(VkCommandBuffer commandBuffer, const VkQueue queueHandle, const VkDevice deviceHandle, const VkCommandPool commandPoolHandle) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end recording a command buffer!");
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    vkQueueSubmit(queueHandle, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queueHandle);

    vkFreeCommandBuffers(deviceHandle, commandPoolHandle, 1, &commandBuffer);
}

void beginCommandBuffer(VkCommandBuffer commandBuffer) {
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo commandBufferBI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };

    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBI) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }
}

void submitCommandBuffer(VkCommandBuffer commandBuffer) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice deviceHandle, const VkSurfaceKHR surfaceHandle) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(deviceHandle, surfaceHandle, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(deviceHandle, surfaceHandle, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(deviceHandle, surfaceHandle, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(deviceHandle, surfaceHandle, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(deviceHandle, surfaceHandle, &presentModeCount, details.presentModes.data());
    }
    return details;
}

static inline double srgbToLinear(double value) {
    const double c = value / 255.0;
    if (c <= 0.04045) {
        return c / 12.92;
    }
    return std::pow((c + 0.055) / 1.055, 2.4);
}

float calculateMSE(const uint8_t* gt, const uint8_t* img, uint32_t width, uint32_t height) {
    const uint64_t pixelCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    double sqErr = 0.0;

    for (uint64_t i = 0; i < pixelCount; ++i) {
        const uint64_t base = i * 4;
        for (uint64_t c = 0; c < 3; ++c) {   // ignore alpha
            const double a = srgbToLinear(gt[base + c]);
            const double b = srgbToLinear(img[base + c]);
            const double d = a - b;
            sqErr += d * d;
        }
    }

    return static_cast<float>(sqErr / (static_cast<double>(pixelCount) * 3.0));
}

float calculateSSIM(const uint8_t* gt, const uint8_t* img, uint32_t width, uint32_t height) {
    // Block-based luminance SSIM over linearized color values.
    const double c1 = (0.01 * 0.01);
    const double c2 = (0.03 * 0.03);
    const uint64_t pixelCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    if (pixelCount == 0) {
        return 0.0f;
    }

    constexpr uint32_t blockSize = 8;

    std::vector<double> lumX(pixelCount);
    std::vector<double> lumY(pixelCount);

    auto luminance = [&](const uint8_t* data, uint64_t base) {
        const double r = srgbToLinear(data[base + 0]);
        const double g = srgbToLinear(data[base + 1]);
        const double b = srgbToLinear(data[base + 2]);
        return 0.2126 * r + 0.7152 * g + 0.0722 * b;
    };

    for (uint64_t i = 0; i < pixelCount; ++i) {
        const uint64_t base = i * 4;
        lumX[i] = luminance(gt, base);
        lumY[i] = luminance(img, base);
    }

    double ssimSum = 0.0;
    uint64_t blockCount = 0;

    for (uint32_t y0 = 0; y0 < height; y0 += blockSize) {
        for (uint32_t x0 = 0; x0 < width; x0 += blockSize) {
            const uint32_t bw = std::min(blockSize, width - x0);
            const uint32_t bh = std::min(blockSize, height - y0);
            const uint64_t n = static_cast<uint64_t>(bw) * static_cast<uint64_t>(bh);
            if (n == 0) {
                continue;
            }

            double meanX = 0.0;
            double meanY = 0.0;
            for (uint32_t dy = 0; dy < bh; ++dy) {
                const uint64_t row = static_cast<uint64_t>(y0 + dy) * width;
                for (uint32_t dx = 0; dx < bw; ++dx) {
                    const uint64_t idx = row + (x0 + dx);
                    meanX += lumX[idx];
                    meanY += lumY[idx];
                }
            }
            meanX /= static_cast<double>(n);
            meanY /= static_cast<double>(n);

            double varX = 0.0;
            double varY = 0.0;
            double covXY = 0.0;
            for (uint32_t dy = 0; dy < bh; ++dy) {
                const uint64_t row = static_cast<uint64_t>(y0 + dy) * width;
                for (uint32_t dx = 0; dx < bw; ++dx) {
                    const uint64_t idx = row + (x0 + dx);
                    const double dxv = lumX[idx] - meanX;
                    const double dyv = lumY[idx] - meanY;
                    varX += dxv * dxv;
                    varY += dyv * dyv;
                    covXY += dxv * dyv;
                }
            }

            const double denom = static_cast<double>(n > 1 ? n - 1 : 1);
            varX /= denom;
            varY /= denom;
            covXY /= denom;

            const double num = (2.0 * meanX * meanY + c1) * (2.0 * covXY + c2);
            const double den = (meanX * meanX + meanY * meanY + c1) * (varX + varY + c2);
            ssimSum += (den != 0.0) ? (num / den) : 1.0;
            ++blockCount;
        }
    }

    if (blockCount == 0) {
        return 0.0f;
    }
    return static_cast<float>(ssimSum / static_cast<double>(blockCount));
}