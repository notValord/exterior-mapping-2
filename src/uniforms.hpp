#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE     // GLM uses openGl depth range -1 to 1
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <cstring>
#include <vector>
#include <chrono>

extern const int MAX_FRAMES_IN_FLIGHT;

class MemoryManager;

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

class Uniforms{
public:
    std::vector<VkBuffer> uniformBuffers;

    Uniforms(const VkDevice& device, MemoryManager& memManager);
    ~Uniforms();

    void updateUniformBuffers(uint32_t currentImage, float screenRatio);

private:
    std::vector<VkDeviceMemory>uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;        // pointer to the mapped memory
    // The buffer stays mapped to this pointer for the application's whole lifetime ("persistent mapping") and works on all Vulkan implementations.
    // Not having to map the buffer every time increases performances, as mapping is not free.

    // Vulkan handels
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void createUniformBuffers();
};