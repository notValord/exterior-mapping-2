#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

class CamerasManager;
class MemoryManager;

class DebugUtil {
public:
    // Debug frustum shader
    VkBuffer frustumVertexBuffer;
    VkBuffer frustumIndexBuffer;

    DebugUtil(MemoryManager& memMan, const uint32_t camCount);
    ~DebugUtil();

    void setFrustumData(CamerasManager& camManager);

private:
    VmaAllocation frustumVertexBufferMemory;
    VmaAllocation frustumIndexBufferMemory;

    // Vulkan handles
    MemoryManager& memManager;

    void createFrustrumBuffer(const uint32_t camCount);
};