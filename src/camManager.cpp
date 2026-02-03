#include <camManager.hpp>
#include <swapchain.hpp>
#include <memManager.hpp>
#include <util.hpp>

static const uint32_t START_CAM_COUNT = 2;
extern const uint32_t MAX_CAM_COUNT = 32;       // issue with reallocation of the vector when creating new cameras, causes the objeccts to get destroyed

CamerasManager::CamerasManager(VkDevice device, MemoryManager& memMan, VkExtent2D swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass, const VkPhysicalDeviceProperties& prop)
    : novelView(swapChainExtent.width / (float) swapChainExtent.height, device, memMan), observer(swapChainExtent.width / (float) swapChainExtent.height), extentRatio(swapChainExtent.width / (float) swapChainExtent.height),
      deviceHandle(device), renderpassHandle(renderpass), swapChainExtentHandle(swapChainExtent), colorFormat(formats.colorImageFormat), depthFormat(formats.depthFormat),
      camCount(START_CAM_COUNT), imageSampler(device, prop), depthSampler(device, prop), memManager(memMan) {

    camArray.reserve(MAX_CAM_COUNT);
    for (int i = 0; i < camCount; i++) {
        camArray.emplace_back(extentRatio, deviceHandle, memMan, swapChainExtentHandle, colorFormat, depthFormat, renderpassHandle);
    }

    activeCam = &novelView;
    createLayeredImage(true);
}

CamerasManager::~CamerasManager() {
    destroyLayeredImage();
}

void CamerasManager::updateResize(VkExtent2D swapChainExtent) {
    extentRatio =  swapChainExtent.width / (float) swapChainExtent.height;

    novelView.updateRatio(extentRatio);
    observer.updateRatio(extentRatio);
    for (auto& cam: camArray) {
        cam.updateRatio(extentRatio);
    }

    if (!offlineImagesRendered) {       // if the offline images were nto rendered yet
        updateOffline(swapChainExtent);
    }
    else {
        postponeResize = true;
    }
}

void CamerasManager::updateOffline(VkExtent2D swapChainExtent) {
    swapChainExtentHandle = swapChainExtent;
    for (auto& cam: camArray) {
        cam.recreateOfflineResources(swapChainExtentHandle, renderpassHandle, colorFormat, depthFormat);
    }
}

uint32_t CamerasManager::getCamCount() {
    return camCount;
}

uint32_t CamerasManager::getActiveIndex() {
    return activeIndex;
}

bool CamerasManager::novelViewToggeled(){
    return novelViewActive;
}

bool CamerasManager::observerToggeled() {
    return observerActive;
}

void CamerasManager::toggleNovel() {
    novelViewActive = novelViewActive ? false:true;

    if (novelViewActive) {
        activeCam = &novelView;
        observerActive = false;     // exclusion
    }
    else {
        activeCam = &camArray[activeIndex];
    }
}

void CamerasManager::toggleObserver() {
    observerActive = observerActive ? false : true;

    if (observerActive) {
        activeCam = &observer;
        novelViewActive = false;        // exclusion
    }
    else {
        activeCam = &camArray[activeIndex];
    }
}

void CamerasManager::nextCam(bool ignoreNovelView) {
    if (!ignoreNovelView && (novelViewActive || observerActive))
        return;

    activeIndex = (activeIndex+1)%(camArray.size());
    activeCam = &camArray[activeIndex];
}

void CamerasManager::setActiveCam(uint32_t newIndex) {
    if (novelViewActive || observerActive)
        return;

    activeIndex = newIndex%camArray.size();
    activeCam = &camArray[activeIndex];
}

void CamerasManager::addCam(MemoryManager& memManager) {
    if (camCount == MAX_CAM_COUNT) {
        return;
    }
    offlineImagesRendered = false;

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

void CamerasManager::saveOfflineImages(MemoryManager& memManager, std::string& filename, SaveImageFormat depthSaveFormat) {
    if (!offlineImagesRendered) {
        return;
    }
    
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    VkDeviceSize imageSize = swapChainExtentHandle.width * swapChainExtentHandle.height * 4;

    glm::vec2 nearFar = camArray[0].getNearFar();

    memManager.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_GPU_TO_CPU, stagingBufferMemory);
    for (uint32_t i= 0; i < camArray.size(); i++) {
        memManager.transitionImageLayout(camArray[i].colorImage, colorFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        memManager.copyImageToBuffer(camArray[i].colorImage, colorFormat, stagingBuffer, swapChainExtentHandle.width, swapChainExtentHandle.height);
        memManager.transitionImageLayout(camArray[i].colorImage, colorFormat,  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        memManager.saveImage(stagingBufferMemory, colorFormat, SaveImageFormat::PNG, "../debug/Color" + filename + "Cam" + std::to_string(i), swapChainExtentHandle.width, swapChainExtentHandle.height);

        memManager.transitionImageLayout(camArray[i].depthImage, depthFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        memManager.copyImageToBuffer(camArray[i].depthImage, depthFormat, stagingBuffer, swapChainExtentHandle.width, swapChainExtentHandle.height);
        memManager.transitionImageLayout(camArray[i].depthImage, depthFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        memManager.saveImage(stagingBufferMemory, depthFormat, depthSaveFormat, "../debug/Depth" + filename + "Cam" + std::to_string(i), swapChainExtentHandle.width, swapChainExtentHandle.height, nearFar.x, nearFar.y);
    }

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}

void CamerasManager::toggleSampled(MemoryManager& memManager) {
    sampleLayout = sampleLayout ? false : true; // change layout to use the image as a texture to be sampled
    if (sampleLayout) {
        for (uint32_t i= 0; i < camArray.size(); i++) {
            memManager.transitionImageLayout(camArray[i].colorImage, colorFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            
            memManager.transitionImageLayout(camArray[i].depthImage, depthFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }
    else {
        for (uint32_t i= 0; i < camArray.size(); i++) {
            memManager.transitionImageLayout(camArray[i].colorImage, colorFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            
            memManager.transitionImageLayout(camArray[i].depthImage, depthFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
    }
}

void CamerasManager::createLayeredImage(bool dummy) {
    destroyLayeredImage();

    memManager.createImage(swapChainExtentHandle.width, swapChainExtentHandle.height, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, layeredImage, VMA_MEMORY_USAGE_GPU_ONLY,
         layeredImageMemory, camCount);

    memManager.transitionImageLayout(layeredImage, colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, camCount);
    if (!dummy) {
        VkCommandBuffer singleCommandBuffer = memManager.beginSingleCommand();
        for (uint32_t i = 0; i < camCount; i++) {
            memManager.transitionImageLayout(camArray[i].colorImage, colorFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, singleCommandBuffer);
            memManager.copyLayeredImage(singleCommandBuffer, camArray[i].colorImage, colorFormat, layeredImage, colorFormat, {swapChainExtentHandle.width, swapChainExtentHandle.height, 1}, i);
            memManager.transitionImageLayout(camArray[i].colorImage, colorFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, singleCommandBuffer);
        }
        memManager.submitSingleCommand(singleCommandBuffer);
    }
    memManager.transitionImageLayout(layeredImage, colorFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, camCount);

    layeredImageView = createImageView(layeredImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, camCount);
}

void CamerasManager::destroyLayeredImage() {
    if (layeredImage != VK_NULL_HANDLE) {
        memManager.destroyImage(layeredImage, layeredImageMemory);
        vkDestroyImageView(deviceHandle, layeredImageView, nullptr);

        layeredImage = VK_NULL_HANDLE;
    }
}

VkImageView CamerasManager::getImageView() {
    if (layeredImage == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    return layeredImageView;
}