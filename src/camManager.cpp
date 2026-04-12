#include <camManager.hpp>
#include <swapchain.hpp>
#include <memManager.hpp>
#include <util.hpp>

static const uint32_t START_CAM_COUNT = 2;
extern const uint32_t MAX_CAM_COUNT = 32;       // issue with reallocation of the vector when creating new cameras, causes the objeccts to get destroyed

CamerasManager::CamerasManager(VkDevice device,
                               MemoryManager& memMan,
                               VkExtent2D swapChainExtent,
                               const AttachementsFormats& formats,
                               VkRenderPass renderpass,
                               const VkPhysicalDeviceProperties& prop,
                               const std::vector<VkImageView>& swapChainImageViews)
    : novelView(swapChainExtent.width / (float) swapChainExtent.height, device, memMan),
      observer(swapChainExtent.width / (float) swapChainExtent.height),
      extentRatio(swapChainExtent.width / (float) swapChainExtent.height),
      deviceHandle(device),
      renderpassHandle(renderpass),
      swapChainExtentHandle(swapChainExtent),
      colorFormat(formats.colorImageFormat),
      depthFormat(formats.depthFormat),
      camCount(START_CAM_COUNT),
      offlineRes(device, prop, memMan, formats.colorImageFormat, formats.depthFormat, swapChainExtent),
      novelRes(device, memMan, renderpass, swapChainExtent, formats, swapChainImageViews) {

    camArray.reserve(MAX_CAM_COUNT);
    for (int i = 0; i < camCount; i++) {
        camArray.emplace_back(extentRatio, deviceHandle, memMan, swapChainExtentHandle, colorFormat, depthFormat, renderpassHandle);
    }

    activeCam = &novelView;
}

CamerasManager::~CamerasManager() {

}

void CamerasManager::updateResize(VkExtent2D swapChainExtent, const std::vector<VkImageView>& swapChainImageViews) {
    extentRatio = swapChainExtent.width / (float) swapChainExtent.height;
    swapChainExtentHandle = swapChainExtent;
    framebuffersInvalid = true;

    novelView.updateRatio(extentRatio);
    observer.updateRatio(extentRatio);
    for (auto& cam: camArray) {
        cam.updateRatio(extentRatio);
    }

    offlineRes.updateLayered(swapChainExtent);

    novelRes.updateExtent(swapChainExtent);
    novelRes.recreateFrameBuffers(swapChainImageViews);
}

uint32_t CamerasManager::getCamCount() const {
    return camCount;
}

uint32_t CamerasManager::getActiveIndex() const {
    return activeIndex;
}

bool CamerasManager::novelViewToggled(){
    return novelViewActive;
}

bool CamerasManager::observerToggled() {
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

    camArray.emplace_back(extentRatio, deviceHandle, memManager, swapChainExtentHandle, colorFormat, depthFormat, renderpassHandle);
    camCount++;

    offlineRes.setLayerCount(camCount);
    offlineRes.offlineImagesRendered = false;
    framebuffersInvalid = true;
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

    offlineRes.setLayerCount(camCount);
}

VkImageView CamerasManager::getImageView(ImageViewType type) {
    return offlineRes.getImageView(type);
}

std::array<VkImageView, 4> CamerasManager::getReduceDepthViews() {
    std::array<VkImageView, 4> depthViews;

    for (uint32_t i = 0; i < depthViews.size(); i++) {
        depthViews[i] = offlineRes.getImageView(ImageViewType::DEPTH, i);
    }
    return depthViews;
}

std::vector<VkImage> CamerasManager::getReduceStorageImages() {
    std::vector<VkImage> depthImages = {offlineRes.depthMipMap1, offlineRes.depthMipMap2, offlineRes.depthMipMap3};
    return depthImages;
}

void CamerasManager::createLayeredImages() {
    offlineRes.createLayeredImage(colorFormat, depthFormat);

    if (!framebuffersInvalid) {
        return;
    }

    for (uint32_t i = 0; i < camArray.size(); i++) {
        camArray[i].recreateOfflineResources(offlineRes.layeredImage,
                                             offlineRes.depthLayeredImage,
                                             i,
                                             swapChainExtentHandle,
                                             renderpassHandle,
                                             colorFormat,
                                             depthFormat);
    }
    framebuffersInvalid = false;
}

