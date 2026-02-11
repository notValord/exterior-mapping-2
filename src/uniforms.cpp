#include <uniforms.hpp>
#include <memManager.hpp>
#include <camManager.hpp>
#include <vertex.hpp>

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

void RenderUniforms::updateUniformBuffers(uint32_t currentImage, const Camera& cam, bool showDepth) {
    // static auto startTime = std::chrono::high_resolution_clock::now();

    // auto currentTime = std::chrono::high_resolution_clock::now();
    // float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    MVPBufferObject ubo{};
    // ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    ubo.model = glm::rotate(ubo.model, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = cam.getViewMatrix();
    ubo.proj = cam.getProjectionMatrix();

    RenderFragmentObject rfo{};
    if (showDepth) {
        rfo.depth = 1;
    }
    else {
        rfo.depth = 0;
    }

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

    if (camManager.novelViewToggeled() || camManager.observerToggeled()){
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

void NovelUniforms::updateUniformBuffers(uint32_t currentImage, CamerasManager& camManager, const VkExtent2D& extent, DebugCompute debugFlag) {
    NovelBufferObject nbo {
        .invViewMat = glm::inverse(camManager.novelView.getViewMatrix()),
        .invProjMat = glm::inverse(camManager.novelView.getProjectionMatrix()),
        .res = glm::vec2(extent.width, extent.height),      // not aligned with the size of the buffer
        .camCnt = camManager.getCamCount(),
        .sampleCount = camManager.sampleCount,
        .debugFlag = debugFlag
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
    updateStorageBuffers(camManager);
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


    for (uint i = 0; i < camCount; i++) {
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

UniformManager::UniformManager(MemoryManager& memManager, const VkExtent2D& extentSize, const uint32_t camCount)
        : renderUniforms(memManager),
          offlineUniforms(memManager),
          novelUnifroms(memManager, camCount, extentSize),
          pointCloudUniforms(memManager) {

}

// Uniforms::Uniforms(const VkDevice device, MemoryManager& memManager, const VkExtent2D& extentSize, const uint32_t camCount)
//     : deviceHandle(device), memManager(memManager), maxScreenRes(extentSize) {
//     createRenderUniformBuffers();
//     createOfflineBuffer();
//     createComputeBuffer(camCount);
// }

// Uniforms::~Uniforms() {
//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         memManager.destroyBuffer(renderUniformBuffers[i], renderUniformBuffersMemory[i]);
//         memManager.destroyBuffer(renderFragmentBuffers[i], renderFragmentBuffersMemory[i]);
//         memManager.destroyBuffer(offlineRenderBuffers[i], offlineRenderBuffersMemory[i]);
//         memManager.destroyBuffer(novelUniformBuffers[i], novelUniformBuffersMemory[i]);
//         memManager.destroyBuffer(intersectionsSSBOOut[i], intersectionsSSBOMemory[i]);
//         memManager.destroyBuffer(vertexCountSSBOOut[i], vertexCountSSBOMemory[i]);
//         memManager.destroyBuffer(camArraySSBOIn[i], camArraySSBOMemory[i]);
//     }
// }

// void Uniforms::createRenderUniformBuffers() {
//     VkDeviceSize bufferSize = sizeof(UniformBufferObject);

//     renderUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
//     renderUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
//     renderUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         VmaAllocationInfo allocInfo{};
//         allocInfo = memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, renderUniformBuffers[i],
//                 VMA_MEMORY_USAGE_CPU_TO_GPU, renderUniformBuffersMemory[i], nullptr, true);

//         renderUniformBuffersMapped[i] = allocInfo.pMappedData;
//     }

//     bufferSize = sizeof(RenderFragmentObject);

//     renderFragmentBuffers.resize(MAX_FRAMES_IN_FLIGHT);
//     renderFragmentBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
//     renderFragmentBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         VmaAllocationInfo allocInfo{};
//         allocInfo = memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, renderFragmentBuffers[i],
//                 VMA_MEMORY_USAGE_CPU_TO_GPU, renderFragmentBuffersMemory[i], nullptr, true);

//         renderFragmentBuffersMapped[i] = allocInfo.pMappedData;
//     }
// }

// void Uniforms::createOfflineBuffer() {
//     VkDeviceSize bufferSize = sizeof(OfflineRenderBuffer);

//     offlineRenderBuffers.resize(MAX_FRAMES_IN_FLIGHT);
//     offlineRenderBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
//     offlineRenderBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

//     for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         VmaAllocationInfo allocInfo{};
//         allocInfo = memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, offlineRenderBuffers[i], 
//                     VMA_MEMORY_USAGE_CPU_TO_GPU, offlineRenderBuffersMemory[i], nullptr, true);

//         offlineRenderBuffersMapped[i] = allocInfo.pMappedData; 
//     }
// }

// void Uniforms::createComputeBuffer(const uint32_t camCount) {
//     // NOVEL UNIFORM BUFFERS creationg
//     VkDeviceSize bufferSize = sizeof(NovelImageObject);

//     novelUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
//     novelUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
//     novelUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         VmaAllocationInfo allocInfo{};
//         allocInfo = memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, novelUniformBuffers[i],
//                 VMA_MEMORY_USAGE_CPU_TO_GPU, novelUniformBuffersMemory[i], nullptr, true);

//         novelUniformBuffersMapped[i] = allocInfo.pMappedData;
//     }

//     // CAM ARRAY SSBO creation
//     bufferSize = sizeof(CamArrayData);

//     camArraySSBOIn.resize(MAX_FRAMES_IN_FLIGHT);
//     camArraySSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
//     camArrayCounts.resize(MAX_FRAMES_IN_FLIGHT);

//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         camArrayCounts[i] = ((camCount / 10) + 1) * 10;
//         memManager.createBuffer(bufferSize * camArrayCounts[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, camArraySSBOIn[i],
//                 VMA_MEMORY_USAGE_GPU_ONLY, camArraySSBOMemory[i]);
//     }

    
//     // INTERSECTIONS SSBO creation
//     intersectionsSSBOOut.resize(MAX_FRAMES_IN_FLIGHT);
//     intersectionsSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);

//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//             memManager.createBuffer(sizeof(glm::vec3) * 2 * maxScreenRes.width * maxScreenRes.height * camArrayCounts[i], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, intersectionsSSBOOut[i],
//                 VMA_MEMORY_USAGE_GPU_ONLY, intersectionsSSBOMemory[i]);
//     }

//     // VERTEX COUNTS SSBO creationg
//     vertexCountSSBOOut.resize(MAX_FRAMES_IN_FLIGHT);
//     vertexCountSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
//     vertexCountSSBOMapped.resize(MAX_FRAMES_IN_FLIGHT);

//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         VmaAllocationInfo allocInfo{};
//         allocInfo = memManager.createBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, vertexCountSSBOOut[i],
//                 VMA_MEMORY_USAGE_GPU_TO_CPU, vertexCountSSBOMemory[i], nullptr, true);
//         vertexCountSSBOMapped[i] = allocInfo.pMappedData;
//     }   
// }

// void Uniforms::updateRenderUniformBuffers(uint32_t currentImage, const Camera& cam, bool showDepth) {
//     static auto startTime = std::chrono::high_resolution_clock::now();

//     auto currentTime = std::chrono::high_resolution_clock::now();
//     float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

//     UniformBufferObject ubo{};
//     // ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
//     ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
//     ubo.model = glm::rotate(ubo.model, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
//     ubo.view = cam.getViewMatrix();
//     ubo.proj = cam.getProjectionMatrix();

//     RenderFragmentObject rfo{};
//     if (showDepth) {
//         rfo.depth = 1;
//     }
//     else {
//         rfo.depth = 0;
//     }

//     memcpy(renderUniformBuffersMapped[currentImage], &ubo, sizeof(UniformBufferObject));
//     memcpy(renderFragmentBuffersMapped[currentImage], &rfo, sizeof(RenderFragmentObject));
// }

// void Uniforms::updateOfflineRenderBuffers(uint32_t currentImage, CamerasManager& camManager, PresentationMode mode, ImageViewType viewType) {
//     OfflineRenderBuffer ubo{};
//     ubo.grid = glm::ivec2(3, ((camManager.getCamCount()-1)/3)+1);
//     ubo.layerCnt = camManager.getCamCount();
//     if (camManager.novelViewToggeled() || camManager.observerToggeled()){
//         ubo.layerID = -1;
//     }
//     else {
//         ubo.layerID = camManager.getActiveIndex();
//     }
//     ubo.presentMode = mode;   // get from input manager or something as argument
//     ubo.presentType = viewType;

//     memcpy(offlineRenderBuffersMapped[currentImage], &ubo, sizeof(OfflineRenderBuffer));
// }

// void Uniforms::updateComputeUniformBuffers(uint32_t currentImage, CamerasManager& camManager, const VkExtent2D& extent, DebugCompute debugFlag) {
//     NovelImageObject novelData {
//         .invViewMat = glm::inverse(camManager.novelView.getViewMatrix()),
//         .invProjMat = glm::inverse(camManager.novelView.getProjectionMatrix()),
//         .res = glm::vec2(extent.width, extent.height),      // not aligned with the size of the buffer
//         .camCnt = camManager.getCamCount(),
//         .sampleCount = camManager.sampleCount,
//         .debugFlag = debugFlag
//     };

//     if (extent.width > maxScreenRes.width || extent.height > maxScreenRes.height) {
//         resGrew = true;
//         maxScreenRes = extent;
//     }

//     memcpy(novelUniformBuffersMapped[currentImage], &novelData, sizeof(NovelImageObject));
// }

// uint32_t Uniforms::getVertexCount(uint32_t currentImage) {
//     uint32_t vertCount = 0;

//     memcpy(&vertCount, vertexCountSSBOMapped[currentImage], sizeof(uint32_t));
//     return vertCount;    
// }

// void Uniforms::recreateIntersectionsSSBO(const uint32_t currentFrame) {
//     memManager.destroyBuffer(intersectionsSSBOOut[currentFrame], intersectionsSSBOMemory[currentFrame]);
//     memManager.createBuffer(sizeof(glm::vec3) * 2 * maxScreenRes.width * maxScreenRes.height * camArrayCounts[currentFrame], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, intersectionsSSBOOut[currentFrame],
//             VMA_MEMORY_USAGE_GPU_ONLY, intersectionsSSBOMemory[currentFrame]);
// }

// bool Uniforms::recreateCamArraySSBO(const uint32_t newCount, const uint32_t currentFrame) {
//     // clear out the unused space if too large or size up the buffer
//     if ((newCount > camArrayCounts[currentFrame]) || (camArrayCounts[currentFrame] - newCount >= 15)) {
//         camArrayCounts[currentFrame] = ((newCount / 10) + 1) * 10;

//         memManager.destroyBuffer(camArraySSBOIn[currentFrame], camArraySSBOMemory[currentFrame]);
//         memManager.createBuffer(sizeof(CamArrayData) * camArrayCounts[currentFrame], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, camArraySSBOIn[currentFrame],
//                 VMA_MEMORY_USAGE_GPU_ONLY, camArraySSBOMemory[currentFrame]);

//         recreateIntersectionsSSBO(currentFrame);
//         resGrew = false;

//         return true;
//     }

//     return false;
// }

// bool Uniforms::setCamArrayData(uint32_t currentImage, CamerasManager& camManager) {
//     bool updated = false;
//     uint32_t camCount = camManager.getCamCount();
//     std::vector<CamArrayData> camData(camCount);

//     for (uint32_t i = 0; i < camCount; i++) {
//         camData[i] = camManager.camArray[i].getCamData();
//     }

//     // check if the SSO is large enough
//     if (camCount != camArrayCounts[currentImage]) {
//         updated |= recreateCamArraySSBO(camCount, currentImage);
//     }
//     if (resGrew) {  // recreate interseciton if resolution grew up
//         recreateIntersectionsSSBO(currentImage);
//         resGrew = false;
//         updated |= true;
//     }
    
//     VkBuffer stagingBuffer;
//     VmaAllocation stagingBufferMemory;
//     auto allocInfo = memManager.createBuffer(sizeof(CamArrayData) * camData.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBufferMemory, camData.data());
//     memManager.copyBuffer(stagingBuffer, camArraySSBOIn[currentImage], sizeof(CamArrayData) * camData.size());

//     memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);

//     return updated;
// }