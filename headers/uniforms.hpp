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
#include <textures.hpp>

extern const size_t MAX_FRAMES_IN_FLIGHT;

class MemoryManager;
class Camera;
class CamerasManager;
class InputManager;
class Sampler;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

/**
 * @class BaseUniforms
 * @brief Allocates a set of uniform buffers for all frames in flight.
 * 
 * Derived classes can extend this for additional per-frame uniform or storage resources.
 */
class BaseUniforms {
public:
    std::vector<VkBuffer> uniformBuffers;  // Per-frame uniform buffers.

    BaseUniforms(MemoryManager& memManager);
    virtual ~BaseUniforms();

protected:
    std::vector<VmaAllocation> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;        // pointer to the mapped memory
    // The buffer stays mapped to this pointer for the application's whole lifetime ("persistent mapping") and works on all Vulkan implementations.
    // Not having to map the buffer every time increases performances, as mapping is not free.

    MemoryManager& memManagerRef;

    /**
     * @brief Allocates and maps a uniform buffer for each frame in flight.
     * @param bufferSize The size of each uniform buffer in bytes.
     */
    void createUniformBuffers(VkDeviceSize bufferSize);
};



/**
 * @class RenderUniforms
 * @brief Manages per-frame uniforms used by the standard graphics render pass.
 *
 * Holds both the MVP uniform buffer and a separate fragment uniform buffer used for
 * lighting, camera position, and depth/debug rendering flags.
 */
class RenderUniforms : public BaseUniforms {
public:
    std::vector<VkBuffer> fragmentUniformBuffers;

    RenderUniforms(MemoryManager& memManager);
    ~RenderUniforms();

    /**
     * @brief Updates the per-frame MVP and fragment uniform buffers.
     * @param currentImage Frame index in flight.
     * @param cam Current camera used for view/projection.
     * @param showDepth Render depth output when true.
     * @param light Light position in world space.
     * @param modelScale Uniform model scale.
     */
    void updateUniformBuffers(uint32_t currentImage, const Camera& cam, bool showDepth, const glm::vec3& light, float modelScale);

private:
    std::vector<VmaAllocation> fragmentUniformBuffersMemory;
    std::vector<void*> fragmentUniformBuffersMapped;

    /**
     * @brief Allocates fragment uniform buffers for each frame in flight.
     * @param bufferSize Size of each fragment uniform buffer.
     */
    void createFragmentUniformBuffers(VkDeviceSize bufferSize = sizeof(RenderFragmentObject));
};



/**
 * @class OfflineUniforms
 * @brief Uniforms used for offline rendering passes.
 *
 * Contains a single MVP-style uniform buffer that is updated with offline camera
 * and presentation state before rendering to offscreen or novel-view targets.
 */
class OfflineUniforms : public BaseUniforms {
public:
    OfflineUniforms(MemoryManager& memManager);

    /**
     * @brief Updates the offline frame-specific uniform buffer.
     * @param currentImage Frame index in flight.
     * @param camManager Cameras manager providing camera state.
     * @param mode Presentation mode for offline rendering.
     * @param viewType Type of image view to present.
     */
    void updateUniformBuffers(uint32_t currentImage, CamerasManager& camManager, PresentationMode mode, ImageViewType viewType);
private:
};


/**
 * @class NovelUniforms
 * @brief Manages compute buffers for novel view generation.
 *
 * Allocates per-frame storage buffers for camera arrays, intersection results, and point counts.
 * Handles buffer resizing when camera count or screen resolution grows.
 */
class NovelUniforms : public BaseUniforms {
public:
    std::vector<VkBuffer> camArraySSBOIn;
    std::vector<VkBuffer> intersectionsSSBOOut;
    std::vector<VkBuffer> intersectCountSSBOOut;

    NovelUniforms(MemoryManager& memManager, const uint32_t camCount, const VkExtent2D& extentSize);
    ~NovelUniforms();

    /**
     * @brief Updates the novel view uniform buffer with current camera and input state.
     * @param currentImage Frame index in flight.
     * @param camManager Camera manager providing novel view matrices.
     * @param extent Current render resolution.
     * @param input Input manager containing algorithm options.
     * @param debug Debug compute mode.
     */
    void updateUniformBuffers(uint32_t currentImage, CamerasManager& camManager, const VkExtent2D& extent, const InputManager& input, const DebugCompute debug);

