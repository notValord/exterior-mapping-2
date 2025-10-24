#include "uniforms.hpp"
#include "util.hpp"

Uniforms::Uniforms(const VkDevice& device, MemoryManager& memManager)
    : deviceHandle(device), memManager(memManager) {
    createUniformBuffers();
}

Uniforms::~Uniforms() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkUnmapMemory(deviceHandle, uniformBuffersMemory[i]);
        vkFreeMemory(deviceHandle, uniformBuffersMemory[i], nullptr);
        vkDestroyBuffer(deviceHandle, uniformBuffers[i], nullptr);
    }
}

void Uniforms::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, uniformBuffers[i],
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffersMemory[i]);
        vkMapMemory(deviceHandle, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void Uniforms::updateUniformBuffers(uint32_t currentImage, float screenRatio) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), screenRatio, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;       // compensation for the Y coordinate being inverted

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}