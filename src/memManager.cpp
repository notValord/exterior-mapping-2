#define VMA_IMPLEMENTATION
#include <memManager.hpp>
#include <vulkanContext.hpp>
#include <util.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>


static bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

MemoryManager::MemoryManager(const PhysicalDeviceInstance& phyDevInst, const VkCommandPool transferPool, const VkQueue transferQueue)
    : deviceHandle(phyDevInst.device), transferPoolHandle(transferPool), transferQueueHandle(transferQueue) {
    VmaAllocatorCreateInfo allocatorInfo = {
        .physicalDevice = phyDevInst.physicalDevice,
        .device = phyDevInst.device,
        .instance = phyDevInst.instance,
        .vulkanApiVersion = VK_API_VERSION_1_0
    };

    vmaCreateAllocator(&allocatorInfo, &allocator);
}

MemoryManager::~MemoryManager(){
    vmaDestroyAllocator(allocator);
}

VkCommandBuffer MemoryManager::beginSingleCommand() {
    return beginSingleTimeCommands(deviceHandle, transferPoolHandle);
}

void MemoryManager::submitSingleCommand(VkCommandBuffer& commandBuffer) {
    endSingleTimeCommands(commandBuffer, transferQueueHandle, deviceHandle, transferPoolHandle);
}

void MemoryManager::createImage(int width, int height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImage& image,
    VmaMemoryUsage vmaUsage, VmaAllocation& allocation, uint32_t layers) {
    VkImageCreateInfo imageCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},       // width, height, depth
        .mipLevels = 1,
        .arrayLayers = layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocationCI{
        .usage = vmaUsage,
    };

    vmaCreateImage(allocator, &imageCI, &allocationCI, &image, &allocation, nullptr);
}

void MemoryManager::destroyImage(VkImage& image, VmaAllocation& allocation) {
    vmaDestroyImage(allocator, image, allocation);
}

void MemoryManager::transitionImageLayout(VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCnt, VkCommandBuffer commandBuffer) {
    bool submit = (commandBuffer == VK_NULL_HANDLE) ? true : false;
    if (submit) {
        commandBuffer = beginSingleTimeCommands(deviceHandle, transferPoolHandle);
    }

    VkImageSubresourceRange subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = layerCnt
    };
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        subresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencilComponent(format)){
            subresource.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    // used for transition image layouts
    VkImageMemoryBarrier imageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = 0,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,     // used when transfering ownership between queue families
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresource,
    };

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    switch (oldLayout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            sourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;

        case VK_IMAGE_LAYOUT_GENERAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        
        default:
            throw std::runtime_error("Unsuported layout transition!");
    };

    switch (newLayout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;

        case VK_IMAGE_LAYOUT_GENERAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        
        default:
            throw std::runtime_error("Unsuported layout transition!");
    };

    // if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    // }
    // else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    //     sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    //     destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    // }
    // else {
    //     throw std::runtime_error("Unsuported layout transition!");
    // }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1 , &imageMemoryBarrier);

    if (submit) {
        endSingleTimeCommands(commandBuffer, transferQueueHandle, deviceHandle, transferPoolHandle);
    }
}

void MemoryManager::copyBufferToImage(VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(deviceHandle, transferPoolHandle);

    VkImageSubresourceLayers imageSubresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    VkBufferImageCopy bufferImageCopy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,

        .imageSubresource = imageSubresource,
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };
    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

    endSingleTimeCommands(commandBuffer, transferQueueHandle, deviceHandle, transferPoolHandle);
}

void MemoryManager::copyImageToBuffer(VkImage& image, VkFormat imageFormat, VkBuffer& buffer, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(deviceHandle, transferPoolHandle);

    VkImageSubresourceLayers imageSubresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    if (imageFormat == VK_FORMAT_D16_UNORM || imageFormat == VK_FORMAT_D32_SFLOAT) {
        imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    VkBufferImageCopy bufferImageCopy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,

        .imageSubresource = imageSubresource,
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };
    vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &bufferImageCopy);

    endSingleTimeCommands(commandBuffer, transferQueueHandle, deviceHandle, transferPoolHandle);
}

