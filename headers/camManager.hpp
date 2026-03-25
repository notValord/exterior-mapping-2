#pragma once

// user includes
#include <camera.hpp>
#include <textures.hpp>

// system includes
#include <vector>

extern const size_t MAX_FRAMES_IN_FLIGHT;

enum class SaveImageFormat;

class OfflineResources {
public:
    Sampler imageSampler;
    Sampler depthSampler;  // needs different setup

    VkImage layeredImage = VK_NULL_HANDLE;
    VkImage depthLayeredImage = VK_NULL_HANDLE;

    VkImage depthMipMap0 = VK_NULL_HANDLE;
    VkImage depthMipMap1 = VK_NULL_HANDLE;
    VkImage depthMipMap2 = VK_NULL_HANDLE;
    VkImage depthMipMap3 = VK_NULL_HANDLE;

    bool offlineImagesRendered = false;
    bool imagesInvalid = false;

    OfflineResources(VkDevice device, const VkPhysicalDeviceProperties& prop, MemoryManager& memMan, VkFormat colorFormat, VkFormat depthFormat, VkExtent2D swapChainExtent);
    ~OfflineResources();

    void updateLayered(VkExtent2D swapChainExtent);
    void saveLayeredImages(std::string& filename, SaveImageFormat depthSaveFormat, VkFormat colorFormat, VkFormat depthFormat, glm::vec2 nearFar);

    void createLayeredImage(VkFormat colorFormat, VkFormat depthFormat, bool dummy = false);
    void createMipMapImages(VkFormat depthFormat, bool dummy = false, VkCommandBuffer commandBuffer = VK_NULL_HANDLE);

    VkImageView getImageView(ImageViewType type, int mipMapID = -1);
    void setLayerCount(u_int32_t count);
    void setRendered();
    void transferLayered(VkImageLayout newLayout, VkFormat colorFormat, VkFormat depthFormat, VkCommandBuffer commandBuffer);
private:
    VmaAllocation layeredImageMemory;
    VkImageView layeredImageView;

    VmaAllocation depthLayeredImageMemory;
    VkImageView depthLayeredImageView;

    VmaAllocation depthMipMap0Memory;
    VmaAllocation depthMipMap1Memory;
    VmaAllocation depthMipMap2Memory;
    VmaAllocation depthMipMap3Memory;
    VkImageView depthMipMap0View;
    VkImageView depthMipMap1View;
    VkImageView depthMipMap2View;
    VkImageView depthMipMap3View;

    VkImageLayout layeredLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D layeredExtent;
    uint32_t usedLayerCount;            // equal to camCount
    uint32_t allocatedLayerCount = 0;       // the image is not recreated when deleting cameras

    VkExtent2D mipMap0Extent = {1024, 1024};
    VkExtent2D mipMap1Extent = {256, 128};
    VkExtent2D mipMap2Extent = {32, 32};
    VkExtent2D mipMap3Extent = {8, 4};

    VkDevice deviceHandle;
    MemoryManager& memManager;

    void setMipMapDataLayout(VkFormat depthFormat, VkCommandBuffer commandBuffer);

    void destroyLayeredImage();
    void destroyMipMapImages();
};

class NovelResources {
public:
    std::vector<VkImage> metadataImage;

    std::vector<VkImageView> depthImageView;
    std::vector<VkImageView> metadataImageView;

    std::vector<VkFramebuffer> novelFramebuffers;

    NovelResources(VkDevice device, MemoryManager& memMan, VkRenderPass renderPass, VkExtent2D extent, const AttachementsFormats& formats, const std::vector<VkImageView>& swapChainImageViews);
    ~NovelResources();

    void updateExtent(VkExtent2D newExtent);

    void recreateFrameBuffers(const std::vector<VkImageView>& swapChainImageViews);
private:
    std::vector<VmaAllocation> depthImageMemory;
    std::vector<VmaAllocation> metadataImageMemory;

    std::vector<VkImage> depthImage;

    VkExtent2D imageExtent;
    uint32_t imageCount;

    VkFormat depthFormat;
    VkFormat metadataFormat;

    VkDevice deviceHandle;
    VkRenderPass renderpassHandle;
    MemoryManager& memManager;

    void createBaseImages();
    void createFrameBuffers(const std::vector<VkImageView>& swapChainImageViews);

    void destroyFrameBuffers();
};

class CamerasManager {
public:
    Camera* activeCam;

    std::vector<OfflineCamera> camArray;
    NovelCamera novelView;
    Camera observer;

    uint32_t sampleCount = 16;
    uint32_t sampleDebug = 16;

    CamerasManager(VkDevice device, MemoryManager& memMan, VkExtent2D swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass, const VkPhysicalDeviceProperties& prop, const std::vector<VkImageView>& swapChainImageViews);
    ~CamerasManager();

    void updateResize(VkExtent2D swapChainExtent, const std::vector<VkImageView>& swapChainImageViews);
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
    std::array<VkImageView, 4> getReduceDepthViews();
    std::vector<VkImage> getReduceStorageImages();
    void createLayeredImages();
    void createMipMapImages(VkCommandBuffer commandBuffer);

    bool imagesInvalid();
    bool imagesRendered();
    void setImagesRendered();
    VkSampler getSampler(ImageViewType type);

    void saveImages(std::string& filename, SaveImageFormat depthSaveFormat);
    void transferLayeredLayout(VkImageLayout layout, VkCommandBuffer commandBuffer);

    VkFramebuffer getNovelFramebuffer(uint32_t imageIndex);
    std::vector<VkImageView> getNovelStorageViews();
    VkImage getNovelStorageImage(uint32_t currentFrame);
private:
    OfflineResources offlineRes;
    NovelResources novelRes;
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