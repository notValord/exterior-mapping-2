#include <uniforms.hpp>
#include <memManager.hpp>
#include <camManager.hpp>
#include <vertex.hpp>
#include <inputManager.hpp>
#include <util.hpp>

BaseUniforms::BaseUniforms(MemoryManager& memManager)
        : memManagerRef(memManager) {
    ;
}

BaseUniforms::~BaseUniforms() {
for (uint32_t i = 0; i < uniformBuffers.size(); i++) {
        memManagerRef.destroyBuffer(uniformBuffers[i], uniformBuffersMemory[i]);
    }
}

void BaseUniforms::createUniformBuffers(VkDeviceSize bufferSize) {
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManagerRef.createBuffer(bufferSize,
                                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                               VK_SHARING_MODE_EXCLUSIVE,
                                               uniformBuffers[i],
                                               VMA_MEMORY_USAGE_CPU_TO_GPU,
                                               uniformBuffersMemory[i],
                                               nullptr,
                                               true);

        uniformBuffersMapped[i] = allocInfo.pMappedData;
    }
}



RenderUniforms::RenderUniforms(MemoryManager& memManager)
        : BaseUniforms(memManager) {
    createUniformBuffers(sizeof(MVPBufferObject));
    createFragmentUniformBuffers();
}

RenderUniforms::~RenderUniforms() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.destroyBuffer(fragmentUniformBuffers[i], fragmentUniformBuffersMemory[i]);
    }
}

void RenderUniforms::createFragmentUniformBuffers(VkDeviceSize bufferSize) {
    fragmentUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    fragmentUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    fragmentUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManagerRef.createBuffer(bufferSize,
                                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                               VK_SHARING_MODE_EXCLUSIVE,
                                               fragmentUniformBuffers[i],
                                               VMA_MEMORY_USAGE_CPU_TO_GPU,
                                               fragmentUniformBuffersMemory[i],
                                               nullptr,
                                               true);

        fragmentUniformBuffersMapped[i] = allocInfo.pMappedData;
    }
}

void RenderUniforms::updateUniformBuffers(uint32_t currentImage, const Camera& cam, bool showDepth, const glm::vec3& light, float modelScale) {
    // static auto startTime = std::chrono::high_resolution_clock::now();

    // auto currentTime = std::chrono::high_resolution_clock::now();
    // float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    MVPBufferObject ubo{};
    // ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.model = glm::scale(glm::mat4(1.0f), glm::vec3(modelScale));
    // ubo.model = glm::rotate(ubo.model, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = cam.getViewMatrix();
    ubo.proj = cam.getProjectionMatrix();

    RenderFragmentObject rfo{};
    if (showDepth) {
        rfo.depth = 1;
    }
    else {
        rfo.depth = 0;
    }
    rfo.camPos = cam.getPosition();
    rfo.lightPos = light;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(MVPBufferObject));
    memcpy(fragmentUniformBuffersMapped[currentImage], &rfo, sizeof(RenderFragmentObject));
}



OfflineUniforms::OfflineUniforms(MemoryManager& memManager)
        : BaseUniforms(memManager) {
    createUniformBuffers(sizeof(OfflineBufferObject));
}

void OfflineUniforms::updateUniformBuffers(uint32_t currentImage, CamerasManager& camManager, PresentationMode mode, ImageViewType viewType) {
    OfflineBufferObject ubo{
        .grid = glm::ivec2(3, ((camManager.getCamCount()-1)/3)+1),      // currently set staticly to 3 columns
        .layerCnt = static_cast<int>(camManager.getCamCount()),
        .presentMode = mode,                                            // get from input manager or something as argument
        .presentType = viewType
    };

    if (camManager.novelViewToggled() || camManager.observerToggled()){
        ubo.layerID = -1;
    }
    else {
        ubo.layerID = camManager.getActiveIndex();
    }

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(OfflineBufferObject));
}



