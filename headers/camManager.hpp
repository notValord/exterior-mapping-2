#pragma once

#include "camera.hpp"

#include <vector>

class CamerasManager {
public:
    Camera* activeCam;

    std::vector<OfflineCamera> camArray;
    Camera novelView;

    CamerasManager(VkDevice device, MemoryManager& memMan, VkExtent2D& swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass);
    ~CamerasManager();

    void updateResolution(VkExtent2D& swapChainExtent);
    void toggleNovel();
    void nextCam(bool ignoreNovelView = false);

    void addCam(MemoryManager& memManager);
    void deleteCam(MemoryManager& memManager);

    void saveOfflineImages(MemoryManager& memManager);

    uint32_t getCamCount();
    uint32_t getActiveIndex();

private:
    bool novelViewActive = true;

    VkFormat colorFormat;
    VkFormat depthFormat;
    float extentRatio;

    uint32_t camCount;
    uint32_t activeIndex = 0;

    // Vulkan Handles
    VkDevice deviceHandle;
    VkRenderPass renderpassHandle;
    VkExtent2D& swapChainExtentHandle;
};