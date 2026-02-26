#pragma once

// user includes
#include <camera.hpp>
#include <textures.hpp>

// system includes
#include <vector>

enum class SaveImageFormat;

class OfflineResources {
public:
    Sampler imageSampler;
    Sampler depthSampler;  // needs different setup

    VkImage layeredImage = VK_NULL_HANDLE;
    VkImage depthLayeredImage = VK_NULL_HANDLE;

    bool offlineImagesRendered = false;
    bool imagesInvalid = false;

    OfflineResources(VkDevice device, const VkPhysicalDeviceProperties& prop, MemoryManager& memMan, VkFormat colorFormat, VkFormat depthFormat, VkExtent2D swapChainExtent);
    ~OfflineResources();

    void updateLayered(VkExtent2D swapChainExtent);
    void saveLayeredImages(std::string& filename, SaveImageFormat depthSaveFormat, VkFormat colorFormat, VkFormat depthFormat, glm::vec2 nearFar);

    void createLayeredImage(VkFormat colorFormat, VkFormat depthFormat, bool dummy = false);

    VkImageView getImageView(ImageViewType type);
    void setLayerCount(u_int32_t count);
    void setRendered();
    void transferLayered(VkImageLayout newLayout, VkFormat colorFormat, VkFormat depthFormat, VkCommandBuffer commandBuffer);
private:
    VmaAllocation layeredImageMemory;
    VkImageView layeredImageView;

    VmaAllocation depthLayeredImageMemory;
    VkImageView depthLayeredImageView;

    VkImageLayout layeredLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D layeredExtent;
    uint32_t usedLayerCount;            // equal to camCount
    uint32_t allocatedLayerCount = 0;       // the image is not recreated when deleting cameras

    VkDevice deviceHandle;
    MemoryManager& memManager;

    void destroyLayeredImage();
};

class CamerasManager {
public:
    Camera* activeCam;

    std::vector<OfflineCamera> camArray;
    NovelCamera novelView;
    Camera observer;

    uint32_t sampleCount = 16;
    uint32_t sampleDebug = 16;

    CamerasManager(VkDevice device, MemoryManager& memMan, VkExtent2D swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass, const VkPhysicalDeviceProperties& prop);
    ~CamerasManager();

    void updateResize(VkExtent2D swapChainExtent);
    void toggleNovel();
    void toggleObserver();
    void nextCam(bool ignoreNovelView = false);
    void setActiveCam(uint32_t newIndex);

    void addCam(MemoryManager& memManager);
    void deleteCam(MemoryManager& memManager);

    uint32_t getCamCount() const;
    uint32_t getActiveIndex() const;
    bool novelViewToggeled();
    bool observerToggeled();

    VkImageView getImageView(ImageViewType type);
    void createLayeredImages();

    bool imagesInvalid();
    bool imagesRendered();
    void setImagesRendered();
    VkSampler getSampler(ImageViewType type);

    void saveImages(std::string& filename, SaveImageFormat depthSaveFormat);
    void transferLayeredLayout(VkImageLayout layout, VkCommandBuffer commandBuffer);
private:
    OfflineResources resources;
    bool framebuffersInvalid = true;

    bool novelViewActive = true;
    bool observerActive = false;

    VkFormat colorFormat;
    VkFormat depthFormat;
    float extentRatio;

    uint32_t camCount;
    uint32_t activeIndex = 0;

    VkExtent2D swapChainExtentHandle;       // size of the rendered images


    // Vulkan Handles
    VkDevice deviceHandle;
    VkRenderPass renderpassHandle;
};