NovelUniforms::NovelUniforms(MemoryManager& memManager, const uint32_t camCount, const VkExtent2D& extentSize)
        : BaseUniforms(memManager), maxScreenRes(extentSize) {
    createUniformBuffers(sizeof(NovelBufferObject));
    createStorageBuffers(camCount);
}

NovelUniforms::~NovelUniforms() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.destroyBuffer(camArraySSBOIn[i], camArraySSBOMemory[i]);
        memManagerRef.destroyBuffer(intersectionsSSBOOut[i], intersectionsSSBOMemory[i]);
        memManagerRef.destroyBuffer(intersectCountSSBOOut[i], intersectCountSSBOMemory[i]);
    }
}

void NovelUniforms::createStorageBuffers(const uint32_t camCount) {
    VkDeviceSize bufferSize = sizeof(CamArrayData);             // CAM ARRAY SSBO creation

    camArraySSBOIn.resize(MAX_FRAMES_IN_FLIGHT);
    camArraySSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    camCounts.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        camCounts[i] = ((camCount / 10) + 1) * 10;              // round up the cam count to the closest x10 to preallocate
        memManagerRef.createBuffer(bufferSize * camCounts[i],
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_SHARING_MODE_EXCLUSIVE,
                                   camArraySSBOIn[i],
                                   VMA_MEMORY_USAGE_GPU_ONLY,
                                   camArraySSBOMemory[i]);
    }


    bufferSize = sizeof(glm::vec3) * 2 * maxScreenRes.width * maxScreenRes.height * camCounts[0];   // INTERSECTIONS SSBO creation
    intersectionsSSBOOut.resize(MAX_FRAMES_IN_FLIGHT);
    intersectionsSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            memManagerRef.createBuffer(bufferSize,
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_SHARING_MODE_EXCLUSIVE,
                                       intersectionsSSBOOut[i],
                                       VMA_MEMORY_USAGE_GPU_ONLY,
                                       intersectionsSSBOMemory[i]);
    }

    bufferSize = sizeof(uint32_t);                              // VERTEX COUNTS SSBO creationg
    intersectCountSSBOOut.resize(MAX_FRAMES_IN_FLIGHT);
    intersectCountSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    intersectCountSSBOMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManagerRef.createBuffer(bufferSize,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               VK_SHARING_MODE_EXCLUSIVE,
                                               intersectCountSSBOOut[i],
                                               VMA_MEMORY_USAGE_GPU_TO_CPU,
                                               intersectCountSSBOMemory[i],
                                               nullptr,
                                               true);
        
        intersectCountSSBOMapped[i] = allocInfo.pMappedData;
    }   
}

void NovelUniforms::updateUniformBuffers(uint32_t currentImage, CamerasManager& camManager, const VkExtent2D& extent, const InputManager& input, const DebugCompute debug) {
    NovelBufferObject nbo {
        .viewMat = camManager.novelView.getViewMatrix(),
        .invViewMat = glm::inverse(camManager.novelView.getViewMatrix()),
        .projMat = camManager.novelView.getProjectionMatrix(),
        .invProjMat = glm::inverse(camManager.novelView.getProjectionMatrix()),
        .res = glm::vec2(extent.width, extent.height),      // not aligned with the size of the buffer
        .camCnt = camManager.getCamCount(),
        .sampleCount = camManager.sampleCount,
        .sampleDebug = camManager.sampleDebug,
        .heuristic = input.novelHeuristic,
        .debugFlag = debug,
        .distanceFlag = input.distance,
        .coneFlag = input.coneMarching,
        .pixelRadius = static_cast<float>(2 * tan(camManager.activeCam->getFOV()/2.0) / (extent.height / input.neighbourCount)),
        .inConePercentage = input.inConePercentage
    };

    if (extent.width > maxScreenRes.width || extent.height > maxScreenRes.height) {
        resGrew = true;
        maxScreenRes = extent;
    }

    memcpy(uniformBuffersMapped[currentImage], &nbo, sizeof(NovelBufferObject));
}

uint32_t NovelUniforms::getIntersectCount(uint32_t currentImage) {
    uint32_t vertCount = 0;

    memcpy(&vertCount, intersectCountSSBOMapped[currentImage], sizeof(uint32_t));
    return vertCount;
}

