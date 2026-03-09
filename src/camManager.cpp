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
                               const VkPhysicalDeviceProperties& prop)
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
      novelRes(device, memMan, renderpass, swapChainExtent, formats) {

    camArray.reserve(MAX_CAM_COUNT);
    for (int i = 0; i < camCount; i++) {
        camArray.emplace_back(extentRatio, deviceHandle, memMan, swapChainExtentHandle, colorFormat, depthFormat, renderpassHandle);
    }

    activeCam = &novelView;
}

CamerasManager::~CamerasManager() {

}

void CamerasManager::updateResize(VkExtent2D swapChainExtent) {
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
}

void CamerasManager::updateNovel() {
    novelRes.recreateFrameBuffers();
}

uint32_t CamerasManager::getCamCount() const {
    return camCount;
}

uint32_t CamerasManager::getActiveIndex() const {
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




OfflineResources::OfflineResources(VkDevice device, const VkPhysicalDeviceProperties& prop, MemoryManager& memMan, VkFormat colorFormat, VkFormat depthFormat, VkExtent2D swapChainExtent)
    : imageSampler(device, prop, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE),
      depthSampler(device, prop, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE),
      deviceHandle(device),
      memManager(memMan),
      usedLayerCount(START_CAM_COUNT) {
    updateLayered(swapChainExtent);
    createLayeredImage(colorFormat, depthFormat, true);     // create dummy
}

OfflineResources::~OfflineResources() {
    destroyLayeredImage();
}

void OfflineResources::setLayerCount(u_int32_t count) {
    usedLayerCount = count;
    if (usedLayerCount > allocatedLayerCount) {
        imagesInvalid = true;           // need to allocate more layeres
    }
}

VkImageView OfflineResources::getImageView(ImageViewType type) {
    if (type == ImageViewType::COLOR) {
        return layeredImageView;
    }
    else if (type == ImageViewType::DEPTH) {
        return depthLayeredImageView;
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


NovelResources::NovelResources(VkDevice device, MemoryManager& memMan, VkRenderPass renderPass, VkExtent2D extent, const AttachementsFormats& formats)
    : deviceHandle(device),
      memManager(memMan),
      renderpassHandle(renderPass),
      imageExtent(extent) {
    colorFormat = formats.colorImageFormat;
    depthFormat = formats.depthFormat;
    metadataFormat = VK_FORMAT_R32G32_UINT;

    createFrameBuffers();
}

NovelResources::~NovelResources() {
    destroyFrameBuffers();
}

void NovelResources::destroyFrameBuffers() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyFramebuffer(deviceHandle, novelFramebuffers[i], nullptr);

        vkDestroyImageView(deviceHandle, colorImageView[i], nullptr);
        vkDestroyImageView(deviceHandle, depthImageView[i], nullptr);
        vkDestroyImageView(deviceHandle, metadataImageView[i], nullptr);

        memManager.destroyImage(colorImage[i], colorImageMemory[i]);
        memManager.destroyImage(depthImage[i], depthImageMemory[i]);
        memManager.destroyImage(metadataImage[i], metadataImageMemory[i]);
    }
}

void NovelResources::updateExtent(VkExtent2D newExtent) {
    imageExtent = newExtent;
    extentChanged = true;
}

void NovelResources::recreateFrameBuffers() {
    if (!extentChanged) {
        return;
    }

    destroyFrameBuffers();
    createFrameBuffers();
    extentChanged = false;
}

void NovelResources::createFrameBuffers() {
    createBaseImages();
    
    novelFramebuffers.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::array<VkImageView, 2> attachements = {
            colorImageView[i],
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
    colorImage.resize(MAX_FRAMES_IN_FLIGHT);
    colorImageMemory.resize(MAX_FRAMES_IN_FLIGHT);
    colorImageView.resize(MAX_FRAMES_IN_FLIGHT);

    depthImage.resize(MAX_FRAMES_IN_FLIGHT);
    depthImageMemory.resize(MAX_FRAMES_IN_FLIGHT);
    depthImageView.resize(MAX_FRAMES_IN_FLIGHT);

    metadataImage.resize(MAX_FRAMES_IN_FLIGHT);
    metadataImageMemory.resize(MAX_FRAMES_IN_FLIGHT);
    metadataImageView.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManager.createImage(imageExtent.width,
                                imageExtent.height,
                                colorFormat,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                colorImage[i],
                                VMA_MEMORY_USAGE_GPU_ONLY,
                                colorImageMemory[i]);
        colorImageView[i] = createImageView(colorImage[i], colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);
        
        memManager.createImage(imageExtent.width,
                                imageExtent.height,
                                depthFormat,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                depthImage[i],
                                VMA_MEMORY_USAGE_GPU_ONLY,
                                depthImageMemory[i]);
        depthImageView[i] = createImageView(depthImage[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, deviceHandle);

        memManager.createImage(imageExtent.width,
                                imageExtent.height,
                                metadataFormat,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                metadataImage[i],
                                VMA_MEMORY_USAGE_GPU_ONLY,
                                metadataImageMemory[i]);
        metadataImageView[i] = createImageView(metadataImage[i], metadataFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);
    }
}