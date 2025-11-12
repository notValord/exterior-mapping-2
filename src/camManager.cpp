#include "camManager.hpp"
#include "swapchain.hpp"
#include "memManager.hpp"

static const uint32_t START_CAM_COUNT = 1;

CamerasManager::CamerasManager(VkDevice device, MemoryManager& memMan, VkExtent2D& swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass)
    : novelView(swapChainExtent.width / (float) swapChainExtent.height), extentRatio(swapChainExtent.width / (float) swapChainExtent.height), deviceHandle(device), renderpassHandle(renderpass),
      swapChainExtentHandle(swapChainExtent), colorFormat(formats.colorImageFormat), depthFormat(formats.depthFormat), camCount(START_CAM_COUNT) {

    camArray.reserve(2);
    for (int i = 0; i < 2; i++) {
        camArray.emplace_back(extentRatio, deviceHandle, memMan, swapChainExtentHandle, colorFormat, depthFormat, renderpassHandle);
    }

    activeCam = &novelView;
}

CamerasManager::~CamerasManager() {

}

void CamerasManager::updateResolution(VkExtent2D& swapChainExtent) {
    extentRatio =  swapChainExtent.width / (float) swapChainExtent.height;
    swapChainExtentHandle = swapChainExtent;

    novelView.updateRatio(extentRatio);
    for (auto& cam: camArray) {
        cam.updateRatio(extentRatio);
        cam.recreateOfflineResources(swapChainExtentHandle, renderpassHandle, colorFormat, depthFormat);
    }
}

void CamerasManager::toggleNovel() {
    novelViewActive = novelViewActive ? false:true;
    std::cout << "Novel view " << novelViewActive << std::endl;
    if (novelViewActive) {
        activeCam = &novelView;
    }
    else {
        activeCam = &camArray[activeIndex];
    }
}

void CamerasManager::nextCam(bool ignoreNovelView) {
    if (!ignoreNovelView && novelViewActive)
        return;

    activeIndex = (activeIndex+1)%(camArray.size());
    activeCam = &camArray[activeIndex];
}

void CamerasManager::addCam(MemoryManager& memManager) {
    camArray.emplace_back(extentRatio, deviceHandle, memManager, swapChainExtentHandle, colorFormat, depthFormat, renderpassHandle);
    camCount++;
}

void CamerasManager::deleteCam(MemoryManager& memManager) {
    if (camCount <= START_CAM_COUNT) {
        std::cerr << "FAIL: Minimal cameras count(" << camCount <<") reached!";
        return;
    }

    if (activeIndex == camArray.size() - 1) {   // we want to remove set cam
        if (novelViewActive) {  // only change index 
            activeIndex = (activeIndex+1)%(camArray.size());
        }
        else {  // change index and change cam
            nextCam();
        }
    }
    camArray.pop_back();
    camCount--;
}

void CamerasManager::saveOfflineImages(MemoryManager& memManager) {
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    VkDeviceSize imageSize = swapChainExtentHandle.width * swapChainExtentHandle.height * 4;

    glm::vec2 nearFar = camArray[0].getNearFar();

    memManager.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_GPU_TO_CPU, stagingBufferMemory);
    for (uint32_t i= 0; i < camArray.size(); i++) {
        memManager.transitionImageLayout(camArray[i].colorImage, colorFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        memManager.copyImageToBuffer(camArray[i].colorImage, colorFormat, stagingBuffer, swapChainExtentHandle.width, swapChainExtentHandle.height);
        memManager.transitionImageLayout(camArray[i].colorImage, colorFormat,  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        memManager.saveImage(stagingBufferMemory, colorFormat, "../debug/ColorImageCam" + std::to_string(i) + ".png", swapChainExtentHandle.width, swapChainExtentHandle.height);

        memManager.transitionImageLayout(camArray[i].depthImage, depthFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        memManager.copyImageToBuffer(camArray[i].depthImage, depthFormat, stagingBuffer, swapChainExtentHandle.width, swapChainExtentHandle.height);
        memManager.transitionImageLayout(camArray[i].depthImage, depthFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        memManager.saveImage(stagingBufferMemory, depthFormat, "../debug/DepthImageCam" + std::to_string(i) + ".hdr", swapChainExtentHandle.width, swapChainExtentHandle.height, nearFar.x, nearFar.y);
    }

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}

uint32_t CamerasManager::getCamCount() {
    return camCount;
}

uint32_t CamerasManager::getActiveIndex() {
    return activeIndex;
}