void NovelUniforms::recreateIntersectionsSSBO(const uint32_t currentFrame) {
    memManagerRef.destroyBuffer(intersectionsSSBOOut[currentFrame], intersectionsSSBOMemory[currentFrame]);

    VkDeviceSize bufferSize = sizeof(glm::vec3) * 2 * maxScreenRes.width * maxScreenRes.height * camCounts[currentFrame];
    memManagerRef.createBuffer(bufferSize,
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_SHARING_MODE_EXCLUSIVE,
                               intersectionsSSBOOut[currentFrame],
                               VMA_MEMORY_USAGE_GPU_ONLY,
                               intersectionsSSBOMemory[currentFrame]);
}

bool NovelUniforms::recreateCamArraySSBO(const uint32_t newCount, const uint32_t currentFrame) {
    // clear out the unused space if too large or size up the buffer
    if ((newCount > camCounts[currentFrame]) || (camCounts[currentFrame] - newCount >= 15)) {
        camCounts[currentFrame] = ((newCount / 10) + 1) * 10;

        memManagerRef.destroyBuffer(camArraySSBOIn[currentFrame], camArraySSBOMemory[currentFrame]);
        memManagerRef.createBuffer(sizeof(CamArrayData) * camCounts[currentFrame],
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_SHARING_MODE_EXCLUSIVE,
                                   camArraySSBOIn[currentFrame],
                                   VMA_MEMORY_USAGE_GPU_ONLY,
                                   camArraySSBOMemory[currentFrame]);

        recreateIntersectionsSSBO(currentFrame);
        resGrew = false;

        return true;
    }

    return false;
}

bool NovelUniforms::setCamArrayData(uint32_t currentImage, CamerasManager& camManager) {
    bool updated = false;
    uint32_t camCount = camManager.getCamCount();
    std::vector<CamArrayData> camData(camCount);

    for (uint32_t i = 0; i < camCount; i++) {
        camData[i] = camManager.camArray[i].getCamData();
    }

    // check if the SSO is large enough
    if (camCount != camCounts[currentImage]) {
        updated |= recreateCamArraySSBO(camCount, currentImage);
    }
    if (resGrew) {  // recreate interseciton if resolution grew up
        recreateIntersectionsSSBO(currentImage);
        resGrew = false;
        updated |= true;
    }
    
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    auto allocInfo = memManagerRef.createBuffer(sizeof(CamArrayData) * camData.size(),
                                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                VK_SHARING_MODE_EXCLUSIVE,
                                                stagingBuffer,
                                                VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                stagingBufferMemory,
                                                camData.data());

    memManagerRef.copyBuffer(stagingBuffer, camArraySSBOIn[currentImage], sizeof(CamArrayData) * camData.size());
    memManagerRef.destroyBuffer(stagingBuffer, stagingBufferMemory);

    return updated;
}



PointCloudUniforms::PointCloudUniforms(MemoryManager& memManager) 
        : BaseUniforms(memManager), currScreenRes({1, 1}) {
    createUniformBuffers(sizeof(PointCloudObject));
    createStorageBuffers(1);        // create dummy resource to bind
}

PointCloudUniforms::~PointCloudUniforms() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.destroyBuffer(camMatricesSSBOIn[i], camMatricesSSBOMemory[i]);
        memManagerRef.destroyBuffer(pointCloudSSBOOut[i], pointCloudSSBOMemory[i]);
        memManagerRef.destroyBuffer(pointCountSSBOOut[i], pointCountSSBOMemory[i]);
    }
}

