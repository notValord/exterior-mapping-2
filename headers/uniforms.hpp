#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE     // GLM uses openGl depth range -1 to 1
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <cstring>
#include <vector>
#include <chrono>

extern const size_t MAX_FRAMES_IN_FLIGHT;

class MemoryManager;
class Camera;
class CamerasManager;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct NovelImageObject {
    // mat4 viewMat;
    glm::mat4 invViewMat;
    // mat4 projMat;
    glm::mat4 invProjMat;
    glm::vec2 res;
    // glm::vec2 _pad0;        // renderdoc complains
    uint32_t camCnt;
    uint32_t sampleCount;
    uint32_t debugIntersections;
    // uint32_t _padd1; 
};

struct CamArrayData {
    glm::vec4 frustumPlanes[6];
    glm::mat4 viewMat;
    glm::mat4 invViewMat;
    glm::mat4 projMat;
    glm::mat4 invProjMat;
};

class Uniforms{
public:
    // Graphics normal render
    std::vector<VkBuffer> renderUniformBuffers;

    // Offline ubo render buffer

    // Compute intersection shader
    std::vector<VkBuffer> novelUniformBuffers;
    std::vector<VkBuffer> camArraySSBOIn;
    std::vector<VkBuffer> intersectionsSSBOOut;
    std::vector<VkBuffer> vertexCountSSBOOut;

    Uniforms(const VkDevice device, MemoryManager& memManager, const VkExtent2D& extentSize, const uint32_t camCount);
    ~Uniforms();

    void updateRenderUniformBuffers(uint32_t currentImage, const Camera& cam);
    void updateComputeUniformBuffers(uint32_t currentImage, CamerasManager& camManager, const VkExtent2D& extent,  uint32_t debugFlag = 0);

    bool setCamArrayData(uint32_t currentImage, CamerasManager& camManager);

    uint32_t getVertexCount(uint32_t currentImage);

private:
    std::vector<VmaAllocation> renderUniformBuffersMemory;
    std::vector<void*> renderUniformBuffersMapped;        // pointer to the mapped memory
    // The buffer stays mapped to this pointer for the application's whole lifetime ("persistent mapping") and works on all Vulkan implementations.
    // Not having to map the buffer every time increases performances, as mapping is not free.

    std::vector<VmaAllocation>novelUniformBuffersMemory;
    std::vector<void*> novelUniformBuffersMapped;

    std::vector<uint32_t> camArrayCounts;
    std::vector<VmaAllocation> camArraySSBOMemory;
    std::vector<VmaAllocation>intersectionsSSBOMemory;
    std::vector<VmaAllocation>vertexCountSSBOMemory;
    std::vector<void*> vertexCountSSBOMapped;        // pointer to the mapped memory

    // Vulkan handels
    VkDevice deviceHandle;
    MemoryManager& memManager;

    VkExtent2D maxScreenRes;
    bool resGrew = false;

    void createRenderUniformBuffers();
    void createComputeBuffer(const uint32_t camCount);

    bool recreateCamArraySSBO(const uint32_t newCount, const uint32_t currentFrame);
    void recreateIntersectionsSSBO(const uint32_t currentFrame);
};