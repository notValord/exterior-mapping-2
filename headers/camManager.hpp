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

class NovelResources {
public:
    std::vector<VkImage> colorImage;
    std::vector<VkImage> depthImage;
    std::vector<VkImage> metadataImage;

    std::vector<VkFramebuffer> novelFramebuffers;

    NovelResources(VkDevice device, MemoryManager& memMan, VkRenderPass renderPass, VkExtent2D extent, const AttachementsFormats& formats);
    ~NovelResources();

    void updateExtent(VkExtent2D newExtent);

    void recreateFrameBuffers();
private:
    std::vector<VmaAllocation> colorImageMemory;
    std::vector<VmaAllocation> depthImageMemory;
    std::vector<VmaAllocation> metadataImageMemory;

    std::vector<VkImageView> colorImageView;
    std::vector<VkImageView> depthImageView;
    std::vector<VkImageView> metadataImageView;

    VkExtent2D imageExtent;
    bool extentChanged = false;

    VkFormat colorFormat;
    VkFormat depthFormat;
    VkFormat metadataFormat;

    VkDevice deviceHandle;
    VkRenderPass renderpassHandle;
    MemoryManager& memManager;

    void createBaseImages();
    void createFrameBuffers();

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

    CamerasManager(VkDevice device, MemoryManager& memMan, VkExtent2D swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass, const VkPhysicalDeviceProperties& prop);
    ~CamerasManager();

    void updateResize(VkExtent2D swapChainExtent);
    void updateNovel();
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

    VkFramebuffer getNovelFramebuffer(uint32_t imageIndex);
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