void PointCloudUniforms::createStorageBuffers(const uint32_t camCount) {
    VkDeviceSize bufferSize = sizeof(CamArrayInvMatrices) * camCount;

    camMatricesSSBOIn.resize(MAX_FRAMES_IN_FLIGHT);
    camMatricesSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.createBuffer(bufferSize,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_SHARING_MODE_EXCLUSIVE,
                                   camMatricesSSBOIn[i],
                                   VMA_MEMORY_USAGE_GPU_ONLY,
                                   camMatricesSSBOMemory[i]);
    }

    bufferSize = sizeof(CloudPoint) * currScreenRes.width * currScreenRes.height * camCount;
    pointCloudSSBOOut.resize(MAX_FRAMES_IN_FLIGHT);
    pointCloudSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            memManagerRef.createBuffer(bufferSize,
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_SHARING_MODE_EXCLUSIVE,
                                       pointCloudSSBOOut[i],
                                       VMA_MEMORY_USAGE_GPU_ONLY,
                                       pointCloudSSBOMemory[i]);
    }

    bufferSize = sizeof(uint32_t);
    pointCountSSBOOut.resize(MAX_FRAMES_IN_FLIGHT);
    pointCountSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    pointCountSSBOMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManagerRef.createBuffer(bufferSize,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE,
                                               pointCountSSBOOut[i],
                                               VMA_MEMORY_USAGE_GPU_TO_CPU,
                                               pointCountSSBOMemory[i],
                                               nullptr,
                                               true);
        
        pointCountSSBOMapped[i] = allocInfo.pMappedData;
    } 
}

void PointCloudUniforms::setScreenRes(const VkExtent2D& newScreenRes) {
    currScreenRes = newScreenRes;
}

void PointCloudUniforms::updateUniformBuffers(CamerasManager& camManager) {
    uint32_t camCount = camManager.getCamCount();
    PointCloudObject pco {                                              // update only once, cannot change while running
        .res = glm::vec2(currScreenRes.width, currScreenRes.height),    // offline image resolution
        .camCnt = camCount,                                             // same as the offline image count
    };
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memcpy(uniformBuffersMapped[i], &pco, sizeof(PointCloudObject));
    }

    recreateStorageBuffers(camCount);
}

void PointCloudUniforms::recreateStorageBuffers(const uint32_t camCount) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.destroyBuffer(camMatricesSSBOIn[i], camMatricesSSBOMemory[i]);
        memManagerRef.destroyBuffer(pointCloudSSBOOut[i], pointCloudSSBOMemory[i]);
    }

    VkDeviceSize bufferSize = sizeof(CamArrayInvMatrices) * camCount;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.createBuffer(bufferSize,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_SHARING_MODE_EXCLUSIVE,
                                   camMatricesSSBOIn[i],
                                   VMA_MEMORY_USAGE_GPU_ONLY,
                                   camMatricesSSBOMemory[i]);
    }

    bufferSize = sizeof(CloudPoint) * currScreenRes.width * currScreenRes.height * camCount;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            memManagerRef.createBuffer(bufferSize,
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_SHARING_MODE_EXCLUSIVE,
                                       pointCloudSSBOOut[i],
                                       VMA_MEMORY_USAGE_GPU_ONLY,
                                       pointCloudSSBOMemory[i]);
            }
}

void PointCloudUniforms::updateStorageBuffers(CamerasManager& camManager) {
    uint32_t camCount = camManager.getCamCount();
    std::vector<CamArrayInvMatrices> matricesData(camCount);


    for (uint32_t i = 0; i < camCount; i++) {
        CamArrayInvMatrices matrices {
            .invViewMat = glm::inverse(camManager.camArray[i].getViewMatrix()),
            .invProjMat = glm::inverse(camManager.camArray[i].getProjectionMatrix()),
        };

        matricesData[i] = matrices;
    }
    
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    VkDeviceSize bufferSize = sizeof(CamArrayInvMatrices) * matricesData.size();

    auto allocInfo = memManagerRef.createBuffer(bufferSize,
                                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                VK_SHARING_MODE_EXCLUSIVE,
                                                stagingBuffer,
                                                VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                stagingBufferMemory,
                                                matricesData.data());

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.copyBuffer(stagingBuffer, camMatricesSSBOIn[i], bufferSize);
    }
    memManagerRef.destroyBuffer(stagingBuffer, stagingBufferMemory);
}