void CamerasManager::createMipMapImages(VkCommandBuffer commandBuffer) {
    offlineRes.createMipMapImages(depthFormat, false, commandBuffer);
}

bool CamerasManager::imagesRendered() {
    return offlineRes.offlineImagesRendered;
}
void CamerasManager::setImagesRendered() {
    offlineRes.setRendered();
}
bool CamerasManager::imagesInvalid() {
    return offlineRes.imagesInvalid || framebuffersInvalid;
}

VkSampler CamerasManager::getSampler(ImageViewType type) {
    if (type == ImageViewType::COLOR) {
        return offlineRes.imageSampler.getSampler();
    }
    else if (type == ImageViewType::DEPTH) {
        return offlineRes.depthSampler.getSampler();
    }
    else {      // unknown type 
        return VK_NULL_HANDLE;
    }
    }

void CamerasManager::saveImages(std::string& filename, SaveImageFormat depthSaveFormat) {
    offlineRes.saveLayeredImages(filename, depthSaveFormat, colorFormat, depthFormat, camArray[0].getNearFar());
}


void CamerasManager::transferLayeredLayout(VkImageLayout layout, VkCommandBuffer commandBuffer) {
    offlineRes.transferLayered(layout, colorFormat, depthFormat, commandBuffer);
}

VkFramebuffer CamerasManager::getNovelFramebuffer(uint32_t imageIndex) {
    return novelRes.novelFramebuffers[imageIndex];
}

std::vector<VkImageView> CamerasManager::getNovelStorageViews() {
    return novelRes.metadataImageView;
}

VkImage CamerasManager::getNovelStorageImage(uint32_t currentFrame) {
    return novelRes.metadataImage[currentFrame];
}

CamSetupJson CamerasManager::jsonSetup() {
    CamSetupJson setup;
    setup.camCount = camArray.size();
    setup.cams.resize(setup.camCount + 2);

    for (uint32_t i = 0; i < setup.cams.size(); i++) {
        if (i == 0) {
            setup.cams[i] = novelView.jsonCam();
        }
        else if (i == 1) {
            setup.cams[i] = observer.jsonCam();
        }
        else {
            setup.cams[i] = camArray[i-2].jsonCam();
        }
    }

    return setup;
}

void CamerasManager::loadFromJson(const CamSetupJson& setup, MemoryManager& memManager) {
    while (camArray.size() < setup.camCount) {
        addCam(memManager);
    }

    while (camArray.size() > setup.camCount) {
        deleteCam(memManager);
    }

    for (uint32_t i = 0; i < setup.cams.size(); i++) {
        CamJson camJson = setup.cams[i];

        if (i == 0) {
            novelView.setPosition(camJson.pos);
            novelView.setYawPitch(camJson.yaw, camJson.pitch);
        }
        else if (i == 1) {
            observer.setPosition(camJson.pos);
            observer.setYawPitch(camJson.yaw, camJson.pitch);
        }
        else {
            camArray[i-2].setPosition(camJson.pos);
            camArray[i-2].setYawPitch(camJson.yaw, camJson.pitch);
        }
    }
}




OfflineResources::OfflineResources(VkDevice device, const VkPhysicalDeviceProperties& prop, MemoryManager& memMan, VkFormat colorFormat, VkFormat depthFormat, VkExtent2D swapChainExtent)
    : imageSampler(device, prop, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE),
      depthSampler(device, prop, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE),
      deviceHandle(device),
      memManager(memMan),
      usedLayerCount(START_CAM_COUNT) {
    updateLayered(swapChainExtent);
    createLayeredImage(colorFormat, depthFormat, true);     // create dummy
    createMipMapImages(depthFormat, true);
}

OfflineResources::~OfflineResources() {
    destroyLayeredImage();
    destroyMipMapImages();
}

void OfflineResources::setLayerCount(u_int32_t count) {
    usedLayerCount = count;
    if (usedLayerCount > allocatedLayerCount) {
        imagesInvalid = true;           // need to allocate more layeres
    }
}

VkImageView OfflineResources::getImageView(ImageViewType type, int mipMapID) {
    if (type == ImageViewType::COLOR) {
        return layeredImageView;
    }
    else if (type == ImageViewType::DEPTH) {
        if (mipMapID == -1) {
            return depthLayeredImageView;
        }
        else if (mipMapID == 0) {
            return depthMipMap0View;
        }
        else if (mipMapID == 1) {
            return depthMipMap1View;
        }
        else if (mipMapID == 2) {
            return depthMipMap2View;
        }
        else if (mipMapID == 3) {
            return depthMipMap3View;
        }
        else {
            return VK_NULL_HANDLE;
        }
    }
    else {
        return VK_NULL_HANDLE;
    }
}

