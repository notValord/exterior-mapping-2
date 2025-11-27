#include <uniforms.hpp>
#include <memManager.hpp>
#include <camManager.hpp>
#include <vertex.hpp>

Uniforms::Uniforms(const VkDevice device, MemoryManager& memManager, const VkExtent2D& extentSize, const uint32_t camCount)
    : deviceHandle(device), memManager(memManager) {
    createRenderUniformBuffers();
    createComputeBuffer(camCount, extentSize);
}

Uniforms::~Uniforms() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManager.destroyBuffer(renderUniformBuffers[i], renderUniformBuffersMemory[i]);
        memManager.destroyBuffer(novelUniformBuffers[i], novelUniformBuffersMemory[i]);
        memManager.destroyBuffer(intersectionsSSBOOut[i], intersectionsSSBOMemory[i]);
        memManager.destroyBuffer(vertexCountSSBOOut[i], vertexCountSSBOMemory[i]);
    }

    memManager.destroyBuffer(camArraySSBOIn, camArraySSBOMemory);       // not per frame data
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

void Uniforms::createComputeBuffer(const uint32_t camCount, const VkExtent2D& extentSize) {
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

    bufferSize = sizeof(CamArrayData);

    VmaAllocationInfo allocInfo{};
    allocInfo = memManager.createBuffer(bufferSize * camCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, camArraySSBOIn,
            VMA_MEMORY_USAGE_GPU_ONLY, camArraySSBOMemory); 

    intersectionsSSBOOut.resize(MAX_FRAMES_IN_FLIGHT);
    intersectionsSSBOMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManager.createBuffer(sizeof(glm::vec3) * 2 * extentSize.width * extentSize.height * camCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, intersectionsSSBOOut[i],
                VMA_MEMORY_USAGE_GPU_ONLY, intersectionsSSBOMemory[i]);
    }

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
    ubo.view = cam.getViewMatrix();
    ubo.proj = cam.getProjectionMatrix();

    memcpy(renderUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Uniforms::updateComputeUniformBuffers(uint32_t currentImage, CamerasManager& camManager, const VkExtent2D& extent) {
    NovelImageObject novelData {
        .invViewMat = glm::inverse(camManager.novelView.getViewMatrix()),
        .invProjMat = glm::inverse(camManager.novelView.getProjectionMatrix()),
        .res = glm::vec2(extent.width, extent.height),
        .camCnt = camManager.getCamCount()
    };

    memcpy(novelUniformBuffersMapped[currentImage], &novelData, sizeof(novelData));
}

uint32_t Uniforms::getVertexCount(uint32_t currentImage) {
    uint32_t vertCount = 0;

    memcpy(&vertCount, vertexCountSSBOMapped[currentImage], sizeof(uint32_t));
    return vertCount;    
}

void Uniforms::setCamArrayData(CamerasManager& camManager) {
    std::vector<CamArrayData> camData(camManager.getCamCount());

    for (uint32_t i = 0; i < camManager.getCamCount(); i++) {
        camData[i] = camManager.camArray[i].getCamData();
    }
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    auto allocInfo = memManager.createBuffer(sizeof(CamArrayData) * camData.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBufferMemory, camData.data());
    memManager.copyBuffer(stagingBuffer, camArraySSBOIn, sizeof(CamArrayData) * camData.size());

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}