uint32_t PointCloudUniforms::getPointCount(uint32_t currentImage) {
    uint32_t pointCount = 0;

    memcpy(&pointCount, pointCountSSBOMapped[currentImage], sizeof(uint32_t));
    return pointCount;
}



RayDataUniforms::RayDataUniforms(MemoryManager& memManager, VkExtent2D screenRes)
    : BaseUniforms(memManager) {
    maxScreenRes.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        maxScreenRes[i] = screenRes;
    }
    createUniformBuffers(sizeof(RayDataObject));
    createRayDataBuffers();
}

RayDataUniforms::~RayDataUniforms() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        destroyDataBuffers(i);
        memManagerRef.destroyBuffer(rayCountSSBOIn[i], rayCountSSBOMemory[i]);
    }
}

bool RayDataUniforms::updateUniformBuffers(uint32_t currentFrame, Camera& novelCam, VkExtent2D screenRes) {
    RayDataObject rdo {
        .invViewMat = glm::inverse(novelCam.getViewMatrix()),
        .invProjMat = glm::inverse(novelCam.getProjectionMatrix()),
        .res = glm::uvec2(screenRes.width, screenRes.height)
    };
    
    memcpy(uniformBuffersMapped[currentFrame], &rdo, sizeof(RayDataObject));

    if (screenRes.width * screenRes.height > maxScreenRes[currentFrame].width * maxScreenRes[currentFrame].height) {
        maxScreenRes[currentFrame] = screenRes;
        recreateRayDataBuffers(currentFrame);

        return true;
    }

    return false;
}

void RayDataUniforms::createRayDataBuffers() {
    rayDataSSBOIn.resize(MAX_FRAMES_IN_FLIGHT);
    rayDataSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    VkDeviceSize bufferSize = maxScreenRes[0].width * maxScreenRes[0].height * sizeof(RayData);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManagerRef.createBuffer(bufferSize,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               VK_SHARING_MODE_EXCLUSIVE,
                                               rayDataSSBOIn[i],
                                               VMA_MEMORY_USAGE_GPU_ONLY,
                                               rayDataSSBOMemory[i]);
    }

    rayCountSSBOIn.resize(MAX_FRAMES_IN_FLIGHT);
    rayCountSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManagerRef.createBuffer(sizeof(uint32_t),
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               VK_SHARING_MODE_EXCLUSIVE,
                                               rayCountSSBOIn[i],
                                               VMA_MEMORY_USAGE_GPU_ONLY,
                                               rayCountSSBOMemory[i]);
    }
}

void RayDataUniforms::destroyDataBuffers(uint32_t currentFrame) {
    memManagerRef.destroyBuffer(rayDataSSBOIn[currentFrame], rayDataSSBOMemory[currentFrame]);
}

void RayDataUniforms::recreateRayDataBuffers(uint32_t currentFrame) {
    destroyDataBuffers(currentFrame);

    VkDeviceSize bufferSize = maxScreenRes[currentFrame].width * maxScreenRes[currentFrame].height * sizeof(RayData);
    
    VmaAllocationInfo allocInfo{};
    allocInfo = memManagerRef.createBuffer(bufferSize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            VK_SHARING_MODE_EXCLUSIVE,
                                            rayDataSSBOIn[currentFrame],
                                            VMA_MEMORY_USAGE_GPU_ONLY,
                                            rayDataSSBOMemory[currentFrame]);
}



NovelSynthUniforms::NovelSynthUniforms(MemoryManager& memManager, VkDevice device, const uint32_t camCount, VkExtent2D screenRes)
    : BaseUniforms(memManager), deviceHandle(device), layerCount(camCount) {
    currScreenRes.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        currScreenRes[i] = screenRes;
    }
    createDebugImages();
    createDataImages();
}

NovelSynthUniforms::~NovelSynthUniforms() {
    destroyDebugImages();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        destroyDataImages(i);
    }
}

void NovelSynthUniforms::setCamCount(const uint32_t camCount) {
    if (camCount > layerCount) {
        layerCount = camCount;
        recreateAllImages();
    }
}

