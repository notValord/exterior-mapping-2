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

#include <structs.hpp>

extern const size_t MAX_FRAMES_IN_FLIGHT;

class MemoryManager;
class Camera;
class CamerasManager;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

class BaseUniforms {
public:
    std::vector<VkBuffer> uniformBuffers;

    BaseUniforms(MemoryManager& memManager);
    virtual ~BaseUniforms();

protected:
    std::vector<VmaAllocation> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;        // pointer to the mapped memory
    // The buffer stays mapped to this pointer for the application's whole lifetime ("persistent mapping") and works on all Vulkan implementations.
    // Not having to map the buffer every time increases performances, as mapping is not free.

    MemoryManager& memManagerRef;

    void createUniformBuffers(VkDeviceSize bufferSize);
};



class RenderUniforms : public BaseUniforms {  // Graphics normal render
public:
    std::vector<VkBuffer> fragmentUniformBuffers;

    RenderUniforms(MemoryManager& memManager);
    ~RenderUniforms();

    void updateUniformBuffers(uint32_t currentImage, const Camera& cam, bool showDepth);

private:
    std::vector<VmaAllocation> fragmentUniformBuffersMemory;
    std::vector<void*> fragmentUniformBuffersMapped;

    void createFragmentUniformBuffers(VkDeviceSize bufferSize = sizeof(RenderFragmentObject));
};



class OfflineUniforms : public BaseUniforms {
public:
    OfflineUniforms(MemoryManager& memManager);

    void updateUniformBuffers(uint32_t currentImage, CamerasManager& camManager, PresentationMode mode, ImageViewType viewType);
private:
};


class NovelUniforms : public BaseUniforms {     // Novel compute
public:
    std::vector<VkBuffer> camArraySSBOIn;
    std::vector<VkBuffer> intersectionsSSBOOut;
    std::vector<VkBuffer> intersectCountSSBOOut;

    NovelUniforms(MemoryManager& memManager, const uint32_t camCount, const VkExtent2D& extentSize);
    ~NovelUniforms();

    void updateUniformBuffers(uint32_t currentImage, CamerasManager& camManager, const VkExtent2D& extent, DebugCompute debugFlag, NovelHeuristic heuristic);

    bool setCamArrayData(uint32_t currentImage, CamerasManager& camManager);
    uint32_t getIntersectCount(uint32_t currentImage);

private:
    std::vector<VmaAllocation> camArraySSBOMemory;
    std::vector<VmaAllocation> intersectionsSSBOMemory;
    std::vector<VmaAllocation> intersectCountSSBOMemory;
    std::vector<void*> intersectCountSSBOMapped;        // pointer to the mapped memory

    std::vector<uint32_t> camCounts;

    VkExtent2D maxScreenRes;
    bool resGrew = false;

    void createStorageBuffers(const uint32_t camCount);

    bool recreateCamArraySSBO(const uint32_t newCount, const uint32_t currentFrame);
    void recreateIntersectionsSSBO(const uint32_t currentFrame);
};



class PointCloudUniforms : public BaseUniforms {        // Point cloud compute
public:
    std::vector<VkBuffer> camMatricesSSBOIn;        // the In buffers are static, no need of copy 
    std::vector<VkBuffer> pointCloudSSBOOut;
    std::vector<VkBuffer> pointCountSSBOOut;

    PointCloudUniforms(MemoryManager& memManager);
    ~PointCloudUniforms();

    void setScreenRes(const VkExtent2D& newScreenRes);
    void updateUniformBuffers(CamerasManager& camManager);      // update only once after the offline images are created with the set resolution

    uint32_t getPointCount(uint32_t currentImage);
private:
    std::vector<VmaAllocation> camMatricesSSBOMemory;
    std::vector<VmaAllocation> pointCloudSSBOMemory;
    std::vector<VmaAllocation> pointCountSSBOMemory;
    std::vector<void*> pointCountSSBOMapped;

    VkExtent2D currScreenRes;

    void createStorageBuffers(const uint32_t camCount);
    void recreateStorageBuffers(const uint32_t camCount);
    void updateStorageBuffers(CamerasManager& camManager);
};



class UniformManager {
public:
    RenderUniforms renderUniforms;
    OfflineUniforms offlineUniforms;
    NovelUniforms novelUnifroms;
    PointCloudUniforms pointCloudUniforms;

    UniformManager(MemoryManager& memManager, const VkExtent2D& extentSize, const uint32_t camCount);
};