    /**
     * @brief Copies current camera array data into the SSBO for the given frame.
     * @param currentImage Frame index in flight.
     * @param camManager Camera manager providing camera array data.
     * @return true if the underlying SSBO was recreated due to size changes.
     */
    bool setCamArrayData(uint32_t currentImage, CamerasManager& camManager);

    /**
     * @brief Reads the per-frame intersection count from the mapped SSBO.
     * @param currentImage Frame index in flight.
     * @return The number of intersections recorded.
     */
    uint32_t getIntersectCount(uint32_t currentImage);

private:
    std::vector<VmaAllocation> camArraySSBOMemory;
    std::vector<VmaAllocation> intersectionsSSBOMemory;
    std::vector<VmaAllocation> intersectCountSSBOMemory;
    std::vector<void*> intersectCountSSBOMapped;        // pointer to the mapped memory

    std::vector<uint32_t> camCounts;

    VkExtent2D maxScreenRes;
    bool resGrew = false;

    /**
     * @brief Allocates the novel compute storage buffers for all frames.
     * @param camCount Number of cameras used for novel view synthesis.
     */
    void createStorageBuffers(const uint32_t camCount);

    bool recreateCamArraySSBO(const uint32_t newCount, const uint32_t currentFrame);
    void recreateIntersectionsSSBO(const uint32_t currentFrame);
};


/**
 * @class PointCloudUniforms
 * @brief Manages buffers for rendering point cloud representations.
 *
 * Holds per-frame input matrices, output point buffers, and point count storage.
 * Buffers are recreated when the offline image resolution or camera count changes.
 */
class PointCloudUniforms : public BaseUniforms {
public:
    std::vector<VkBuffer> camMatricesSSBOIn;
    std::vector<VkBuffer> pointCloudSSBOOut;
    std::vector<VkBuffer> pointCountSSBOOut;

    PointCloudUniforms(MemoryManager& memManager);
    ~PointCloudUniforms();

    /**
     * @brief Updates the current screen resolution used for point cloud buffer sizing.
     * @param newScreenRes New offline image resolution.
     */
    void setScreenRes(const VkExtent2D& newScreenRes);

    /**
     * @brief Updates the point cloud uniform and storage buffers based on the camera manager.
     * @param camManager Camera manager containing camera count and matrices.
     */
    void updateUniformBuffers(CamerasManager& camManager);      // update only once after the offline images are created with the set resolution

    /**
     * @brief Updates the point cloud storage buffers with current camera matrices.
     * @param camManager Camera manager providing camera array data.
     */
    void updateStorageBuffers(CamerasManager& camManager);      // update every frame before rendering the point cloud to get the correct cma position

    /**
     * @brief Reads the mapped point count buffer for a frame.
     * @param currentImage Frame index in flight.
     * @return The number of points produced.
     */
    uint32_t getPointCount(uint32_t currentImage);
private:
    std::vector<VmaAllocation> camMatricesSSBOMemory;
    std::vector<VmaAllocation> pointCloudSSBOMemory;
    std::vector<VmaAllocation> pointCountSSBOMemory;
    std::vector<void*> pointCountSSBOMapped;

    VkExtent2D currScreenRes;

    void createStorageBuffers(const uint32_t camCount);
    void recreateStorageBuffers(const uint32_t camCount);
};


/**
 * @class RayDataUniforms
 * @brief Manages ray-tracing buffers for novel synthesis.
 *
 * Stores per-frame ray data and ray count buffers, and resizes them when the render resolution increases.
 */
class RayDataUniforms : public BaseUniforms {
public:
    std::vector<VkBuffer> rayDataSSBOIn;
    std::vector<VkBuffer> rayCountSSBOIn;

    RayDataUniforms(MemoryManager& memManager, VkExtent2D screenRes);
    ~RayDataUniforms();

