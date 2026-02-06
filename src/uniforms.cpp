#include <uniforms.hpp>
#include <memManager.hpp>
#include <camManager.hpp>
#include <vertex.hpp>

Uniforms::Uniforms(const VkDevice device, MemoryManager& memManager, const VkExtent2D& extentSize, const uint32_t camCount)
    : deviceHandle(device), memManager(memManager), maxScreenRes(extentSize) {
    createRenderUniformBuffers();
    createOfflineBuffer();
    createComputeBuffer(camCount);
}

Uniforms::~Uniforms() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManager.destroyBuffer(renderUniformBuffers[i], renderUniformBuffersMemory[i]);
        memManager.destroyBuffer(offlineRenderBuffers[i], offlineRenderBuffersMemory[i]);
        memManager.destroyBuffer(novelUniformBuffers[i], novelUniformBuffersMemory[i]);
        memManager.destroyBuffer(intersectionsSSBOOut[i], intersectionsSSBOMemory[i]);
        memManager.destroyBuffer(vertexCountSSBOOut[i], vertexCountSSBOMemory[i]);
        memManager.destroyBuffer(camArraySSBOIn[i], camArraySSBOMemory[i]);
    }
}

void Uniforms::createRenderUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    renderUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    renderUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    renderUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, renderUniformBuffers[i],
                VMA_MEMORY_USAGE_CPU_TO_GPU, renderUniformBuffersMemory[i], nullptr, true);

        renderUniformBuffersMapped[i] = allocInfo.pMappedData;
    }
}

void Uniforms::createOfflineBuffer() {
    VkDeviceSize bufferSize = sizeof(OfflineRenderBuffer);

    offlineRenderBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    offlineRenderBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    offlineRenderBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, offlineRenderBuffers[i], 
                    VMA_MEMORY_USAGE_CPU_TO_GPU, offlineRenderBuffersMemory[i], nullptr, true);

        offlineRenderBuffersMapped[i] = allocInfo.pMappedData; 
    }
}

void Uniforms::createComputeBuffer(const uint32_t camCount) {
    // NOVEL UNIFORM BUFFERS creationg
    VkDeviceSize bufferSize = sizeof(NovelImageObject);

    novelUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    novelUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    novelUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, novelUniformBuffers[i],
                VMA_MEMORY_USAGE_CPU_TO_GPU, novelUniformBuffersMemory[i], nullptr, true);

        novelUniformBuffersMapped[i] = allocInfo.pMappedData;
    }

    // CAM ARRAY SSBO creation
    bufferSize = sizeof(CamArrayData);

    camArraySSBOIn.resize(MAX_FRAMES_IN_FLIGHT);
    camArraySSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    camArrayCounts.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        camArrayCounts[i] = ((camCount / 10) + 1) * 10;
        memManager.createBuffer(bufferSize * camArrayCounts[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, camArraySSBOIn[i],
                VMA_MEMORY_USAGE_GPU_ONLY, camArraySSBOMemory[i]);
    }

    
    // INTERSECTIONS SSBO creation
    intersectionsSSBOOut.resize(MAX_FRAMES_IN_FLIGHT);
    intersectionsSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            memManager.createBuffer(sizeof(glm::vec3) * 2 * maxScreenRes.width * maxScreenRes.height * camArrayCounts[i], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, intersectionsSSBOOut[i],
                VMA_MEMORY_USAGE_GPU_ONLY, intersectionsSSBOMemory[i]);
    }

    // VERTEX COUNTS SSBO creationg
    vertexCountSSBOOut.resize(MAX_FRAMES_IN_FLIGHT);
    vertexCountSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
    vertexCountSSBOMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManager.createBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, vertexCountSSBOOut[i],
                VMA_MEMORY_USAGE_GPU_TO_CPU, vertexCountSSBOMemory[i], nullptr, true);
        vertexCountSSBOMapped[i] = allocInfo.pMappedData;
    }   
}