void OfflineResources::updateLayered(VkExtent2D swapChainExtent) {
    layeredExtent = swapChainExtent;
    imagesInvalid = true;
}

void OfflineResources::setRendered() {
    layeredLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    offlineImagesRendered = true;
}

void OfflineResources::createLayeredImage(VkFormat colorFormat, VkFormat depthFormat, bool dummy) {
    if (usedLayerCount <= allocatedLayerCount && !imagesInvalid){
        return;         // got enough layeres allocated
    }

    destroyLayeredImage();

    memManager.createImage(layeredExtent.width,
                           layeredExtent.height,
                           colorFormat,
                           VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           layeredImage,
                           VMA_MEMORY_USAGE_GPU_ONLY,
                           layeredImageMemory,
                           usedLayerCount);

    layeredImageView = createImageView(layeredImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, usedLayerCount);


    memManager.createImage(layeredExtent.width,
                           layeredExtent.height,
                           depthFormat,
                           VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           depthLayeredImage,
                           VMA_MEMORY_USAGE_GPU_ONLY,
                           depthLayeredImageMemory,
                           usedLayerCount);

    depthLayeredImageView = createImageView(depthLayeredImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, deviceHandle, usedLayerCount);

    if (!dummy) {
        imagesInvalid = false;
    }
    layeredLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    allocatedLayerCount = usedLayerCount;
}

void OfflineResources::destroyLayeredImage() {
    if (layeredImage != VK_NULL_HANDLE) {
        memManager.destroyImage(layeredImage, layeredImageMemory);
        vkDestroyImageView(deviceHandle, layeredImageView, nullptr);

        layeredImage = VK_NULL_HANDLE;
    }

    if (depthLayeredImage != VK_NULL_HANDLE) {
        memManager.destroyImage(depthLayeredImage, depthLayeredImageMemory);
        vkDestroyImageView(deviceHandle, depthLayeredImageView, nullptr);

        depthLayeredImage = VK_NULL_HANDLE;
    }
}

void OfflineResources::createMipMapImages(VkFormat depthFormat, bool dummy, VkCommandBuffer commandBuffer) {
    destroyMipMapImages();

    memManager.createImage(mipMap0Extent.width,
                           mipMap0Extent.height,
                           depthFormat,
                           VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                           depthMipMap0,
                           VMA_MEMORY_USAGE_GPU_ONLY,
                           depthMipMap0Memory,
                           usedLayerCount);
    depthMipMap0View = createImageView(depthMipMap0, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, deviceHandle, usedLayerCount);

    memManager.createImage(mipMap1Extent.width,
                           mipMap1Extent.height,
                           VK_FORMAT_R32G32_SFLOAT,
                           VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_STORAGE_BIT,
                           depthMipMap1,
                           VMA_MEMORY_USAGE_GPU_ONLY,
                           depthMipMap1Memory,
                           usedLayerCount);
    depthMipMap1View = createImageView(depthMipMap1, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, usedLayerCount);
    

    memManager.createImage(mipMap2Extent.width,
                           mipMap2Extent.height,
                           VK_FORMAT_R32G32_SFLOAT,
                           VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_STORAGE_BIT,
                           depthMipMap2,
                           VMA_MEMORY_USAGE_GPU_ONLY,
                           depthMipMap2Memory,
                           usedLayerCount);
    depthMipMap2View = createImageView(depthMipMap2, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, usedLayerCount);

    memManager.createImage(mipMap3Extent.width,
                           mipMap3Extent.height,
                           VK_FORMAT_R32G32_SFLOAT,
                           VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_STORAGE_BIT,
                           depthMipMap3,
                           VMA_MEMORY_USAGE_GPU_ONLY,
                           depthMipMap3Memory,
                           usedLayerCount);
    depthMipMap3View = createImageView(depthMipMap3, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, usedLayerCount);

    if (!dummy) {
        setMipMapDataLayout(depthFormat, commandBuffer);
    }
}

