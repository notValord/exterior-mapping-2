#include "debug.hpp"
#include "vertex.hpp"
#include "memManager.hpp"
#include "camManager.hpp"
#include "uniforms.hpp"

// 36 indices per frustum (6 faces × 2 triangles × 3 vertices)
const std::vector<uint32_t> frustumIndices = {
    // Near face
    0, 1, 2,
    2, 1, 3,

    // Far face
    4, 6, 5,
    5, 6, 7,

    // Left face
    0, 4, 1,
    1, 4, 5,

    // Right face
    2, 3, 6,
    3, 7, 6,

    // Top face
    0, 2, 4,
    2, 6, 4,

    // Bottom face
    1, 5, 3,
    3, 5, 7
};

static glm::vec3 intersectPlanes(const glm::vec4& p1, const glm::vec4& p2, const glm::vec4& p3)
{
    glm::vec3 n1 = glm::vec3(p1);
    glm::vec3 n2 = glm::vec3(p2);
    glm::vec3 n3 = glm::vec3(p3);

    float d1 = p1.w;
    float d2 = p2.w;
    float d3 = p3.w;

    glm::vec3 n2xn3 = glm::cross(n2, n3);
    glm::vec3 n3xn1 = glm::cross(n3, n1);
    glm::vec3 n1xn2 = glm::cross(n1, n2);

    float denom = glm::dot(n1, n2xn3);

    // If denom ≈ 0, the planes are parallel or nearly parallel — no unique intersection.
    return (-d1 * n2xn3 - d2 * n3xn1 - d3 * n1xn2) / denom;
}

DebugUtil::DebugUtil(MemoryManager& memMan, const uint32_t camCount)
    : memManager(memMan) {
    createFrustrumBuffer(camCount);
}

DebugUtil::~DebugUtil() {
    memManager.destroyBuffer(frustumVertexBuffer, frustumVertexBufferMemory);
    memManager.destroyBuffer(frustumIndexBuffer, frustumIndexBufferMemory);
}

void DebugUtil::createFrustrumBuffer(const uint32_t camCount) {
    VkDeviceSize vertexSize = sizeof(Vertex) * camCount * 8;
    VkDeviceSize indexSize = sizeof(uint32_t) * camCount * frustumIndices.size();
    memManager.createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, frustumVertexBuffer, VMA_MEMORY_USAGE_GPU_ONLY, frustumVertexBufferMemory);
    memManager.createBuffer(indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, frustumIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY, frustumIndexBufferMemory);

}

void DebugUtil::setFrustumData(CamerasManager& camManager) {
    std::vector<Vertex> frustrumPoints(camManager.getCamCount() * 8);
    std::vector<uint32_t> frustumIndex(camManager.getCamCount() * frustumIndices.size());

    for (uint32_t i = 0; i < camManager.getCamCount(); i++) {
        uint32_t base = i * 8; // 8 vertices per camera
        CamArrayData camData = camManager.camArray[i].getCamData();

        frustrumPoints[base+0].pos = intersectPlanes(camData.frustumPlanes[4], camData.frustumPlanes[0], camData.frustumPlanes[3]); // near, left, top
        frustrumPoints[base+1].pos = intersectPlanes(camData.frustumPlanes[4], camData.frustumPlanes[0], camData.frustumPlanes[2]); // near, left, bottom

        frustrumPoints[base+2].pos = intersectPlanes(camData.frustumPlanes[4], camData.frustumPlanes[1], camData.frustumPlanes[3]); // near, right, top
        frustrumPoints[base+3].pos = intersectPlanes(camData.frustumPlanes[4], camData.frustumPlanes[1], camData.frustumPlanes[2]); // near, right, bottom

        frustrumPoints[base+4].pos = intersectPlanes(camData.frustumPlanes[5], camData.frustumPlanes[0], camData.frustumPlanes[3]); // far, left, top
        frustrumPoints[base+5].pos = intersectPlanes(camData.frustumPlanes[5], camData.frustumPlanes[0], camData.frustumPlanes[2]); // far, left, bottom

        frustrumPoints[base+6].pos = intersectPlanes(camData.frustumPlanes[5], camData.frustumPlanes[1], camData.frustumPlanes[3]); // far, right, top
        frustrumPoints[base+7].pos = intersectPlanes(camData.frustumPlanes[5], camData.frustumPlanes[1], camData.frustumPlanes[2]); // far, right, bottom

        for (uint32_t colors = 0; colors < 8; colors++) {
            frustrumPoints[base+colors].color = glm::vec3(0.3, 0.0, 0.0);
        }

        for (uint32_t id = 0; id < frustumIndices.size(); id++) {
            frustumIndex[base+id] = frustumIndices[id] + base;
        }
    }
    
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    auto allocInfo = memManager.createBuffer(sizeof(Vertex) * frustrumPoints.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBufferMemory, frustrumPoints.data());
    memManager.copyBuffer(stagingBuffer, frustumVertexBuffer, sizeof(Vertex) * frustrumPoints.size());

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);

    allocInfo = memManager.createBuffer(sizeof(uint32_t) * frustumIndex.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBufferMemory, frustumIndex.data());
    memManager.copyBuffer(stagingBuffer, frustumIndexBuffer, sizeof(uint32_t) * frustumIndex.size());

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}