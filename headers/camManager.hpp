#pragma once

// user includes
#include <camera.hpp>
#include <textures.hpp>

// system includes
#include <vector>

enum class SaveImageFormat;

class CamerasManager {
public:
    Camera* activeCam;

    std::vector<OfflineCamera> camArray;
    NovelCamera novelView;
    Camera observer;

    Sampler imageSampler;
    Sampler depthSampler;  // needs different setup

    VkImage layeredImage = VK_NULL_HANDLE;
    VkImage depthLayeredImage = VK_NULL_HANDLE;

    uint32_t sampleCount = 16;

    bool offlineImagesRendered = false;
    bool postponeResize = false;

    CamerasManager(VkDevice device, MemoryManager& memMan, VkExtent2D swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass, const VkPhysicalDeviceProperties& prop);
    ~CamerasManager();

    void updateResize(VkExtent2D swapChainExtent);
    void updateOffline(VkExtent2D swapChainExtent);
    void toggleNovel();
    void toggleObserver();
    void nextCam(bool ignoreNovelView = false);
    void setActiveCam(uint32_t newIndex);

    void addCam(MemoryManager& memManager);
    void deleteCam(MemoryManager& memManager);

    void saveOfflineImages(MemoryManager& memManager, std::string& filename, SaveImageFormat depthSaveFormat);

    uint32_t getCamCount();
    uint32_t getActiveIndex();
    bool novelViewToggeled();
    bool observerToggeled();

    void createLayeredImage(bool dummy = false);
    VkImageView getImageView(ImageViewType type);

private:
    bool novelViewActive = true;
    bool observerActive = false;
    bool sampleLayout = false;

    VmaAllocation layeredImageMemory;
    VkImageView layeredImageView;

    VmaAllocation depthLayeredImageMemory;
    VkImageView depthLayeredImageView;

    VkFormat colorFormat;
    VkFormat depthFormat;
    float extentRatio;

    uint32_t camCount;
    uint32_t activeIndex = 0;

    VkExtent2D swapChainExtentHandle;       // size of the rendered images

    void destroyLayeredImage();

    // Vulkan Handles
    VkDevice deviceHandle;
    VkRenderPass renderpassHandle;
    MemoryManager& memManager;
};