void OfflineResources::setMipMapDataLayout(VkFormat depthFormat, VkCommandBuffer commandBuffer) {
    memManager.transitionImageLayout(depthMipMap0, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, usedLayerCount, commandBuffer);

    VkImageLayout oldLayout = layeredLayout;
    if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    memManager.transitionImageLayout(depthLayeredImage, depthFormat, oldLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, usedLayerCount, commandBuffer);

    VkOffset3D srcOffset = {static_cast<int>(layeredExtent.width), static_cast<int>(layeredExtent.height), 1};
    VkOffset3D dstOffset = {static_cast<int>(mipMap0Extent.width), static_cast<int>(mipMap0Extent.height), 1};
    memManager.resampleImage(commandBuffer, depthLayeredImage, VK_IMAGE_ASPECT_DEPTH_BIT, depthMipMap0, VK_IMAGE_ASPECT_DEPTH_BIT, srcOffset, dstOffset, usedLayerCount);

    memManager.transitionImageLayout(depthLayeredImage, depthFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, oldLayout, usedLayerCount, commandBuffer);
    memManager.transitionImageLayout(depthMipMap0, depthFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, usedLayerCount, commandBuffer);

    memManager.transitionImageLayout(depthMipMap1, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, usedLayerCount, commandBuffer);
    memManager.transitionImageLayout(depthMipMap2, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, usedLayerCount, commandBuffer);
    memManager.transitionImageLayout(depthMipMap3, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, usedLayerCount, commandBuffer);
}

void OfflineResources::destroyMipMapImages() {
    if (depthMipMap0 != VK_NULL_HANDLE) {
        memManager.destroyImage(depthMipMap0, depthMipMap0Memory);
        vkDestroyImageView(deviceHandle, depthMipMap0View, nullptr);

        depthMipMap0 = VK_NULL_HANDLE;
    }

    if (depthMipMap1 != VK_NULL_HANDLE) {
        memManager.destroyImage(depthMipMap1, depthMipMap1Memory);
        vkDestroyImageView(deviceHandle, depthMipMap1View, nullptr);

        depthMipMap1 = VK_NULL_HANDLE;
    }

    if (depthMipMap2 != VK_NULL_HANDLE) {
        memManager.destroyImage(depthMipMap2, depthMipMap2Memory);
        vkDestroyImageView(deviceHandle, depthMipMap2View, nullptr);

        depthMipMap2 = VK_NULL_HANDLE;
    }

    if (depthMipMap3 != VK_NULL_HANDLE) {
        memManager.destroyImage(depthMipMap3, depthMipMap3Memory);
        vkDestroyImageView(deviceHandle, depthMipMap3View, nullptr);

        depthMipMap3 = VK_NULL_HANDLE;
    }
}

void OfflineResources::saveLayeredImages(std::string& filename, SaveImageFormat depthSaveFormat, VkFormat colorFormat, VkFormat depthFormat, glm::vec2 nearFar) {
    if (!offlineImagesRendered) {
        throw std::runtime_error("Trying to save not rendered images!");
    }
    
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    VkDeviceSize imageSize = layeredExtent.width * layeredExtent.height * 4;        // 4 channeles

    memManager.createBuffer(imageSize * usedLayerCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_GPU_TO_CPU, stagingBufferMemory);

    memManager.transitionImageLayout(layeredImage, colorFormat, layeredLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, usedLayerCount);
    if (layeredLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        memManager.transitionImageLayout(depthLayeredImage, depthFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, usedLayerCount);
    }
    else {
        memManager.transitionImageLayout(depthLayeredImage, depthFormat, layeredLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, usedLayerCount);
    }
    
    memManager.copyImageToBuffer(layeredImage, colorFormat, stagingBuffer, layeredExtent.width, layeredExtent.height, usedLayerCount);
    memManager.saveImage(stagingBufferMemory, colorFormat, SaveImageFormat::PNG, "../debug/Color" + filename + "Cam", layeredExtent.width, layeredExtent.height, usedLayerCount);

    memManager.copyImageToBuffer(depthLayeredImage, depthFormat, stagingBuffer, layeredExtent.width, layeredExtent.height, usedLayerCount);
    memManager.saveImage(stagingBufferMemory, depthFormat, depthSaveFormat, "../debug/Depth" + filename + "Cam", layeredExtent.width, layeredExtent.height, usedLayerCount);

    memManager.transitionImageLayout(layeredImage, colorFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layeredLayout, usedLayerCount);
    if (layeredLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        memManager.transitionImageLayout(depthLayeredImage, depthFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, usedLayerCount);
    }
    else {
        memManager.transitionImageLayout(depthLayeredImage, depthFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layeredLayout, usedLayerCount);
    }
    
    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}

