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
class InputManager;

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

    void updateUniformBuffers(uint32_t currentImage, const Camera& cam, bool showDepth, const glm::vec3& light, float modelScale);

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

    void updateUniformBuffers(uint32_t currentImage, CamerasManager& camManager, const VkExtent2D& extent, const InputManager& input);

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


class RayDataUniforms : public BaseUniforms {
public:
    std::vector<VkBuffer> rayDataSSBOIn;
    std::vector<VkBuffer> rayCountSSBOIn;

    RayDataUniforms(MemoryManager& memManager, VkExtent2D screenRes);
    ~RayDataUniforms();

    bool updateUniformBuffers(uint32_t currentImage, Camera& novelCam, VkExtent2D screenRes);
private:
    std::vector<VmaAllocation> rayDataSSBOMemory;
    std::vector<VmaAllocation> rayCountSSBOMemory;
    std::vector<VkExtent2D> maxScreenRes;

    void createRayDataBuffers();
    void recreateRayDataBuffers(uint32_t currentImage);

    void destroyDataBuffers(uint32_t currentImage);
};


class NovelSynthUniforms : public BaseUniforms {
public:
    std::vector<VkImageView> debugImageView;
    std::vector<VkImageView> resultImageView;

    NovelSynthUniforms(MemoryManager& memManager, VkDevice device, const uint32_t camCount, VkExtent2D screenRes);
    ~NovelSynthUniforms();

    void setCamCount(const uint32_t camCount);
    bool setResolution(VkExtent2D screenRes, uint32_t currentFrame);
private:
    std::vector<VkImage> debugImage;
    std::vector<VmaAllocation> debugImageMemory;

    std::vector<VkImage> resultImage;
    std::vector<VmaAllocation> resultImageMemory;

    uint32_t layerCount;
    std::vector<VkExtent2D> currScreenRes;

    VkDevice deviceHandle;

    void createDebugImages();
    void recreateDebugImages();

    void createDataImages();
    void recreateDataImages(uint32_t currentFrame);

    void recreateAllImages();

    void destroyDebugImages();
    void destroyDataImages(uint32_t currentFrame);
};



class UniformManager {
public:
    RenderUniforms renderUniforms;
    OfflineUniforms offlineUniforms;
    NovelUniforms novelUnifroms;
    PointCloudUniforms pointCloudUniforms;
    RayDataUniforms rayDataUniforms;
    NovelSynthUniforms novelSynthUniforms;

    UniformManager(MemoryManager& memManager, VkDevice device, const VkExtent2D& extentSize, const uint32_t camCount);
};