void MemoryManager::copyImage(VkImage& srcImage, VkFormat srcImageFormat, VkImage& dstImage, VkFormat dstImageFormat, VkExtent3D extent) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(deviceHandle, transferPoolHandle);

    VkImageSubresourceLayers srcImageSubresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    if (srcImageFormat == VK_FORMAT_D16_UNORM || srcImageFormat == VK_FORMAT_D32_SFLOAT) {
        srcImageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageSubresourceLayers dstImageSubresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    if (dstImageFormat == VK_FORMAT_D16_UNORM || dstImageFormat == VK_FORMAT_D32_SFLOAT) {
        dstImageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    VkImageCopy imageCopy {
        .srcSubresource = srcImageSubresource,
        .srcOffset = {0, 0, 0},
        .dstSubresource = dstImageSubresource,
        .dstOffset = {0, 0, 0},
        .extent = extent,
    };
    vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

    endSingleTimeCommands(commandBuffer, transferQueueHandle, deviceHandle, transferPoolHandle);
}

void MemoryManager::copyLayeredImage(VkCommandBuffer& commandBuffer, VkImage& srcImage, VkFormat srcImageFormat, VkImage& dstImage, VkFormat dstImageFormat, VkExtent3D extent, uint32_t layerId) {
    VkImageSubresourceLayers srcImageSubresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    if (srcImageFormat == VK_FORMAT_D16_UNORM || srcImageFormat == VK_FORMAT_D32_SFLOAT) {
        srcImageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageSubresourceLayers dstImageSubresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = layerId,
        .layerCount = 1
    };
    if (dstImageFormat == VK_FORMAT_D16_UNORM || dstImageFormat == VK_FORMAT_D32_SFLOAT) {
        dstImageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    VkImageCopy imageCopy {
        .srcSubresource = srcImageSubresource,
        .srcOffset = {0, 0, 0},
        .dstSubresource = dstImageSubresource,
        .dstOffset = {0, 0, 0},
        .extent = extent,
    };
    vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
}

VmaAllocationInfo MemoryManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkBuffer& buffer,
     VmaMemoryUsage vmaUsage, VmaAllocation& allocation, void* data, bool mapped) {
    VkBufferCreateInfo bufferCI{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = sharingMode,
    };

    VmaAllocationCreateInfo allocationCI = {
        .usage = vmaUsage,
    };
    if (mapped) {
        allocationCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                  VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocationInfo allocInfo;
    vmaCreateBuffer(allocator, &bufferCI, &allocationCI, &buffer, &allocation, &allocInfo);

    if (data) {
        populateBuffer(allocation, data, size, !(allocInfo.memoryType & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    }

    return allocInfo;
}

void MemoryManager::populateBuffer(VmaAllocation& allocation, void* data, VkDeviceSize bufferSize, bool flush) {
    void* mappedPtr;
    vmaMapMemory(allocator, allocation, &mappedPtr);
    if (mappedPtr == nullptr) {
        throw std::runtime_error("Failed to populate buffer!");
    }
    memcpy(mappedPtr, data, bufferSize);

    // Flush if not coherent
    if (flush) {
        vmaFlushAllocation(allocator, allocation, 0, bufferSize);
    }

    vmaUnmapMemory(allocator, allocation);
}

void MemoryManager::destroyBuffer(VkBuffer& buffer, VmaAllocation& allocation) {
    vmaDestroyBuffer(allocator, buffer, allocation);
}

void MemoryManager::copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(deviceHandle, transferPoolHandle);

    VkBufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer, transferQueueHandle, deviceHandle, transferPoolHandle);
}

void MemoryManager::saveImage(VmaAllocation& allocation, VkFormat imageFormat, SaveImageFormat saveFormat, std::string filename, 
                                uint32_t width, uint32_t height, float zNear, float zFar) {
    void* mappedPtr;
    vmaMapMemory(allocator, allocation, &mappedPtr);

    if (saveFormat == SaveImageFormat::PNG && imageFormat == VK_FORMAT_B8G8R8A8_SRGB) {       // if color image
        uint8_t* pixels = reinterpret_cast<uint8_t*>(mappedPtr);
        for (uint32_t i = 0; i < width * height; i++) {
            uint8_t* p = &pixels[i * 4];
            uint8_t tmp = p[0];  // Blue
            p[0] = p[2];         // Replace with Red for RGB
            p[2] = tmp;          // Replace with Blue for RGB
        }
        
        stbi_write_png((filename+".png").c_str(), width, height, 4, mappedPtr, width * 4);
    }
    else if (imageFormat == VK_FORMAT_D32_SFLOAT) {     // if depth image
        float* depthData = reinterpret_cast<float*>(mappedPtr);

        if (saveFormat == SaveImageFormat::HDR) {
            std::vector<float> hdrData(width*height);

            for (uint32_t i = 0; i < width * height; i++) { // linerize and normalize the depth data
                float linearize =  (zNear * zFar) / (zFar - depthData[i] * (zFar - zNear));
                float normalize = (linearize - zNear) / (zFar - zNear);

                hdrData[i] = normalize;
            }
            stbi_write_hdr((filename+".hdr").c_str(), width, height, 1, hdrData.data());
        }
        else if (saveFormat == SaveImageFormat::EXR) {
            // used example form https://github.com/syoyo/tinyexr.git
            EXRHeader header;
            InitEXRHeader(&header);

            EXRImage image_exr;
            InitEXRImage(&image_exr);

            image_exr.num_channels = 3;
            float* channels[3] = { depthData, depthData, depthData };
            image_exr.images = reinterpret_cast<unsigned char**>(channels);
            image_exr.width = width;
            image_exr.height = height;

            header.num_channels = 3;
            header.channels = (EXRChannelInfo *)malloc(sizeof(EXRChannelInfo) * header.num_channels);
            // Must be (A)BGR order, since most of EXR viewers expect this channel order.
            strncpy(header.channels[0].name, "B", 255); header.channels[0].name[strlen("B")] = '\0';
            strncpy(header.channels[1].name, "G", 255); header.channels[1].name[strlen("G")] = '\0';
            strncpy(header.channels[2].name, "R", 255); header.channels[2].name[strlen("R")] = '\0';

            header.pixel_types = (int *)malloc(sizeof(int) * header.num_channels);
            header.requested_pixel_types = (int *)malloc(sizeof(int) * header.num_channels);
            
            for (int i = 0; i < header.num_channels; i++) {
                header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
                header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF; // pixel type of output image to be stored in .EXR
            }
            
            const char* err = NULL; // or nullptr in C++11 or later.
            int ret = SaveEXRImageToFile(&image_exr, &header, (filename+".exr").c_str(), &err);
            if (ret != TINYEXR_SUCCESS) {
                std::cerr << "Save EXR err:" << err << std::endl;
                FreeEXRErrorMessage(err); // free's buffer for an error message
            }

            free(header.channels);
            free(header.pixel_types);
            free(header.requested_pixel_types);

        }
        else {
            std::cerr << "Failed to save image: Unknown depth format!" << std::endl;
        }
    }
    else {
        std::cerr << "Failed to save image: Unknown format!" << std::endl;
    }

    vmaUnmapMemory(allocator, allocation);
}