    /**
     * @brief Updates ray data uniforms and extends the storage buffers if resolution grew.
     * @param currentImage Frame index in flight.
     * @param novelCam Novel camera used for current view.
     * @param screenRes Current render resolution.
     * @return true if buffers were recreated due to resolution growth.
     */
    bool updateUniformBuffers(uint32_t currentImage, Camera& novelCam, VkExtent2D screenRes);
private:
    std::vector<VmaAllocation> rayDataSSBOMemory;
    std::vector<VmaAllocation> rayCountSSBOMemory;
    std::vector<VkExtent2D> maxScreenRes;

    void createRayDataBuffers();
    void recreateRayDataBuffers(uint32_t currentImage);

    void destroyDataBuffers(uint32_t currentImage);
};


/**
 * @class NovelSynthUniforms
 * @brief Manages images used during novel view synthesis.
 *
 * Allocates debug and result images per frame and recreates them when resolution or camera count changes.
 */
class NovelSynthUniforms : public BaseUniforms {
public:
    std::vector<VkImageView> debugImageView;
    std::vector<VkImageView> resultImageView;

    NovelSynthUniforms(MemoryManager& memManager, VkDevice device, const uint32_t camCount, VkExtent2D screenRes);
    ~NovelSynthUniforms();

    /**
     * @brief Updates the number of image layers for camera count changes.
     * @param camCount New camera layer count.
     */
    void setCamCount(const uint32_t camCount);

    /**
     * @brief Updates the render resolution and recreates images for the current frame if needed.
     * @param screenRes New resolution.
     * @param currentFrame Frame index in flight.
     * @return true if the frame's images were recreated.
     */
    bool setResolution(VkExtent2D screenRes, uint32_t currentFrame);

    std::vector<VkImage> getResultImages();
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

/**
 * @class NovelReconUniforms
 * @brief Manages result images and a sampler for novel reconstruction.
 *
 * Allocates per-frame result images, recreates them for resolution changes, and manages image layout transitions.
 */
class NovelReconUniforms : public BaseUniforms {
public:
    std::vector<VkImageView> resultImageView;
    Sampler colorSampler;

    NovelReconUniforms(MemoryManager& memManager, VkDevice device, VkExtent2D screenRes, const VkPhysicalDeviceProperties& prop);
    ~NovelReconUniforms();

    /**
     * @brief Recreates result images when the resolution changes.
     * @param screenRes New resolution.
     * @param currentFrame Frame index in flight.
     * @return true if images were recreated.
     */
    bool setResolution(VkExtent2D screenRes, uint32_t currentFrame);

    /**
     * @brief Updates the reconstruction uniform buffer for the current frame.
     * @param currentFrame Frame index in flight.
     * @param layerCount Number of layers used by the reconstruction.
     * @param novelRes Novel view resolution.
     */
    void updateUniformBuffers(uint32_t currentFrame, uint32_t layerCount, VkExtent2D novelRes);

    /**
     * @brief Transitions the result image for the current frame to a new layout.
     * @param currentFrame Frame index in flight.
     * @param newLayout Target image layout.
     * @param commandBuffer Command buffer used for the layout transition.
     */
    void transferResultLayout(uint32_t currentFrame, VkImageLayout newLayout, VkCommandBuffer commandBuffer);

    VkImage getResultImage(uint32_t currentFrame);
private:
    std::vector<VkImage> resultImage;
    std::vector<VmaAllocation> resultImageMemory;

    std::vector<VkExtent2D> currScreenRes;
    std::vector<VkImageLayout> resultLayout;

    VkDevice deviceHandle;

    void createImages();
    void recreateImages(uint32_t currentFrame);


    void destroyImages(uint32_t currentFrame);
};



/**
 * @class UniformManager
 * @brief Aggregates all uniform and storage buffer managers used by the application.
 *
 * Provides a single entry point for creating and accessing the various uniform groups
 * required by graphics rendering, offline rendering, novel synthesis, and point cloud generation.
 */
class UniformManager {
public:
    RenderUniforms renderUniforms;
    OfflineUniforms offlineUniforms;
    NovelUniforms novelUniforms;
    PointCloudUniforms pointCloudUniforms;
    RayDataUniforms rayDataUniforms;
    NovelSynthUniforms novelSynthUniforms;
    NovelReconUniforms novelReconUniforms;

    UniformManager(MemoryManager& memManager, VkDevice device, const VkExtent2D& extentSize, const uint32_t camCount, const VkPhysicalDeviceProperties& prop);
};