bool NovelSynthUniforms::setResolution(VkExtent2D screenRes, uint32_t currentFrame) {
    if (currScreenRes[currentFrame].width != screenRes.width || currScreenRes[currentFrame].height != screenRes.height) {
        currScreenRes[currentFrame] = screenRes;
        recreateDataImages(currentFrame);

        return true;
    }
    return false;
}

std::vector<VkImage> NovelSynthUniforms::getResultImages(){
    return resultImage;
}

void NovelSynthUniforms::createDebugImages() {
    debugImage.resize(MAX_FRAMES_IN_FLIGHT);
    debugImageMemory.resize(MAX_FRAMES_IN_FLIGHT);
    debugImageView.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.createImage(1024,
                                  1024,
                                  VK_FORMAT_R32_SFLOAT,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_STORAGE_BIT,
                                  debugImage[i],
                                  VMA_MEMORY_USAGE_GPU_ONLY,
                                  debugImageMemory[i],
                                  layerCount);
        debugImageView[i] = createImageView(debugImage[i], VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, layerCount);
        memManagerRef.transitionImageLayout(debugImage[i], VK_FORMAT_R32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, layerCount);
    }
}

void NovelSynthUniforms::destroyDebugImages() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.destroyImage(debugImage[i], debugImageMemory[i]);
        vkDestroyImageView(deviceHandle, debugImageView[i], nullptr);
    }
}

void NovelSynthUniforms::recreateDebugImages() {
    destroyDebugImages();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.createImage(1024,
                                  1024,
                                  VK_FORMAT_R32_SFLOAT,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_STORAGE_BIT,
                                  debugImage[i],
                                  VMA_MEMORY_USAGE_GPU_ONLY,
                                  debugImageMemory[i],
                                  layerCount);
        debugImageView[i] = createImageView(debugImage[i], VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, layerCount);
        memManagerRef.transitionImageLayout(debugImage[i], VK_FORMAT_R32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, layerCount);
    }
}

void NovelSynthUniforms::createDataImages() {
    resultImage.resize(MAX_FRAMES_IN_FLIGHT);
    resultImageMemory.resize(MAX_FRAMES_IN_FLIGHT);
    resultImageView.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.createImage(currScreenRes[i].width,
                                  currScreenRes[i].height,
                                  VK_FORMAT_R32G32_SFLOAT,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_STORAGE_BIT,
                                  resultImage[i],
                                  VMA_MEMORY_USAGE_GPU_ONLY,
                                  resultImageMemory[i],
                                  layerCount);
        resultImageView[i] = createImageView(resultImage[i], VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, layerCount);
        memManagerRef.transitionImageLayout(resultImage[i], VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, layerCount);
    }
}

void NovelSynthUniforms::destroyDataImages(uint32_t currentFrame) {
    memManagerRef.destroyImage(resultImage[currentFrame], resultImageMemory[currentFrame]);
    vkDestroyImageView(deviceHandle, resultImageView[currentFrame], nullptr);
}

void NovelSynthUniforms::recreateDataImages(uint32_t currentFrame) {
    destroyDataImages(currentFrame);

    memManagerRef.createImage(currScreenRes[currentFrame].width,
                              currScreenRes[currentFrame].height,
                              VK_FORMAT_R32G32_SFLOAT,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_STORAGE_BIT,
                              resultImage[currentFrame],
                              VMA_MEMORY_USAGE_GPU_ONLY,
                              resultImageMemory[currentFrame],
                              layerCount);
    resultImageView[currentFrame] = createImageView(resultImage[currentFrame], VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, layerCount);
    memManagerRef.transitionImageLayout(resultImage[currentFrame], VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, layerCount);
}

void NovelSynthUniforms::recreateAllImages() {
    recreateDebugImages();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        recreateDataImages(i);
    }
}



