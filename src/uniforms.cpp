#include "uniforms.hpp"
#include "memManager.hpp"
#include "camera.hpp"

Uniforms::Uniforms(const VkDevice& device, MemoryManager& memManager)
    : deviceHandle(device), memManager(memManager) {
    createUniformBuffers();
}

Uniforms::~Uniforms() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManager.destroyBuffer(uniformBuffers[i], uniformBuffersMemory[i]);
    }
}

void Uniforms::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo{};
        allocInfo = memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, uniformBuffers[i],
                VMA_MEMORY_USAGE_CPU_TO_GPU, uniformBuffersMemory[i]);

        uniformBuffersMapped[i] = allocInfo.pMappedData;
    }
}

void Uniforms::updateUniformBuffers(uint32_t currentImage, const Camera& cam) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.model = glm::rotate(ubo.model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    ubo.view = cam.getViewMatrix();
    ubo.proj = cam.getProjectionMatrix();

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}