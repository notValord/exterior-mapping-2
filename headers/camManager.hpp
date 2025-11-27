#pragma once

// user includes
#include <camera.hpp>

// system includes
#include <vector>

enum class SaveImageFormat;

class CamerasManager {
public:
    Camera* activeCam;

    std::vector<OfflineCamera> camArray;
    Camera novelView;

    bool offlineImagesRendered = false;
    bool postponeResize = false;

    CamerasManager(VkDevice device, MemoryManager& memMan, VkExtent2D swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass);
    ~CamerasManager();

    void updateResize(VkExtent2D swapChainExtent);
    void updateOffline(VkExtent2D swapChainExtent);
    void toggleNovel();
    void nextCam(bool ignoreNovelView = false);
    void setActiveCam(uint32_t newIndex);

    void addCam(MemoryManager& memManager);
    void deleteCam(MemoryManager& memManager);

    void saveOfflineImages(MemoryManager& memManager, std::string& filename, SaveImageFormat depthSaveFormat);
    void toggleSampled(MemoryManager& memManager);

    uint32_t getCamCount();
    uint32_t getActiveIndex();
    bool novelViewToggeled();

private:
    bool novelViewActive = true;
    bool sampleLayout = false;

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