NovelReconUniforms::NovelReconUniforms(MemoryManager& memManager, VkDevice device, VkExtent2D screenRes, const VkPhysicalDeviceProperties& prop)
    :BaseUniforms(memManager), deviceHandle(device), colorSampler(device, prop) {
    currScreenRes.resize(MAX_FRAMES_IN_FLIGHT);
    resultLayout.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        currScreenRes[i] = screenRes;
        resultLayout[i] = VK_IMAGE_LAYOUT_GENERAL;
    }

    createUniformBuffers(sizeof(ReconDataObject));
    createImages();
}

NovelReconUniforms::~NovelReconUniforms() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        destroyImages(i);
    }
}

void NovelReconUniforms::destroyImages(uint32_t currentFrame) {
    memManagerRef.destroyImage(resultImage[currentFrame], resultImageMemory[currentFrame]);
    vkDestroyImageView(deviceHandle, resultImageView[currentFrame], nullptr);
}

bool NovelReconUniforms::setResolution(VkExtent2D screenRes, uint32_t currentFrame) {
    if (currScreenRes[currentFrame].width != screenRes.width || currScreenRes[currentFrame].height != screenRes.height) {
        currScreenRes[currentFrame] = screenRes;
        recreateImages(currentFrame);

        return true;
    }
    return false;
}

void NovelReconUniforms::createImages() {
    resultImage.resize(MAX_FRAMES_IN_FLIGHT);
    resultImageMemory.resize(MAX_FRAMES_IN_FLIGHT);
    resultImageView.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManagerRef.createImage(currScreenRes[i].width,
                                  currScreenRes[i].height,
                                  VK_FORMAT_R32G32B32A32_SFLOAT,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                  resultImage[i],
                                  VMA_MEMORY_USAGE_GPU_ONLY,
                                  resultImageMemory[i]);
        resultImageView[i] = createImageView(resultImage[i], VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);
        memManagerRef.transitionImageLayout(resultImage[i], VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
}

void NovelReconUniforms::updateUniformBuffers(uint32_t currentFrame, uint32_t layerCount, VkExtent2D novelRes) {
    ReconDataObject rdo {
        .novelRes = glm::uvec2(novelRes.width, novelRes.height),
        .layerCount = layerCount
    };
    
    memcpy(uniformBuffersMapped[currentFrame], &rdo, sizeof(ReconDataObject));
}

void NovelReconUniforms::recreateImages(uint32_t currentFrame) {
    destroyImages(currentFrame);

    memManagerRef.createImage(currScreenRes[currentFrame].width,
                              currScreenRes[currentFrame].height,
                              VK_FORMAT_R32G32B32A32_SFLOAT,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              resultImage[currentFrame],
                              VMA_MEMORY_USAGE_GPU_ONLY,
                              resultImageMemory[currentFrame]);
    resultImageView[currentFrame] = createImageView(resultImage[currentFrame], VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);
    memManagerRef.transitionImageLayout(resultImage[currentFrame], VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    resultLayout[currentFrame] = VK_IMAGE_LAYOUT_GENERAL;
}

void NovelReconUniforms::transferResultLayout(uint32_t currentFrame, VkImageLayout newLayout, VkCommandBuffer commandBuffer) {
    if (newLayout == resultLayout[currentFrame]) {
        return;
    }

    if (newLayout != VK_IMAGE_LAYOUT_GENERAL && newLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        throw std::runtime_error("Wrong image layout!");
    }

    memManagerRef.transitionImageLayout(resultImage[currentFrame], VK_FORMAT_R32G32B32A32_SFLOAT, resultLayout[currentFrame], newLayout, 1 ,commandBuffer);
    resultLayout[currentFrame] = newLayout;
}



UniformManager::UniformManager(MemoryManager& memManager, VkDevice device, const VkExtent2D& extentSize, const uint32_t camCount, const VkPhysicalDeviceProperties& prop)
        : renderUniforms(memManager),
          offlineUniforms(memManager),
          novelUniforms(memManager, camCount, extentSize),
          pointCloudUniforms(memManager),
          rayDataUniforms(memManager, extentSize),
          novelSynthUniforms(memManager, device, camCount, extentSize),
          novelReconUniforms(memManager, device, extentSize, prop) {
    ;
}