void OfflineResources::transferLayered(VkImageLayout newLayout, VkFormat colorFormat, VkFormat depthFormat, VkCommandBuffer commandBuffer) {
    if (newLayout == layeredLayout || layeredLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
        return;
    }

    if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        memManager.transitionImageLayout(layeredImage, colorFormat, layeredLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, allocatedLayerCount, commandBuffer);
        memManager.transitionImageLayout(depthLayeredImage, depthFormat, layeredLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, allocatedLayerCount, commandBuffer);
    }
    else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        memManager.transitionImageLayout(layeredImage, colorFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, newLayout, allocatedLayerCount, commandBuffer);
        memManager.transitionImageLayout(depthLayeredImage, depthFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, newLayout, allocatedLayerCount, commandBuffer);
    }
    else {
        throw std::runtime_error("Transfering image to unknown layout!");
    }

    layeredLayout = newLayout;
}


NovelResources::NovelResources(VkDevice device, MemoryManager& memMan, VkRenderPass renderPass, VkExtent2D extent, const AttachementsFormats& formats,
                               const std::vector<VkImageView>& swapChainImageViews)
    : deviceHandle(device),
      memManager(memMan),
      renderpassHandle(renderPass),
      imageExtent(extent) {
    depthFormat = formats.depthFormat;
    metadataFormat = VK_FORMAT_R32G32_UINT;

    imageCount = swapChainImageViews.size();

    createFrameBuffers(swapChainImageViews);
}

NovelResources::~NovelResources() {
    destroyFrameBuffers();
}

void NovelResources::destroyFrameBuffers() {
    for (uint32_t i = 0; i < novelFramebuffers.size(); i++) {
        vkDestroyFramebuffer(deviceHandle, novelFramebuffers[i], nullptr);

        vkDestroyImageView(deviceHandle, depthImageView[i], nullptr);
        memManager.destroyImage(depthImage[i], depthImageMemory[i]);
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) { 
        vkDestroyImageView(deviceHandle, metadataImageView[i], nullptr);
        memManager.destroyImage(metadataImage[i], metadataImageMemory[i]);
    }
}

void NovelResources::updateExtent(VkExtent2D newExtent) {
    imageExtent = newExtent;
    
}

void NovelResources::recreateFrameBuffers(const std::vector<VkImageView>& swapChainImageViews) {
    destroyFrameBuffers();
    createFrameBuffers(swapChainImageViews);
}

void NovelResources::createFrameBuffers(const std::vector<VkImageView>& swapChainImageViews) {
    createBaseImages();

    novelFramebuffers.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; i++) {
        std::array<VkImageView, 2> attachements = {
            swapChainImageViews[i],
            depthImageView[i]
        };

        VkFramebufferCreateInfo framebufferCI{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderpassHandle,
            .attachmentCount = static_cast<uint32_t>(attachements.size()),
            .pAttachments = attachements.data(),
            .width = imageExtent.width,
            .height = imageExtent.height,
            .layers = 1
        };

        if (vkCreateFramebuffer(deviceHandle, &framebufferCI, nullptr, &novelFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create frambuffers!");
        }
    }
}

void NovelResources::createBaseImages() {
    depthImage.resize(imageCount);
    depthImageMemory.resize(imageCount);
    depthImageView.resize(imageCount);

    metadataImage.resize(imageCount);
    metadataImageMemory.resize(imageCount);
    metadataImageView.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; i++) {        
        memManager.createImage(imageExtent.width,
                                imageExtent.height,
                                depthFormat,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                depthImage[i],
                                VMA_MEMORY_USAGE_GPU_ONLY,
                                depthImageMemory[i]);
        depthImageView[i] = createImageView(depthImage[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, deviceHandle);
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {        
        memManager.createImage(imageExtent.width,
                                imageExtent.height,
                                metadataFormat,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                metadataImage[i],
                                VMA_MEMORY_USAGE_GPU_ONLY,
                                metadataImageMemory[i]);
        metadataImageView[i] = createImageView(metadataImage[i], metadataFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);

        memManager.transitionImageLayout(metadataImage[i], metadataFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
}