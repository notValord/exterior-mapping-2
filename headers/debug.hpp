#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// system includes
#include <vector>
#include <array>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

class CamerasManager;
class MemoryManager;

extern const size_t MAX_FRAMES_IN_FLIGHT;

/**
 * @class DebugUtil
 * @brief Utility for rendering camera frustum debug visualizations.
 * 
 *  Manages vertex and index buffers for drawing wireframe frustums of multiple cameras.
 */
class DebugUtil {   // Debug frustum shader
public:
    std::vector<VkBuffer> frustumVertexBuffers;
    std::vector<VkBuffer> frustumIndexBuffers;

    VkQueryPool timestampQueryPool;
    uint32_t recordedCount = 0;
    const uint32_t recordTimeCount = 10;
    const uint32_t timestampPerFrame = 8;

    /**
     * @brief Initialize debug utility with buffers for camera frustums.
     * @param memMan Memory manager for buffer allocation.
     * @param camCount Initial number of cameras to support.
     */
    DebugUtil(VkDevice device, MemoryManager& memMan, const uint32_t camCount, double timestampStep);
    ~DebugUtil();

    /**
     * @brief Update frustum data for the current frame based on camera positions.
     * @param camManager Camera manager providing frustum plane data.
     * @param currentFrame Frame index for buffer selection.
     */
    void setFrustumData(CamerasManager& camManager, uint32_t currentFrame);

    /**
     * @brief Get the total index count for frustum rendering in the current frame.
     * @param curretnFrame Frame index.
     * @return Number of indices to draw.
     */
    uint32_t getFrustumIndexCount(uint32_t curretnFrame) ;

    double getTimeStamp(uint32_t firstTimestamp, uint32_t timeStampCount = 2);
    void printTimestamp(uint32_t counterIndex);

private:
    std::vector<VmaAllocation> frustumVertexBufferMemories;
    std::vector<VmaAllocation> frustumIndexBufferMemory;

    std::vector<uint32_t> frustumCounts;
    std::array<std::vector<double>, 4> timingCounters;

    double timestampPeriod;

    // Vulkan handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void createFrustumBuffers();
    void recreateFrustumBuffers(uint32_t newCount, uint32_t currentFrame);

    void createTimeStamp();
};