void Uniforms::updateRenderUniformBuffers(uint32_t currentImage, const Camera& cam) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    // ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    ubo.model = glm::rotate(ubo.model, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = cam.getViewMatrix();
    ubo.proj = cam.getProjectionMatrix();

    memcpy(renderUniformBuffersMapped[currentImage], &ubo, sizeof(UniformBufferObject));
}

void Uniforms::updateOfflineRenderBuffers(uint32_t currentImage, CamerasManager& camManager, PresentationMode mode, ImageViewType viewType) {
    OfflineRenderBuffer ubo{};
    ubo.grid = glm::ivec2(3, ((camManager.getCamCount()-1)/3)+1);
    ubo.layerCnt = camManager.getCamCount();
    if (camManager.novelViewToggeled() || camManager.observerToggeled()){
        ubo.layerID = -1;
    }
    else {
        ubo.layerID = camManager.getActiveIndex();
    }
    ubo.presentMode = mode;   // get from input manager or something as argument
    ubo.presentType = viewType;

    memcpy(offlineRenderBuffersMapped[currentImage], &ubo, sizeof(OfflineRenderBuffer));
}

void Uniforms::updateComputeUniformBuffers(uint32_t currentImage, CamerasManager& camManager, const VkExtent2D& extent, DebugCompute debugFlag) {
    NovelImageObject novelData {
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

    memcpy(novelUniformBuffersMapped[currentImage], &novelData, sizeof(NovelImageObject));
}

uint32_t Uniforms::getVertexCount(uint32_t currentImage) {
    uint32_t vertCount = 0;

    memcpy(&vertCount, vertexCountSSBOMapped[currentImage], sizeof(uint32_t));
    return vertCount;    
}

void Uniforms::recreateIntersectionsSSBO(const uint32_t currentFrame) {
    memManager.destroyBuffer(intersectionsSSBOOut[currentFrame], intersectionsSSBOMemory[currentFrame]);
    memManager.createBuffer(sizeof(glm::vec3) * 2 * maxScreenRes.width * maxScreenRes.height * camArrayCounts[currentFrame], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, intersectionsSSBOOut[currentFrame],
            VMA_MEMORY_USAGE_GPU_ONLY, intersectionsSSBOMemory[currentFrame]);
}

bool Uniforms::recreateCamArraySSBO(const uint32_t newCount, const uint32_t currentFrame) {
    // clear out the unused space if too large or size up the buffer
    if ((newCount > camArrayCounts[currentFrame]) || (camArrayCounts[currentFrame] - newCount >= 15)) {
        camArrayCounts[currentFrame] = ((newCount / 10) + 1) * 10;

        memManager.destroyBuffer(camArraySSBOIn[currentFrame], camArraySSBOMemory[currentFrame]);
        memManager.createBuffer(sizeof(CamArrayData) * camArrayCounts[currentFrame], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, camArraySSBOIn[currentFrame],
                VMA_MEMORY_USAGE_GPU_ONLY, camArraySSBOMemory[currentFrame]);

        recreateIntersectionsSSBO(currentFrame);
        resGrew = false;

        return true;
    }

    return false;
}

bool Uniforms::setCamArrayData(uint32_t currentImage, CamerasManager& camManager) {
    bool updated = false;
    uint32_t camCount = camManager.getCamCount();
    std::vector<CamArrayData> camData(camCount);

    for (uint32_t i = 0; i < camCount; i++) {
        camData[i] = camManager.camArray[i].getCamData();
    }

    // check if the SSO is large enough
    if (camCount != camArrayCounts[currentImage]) {
        updated |= recreateCamArraySSBO(camCount, currentImage);
    }
    if (resGrew) {  // recreate interseciton if resolution grew up
        recreateIntersectionsSSBO(currentImage);
        resGrew = false;
        updated |= true;
    }
    
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    auto allocInfo = memManager.createBuffer(sizeof(CamArrayData) * camData.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBufferMemory, camData.data());
    memManager.copyBuffer(stagingBuffer, camArraySSBOIn[currentImage], sizeof(CamArrayData) * camData.size());

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);

    return updated;
}