#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// system includes
#include <vector>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

class CamerasManager;
class MemoryManager;

extern const size_t MAX_FRAMES_IN_FLIGHT;

class DebugUtil {   // Debug frustum shader
public:
    std::vector<VkBuffer> frustumVertexBuffers;
    std::vector<VkBuffer> frustumIndexBuffers;

    DebugUtil(MemoryManager& memMan, const uint32_t camCount);
    ~DebugUtil();

    void setFrustumData(CamerasManager& camManager, uint32_t currentFrame);
    uint32_t getFrustumIndexCount(uint32_t curretnFrame) ;

private:
    std::vector<VmaAllocation> frustumVertexBufferMemories;
    std::vector<VmaAllocation> frustumIndexBufferMemory;

    std::vector<uint32_t> frustumCounts;

    // Vulkan handles
    MemoryManager& memManager;

    void createFrustrumBuffers();
    void recreateFrustumBuffers(uint32_t newCount, uint32_t currentFrame);
};