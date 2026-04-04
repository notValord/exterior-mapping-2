#include <debug.hpp>
#include <vertex.hpp>
#include <memManager.hpp>
#include <camManager.hpp>
#include <uniforms.hpp>

#define UINT32_RESTART_VALUE 0xFFFFFFFF

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
const uint32_t frustumVertexCount = 8;

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
    frustumCounts.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
        frustumCounts[frame] = camCount;
    }

    createFrustumBuffers();
}

DebugUtil::~DebugUtil() {
    for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
        memManager.destroyBuffer(frustumVertexBuffers[frame], frustumVertexBufferMemories[frame]);
        memManager.destroyBuffer(frustumIndexBuffers[frame], frustumIndexBufferMemory[frame]);
    }
}

uint32_t DebugUtil::getFrustumIndexCount(uint32_t currentFrame) {
    return frustumCounts[currentFrame] * frustumIndices.size();
}

void DebugUtil::createFrustumBuffers() {
    VkDeviceSize vertexSize = sizeof(Vertex) * frustumVertexCount * frustumCounts[0];
    VkDeviceSize indexSize = sizeof(uint32_t) * frustumIndices.size() * frustumCounts[0];

    frustumVertexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    frustumVertexBufferMemories.resize(MAX_FRAMES_IN_FLIGHT);

    frustumIndexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    frustumIndexBufferMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
        memManager.createBuffer(vertexSize,
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_SHARING_MODE_EXCLUSIVE,
                                frustumVertexBuffers[frame],
                                VMA_MEMORY_USAGE_GPU_ONLY,
                                frustumVertexBufferMemories[frame]);
    
        memManager.createBuffer(indexSize,
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                VK_SHARING_MODE_EXCLUSIVE,
                                frustumIndexBuffers[frame],
                                VMA_MEMORY_USAGE_GPU_ONLY,
                                frustumIndexBufferMemory[frame]);
    }
}

void DebugUtil::recreateFrustumBuffers(uint32_t newCount, uint32_t currentFrame) {
    // if (newCount < frustumCounts[currentFrame]) {
    //     while (frustumCounts[currentFrame] != newCount) {
    //         frustumCounts[currentFrame]--;
            
    //         memManager.destroyBuffer(frustumVertexBuffers[currentFrame][frustumCounts[currentFrame]], frustumVertexBufferMemories[currentFrame][frustumCounts[currentFrame]]);
    //         frustumVertexBuffers[currentFrame].pop_back();
    //         frustumVertexBufferMemories[currentFrame].pop_back();
    //     }
    // }

    // if (newCount > frustumCounts[currentFrame]) {
    //     VkDeviceSize vertexSize = sizeof(Vertex)* 8;

    //     frustumVertexBuffers[currentFrame].resize(newCount);
    //     frustumVertexBufferMemories[currentFrame].resize(newCount);

    //     while (frustumCounts[currentFrame] != newCount) {
    //         memManager.createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, frustumVertexBuffers[currentFrame][frustumCounts[currentFrame]], VMA_MEMORY_USAGE_GPU_ONLY, frustumVertexBufferMemories[currentFrame][frustumCounts[currentFrame]]);

    //         frustumCounts[currentFrame]++;
    //     }
    // }

    frustumCounts[currentFrame] = newCount;

    VkDeviceSize vertexSize = sizeof(Vertex) * frustumVertexCount * frustumCounts[currentFrame];
    VkDeviceSize indexSize = sizeof(uint32_t) * (frustumIndices.size() * frustumCounts[currentFrame]);// + frustumCounts[currentFrame]);

    memManager.destroyBuffer(frustumVertexBuffers[currentFrame], frustumVertexBufferMemories[currentFrame]);
    memManager.createBuffer(vertexSize,
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_SHARING_MODE_EXCLUSIVE,
                            frustumVertexBuffers[currentFrame],
                            VMA_MEMORY_USAGE_GPU_ONLY,
                            frustumVertexBufferMemories[currentFrame]);

    memManager.destroyBuffer(frustumIndexBuffers[currentFrame], frustumIndexBufferMemory[currentFrame]);
    memManager.createBuffer(indexSize,
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            VK_SHARING_MODE_EXCLUSIVE,
                            frustumIndexBuffers[currentFrame],
                            VMA_MEMORY_USAGE_GPU_ONLY,
                            frustumIndexBufferMemory[currentFrame]);
}

void DebugUtil::setFrustumData(CamerasManager& camManager, uint32_t currentFrame) {
    if (camManager.getCamCount() != frustumCounts[currentFrame]){
        recreateFrustumBuffers(camManager.getCamCount(), currentFrame);
    }

    std::vector<uint32_t> frustumIndex;
    frustumIndex.reserve(frustumIndices.size() * frustumCounts[currentFrame]);
    for (uint32_t camID = 0; camID < frustumCounts[currentFrame]; camID++) {
        uint32_t indexOffset = camID * frustumVertexCount;

        for (uint32_t id = 0; id < frustumIndices.size(); id++) {
            frustumIndex.push_back(frustumIndices[id] + indexOffset);
        }
    }

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;
    auto allocInfo = memManager.createBuffer(sizeof(uint32_t) * frustumIndex.size(),
                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                             VK_SHARING_MODE_EXCLUSIVE,
                                             stagingBuffer,
                                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                                             stagingBufferMemory,
                                             frustumIndex.data());
    memManager.copyBuffer(stagingBuffer, frustumIndexBuffers[currentFrame], sizeof(uint32_t) * frustumIndex.size());

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);

    std::vector<Point> frustrumPoints(frustumVertexCount * frustumCounts[currentFrame]);
    for (uint32_t i = 0; i < camManager.getCamCount(); i++) {
        CamArrayData camData = camManager.camArray[i].getCamData();
        uint32_t camOffset = frustumVertexCount * i;

        frustrumPoints[camOffset + 0].pos = {intersectPlanes(camData.frustumPlanes[4], camData.frustumPlanes[0], camData.frustumPlanes[3]), 1.0f}; // near, left, top
        frustrumPoints[camOffset + 1].pos = {intersectPlanes(camData.frustumPlanes[4], camData.frustumPlanes[0], camData.frustumPlanes[2]), 1.0f}; // near, left, bottom

        frustrumPoints[camOffset + 2].pos = {intersectPlanes(camData.frustumPlanes[4], camData.frustumPlanes[1], camData.frustumPlanes[3]), 1.0f}; // near, right, top
        frustrumPoints[camOffset + 3].pos = {intersectPlanes(camData.frustumPlanes[4], camData.frustumPlanes[1], camData.frustumPlanes[2]), 1.0f}; // near, right, bottom

        frustrumPoints[camOffset + 4].pos = {intersectPlanes(camData.frustumPlanes[5], camData.frustumPlanes[0], camData.frustumPlanes[3]), 1.0f}; // far, left, top
        frustrumPoints[camOffset + 5].pos = {intersectPlanes(camData.frustumPlanes[5], camData.frustumPlanes[0], camData.frustumPlanes[2]), 1.0f}; // far, left, bottom

        frustrumPoints[camOffset + 6].pos = {intersectPlanes(camData.frustumPlanes[5], camData.frustumPlanes[1], camData.frustumPlanes[3]), 1.0f}; // far, right, top
        frustrumPoints[camOffset + 7].pos = {intersectPlanes(camData.frustumPlanes[5], camData.frustumPlanes[1], camData.frustumPlanes[2]), 1.0f}; // far, right, bottom

        // near plane (z = near)
        frustrumPoints[camOffset + 0].col = glm::vec4(1.0f, 0.0f, 0.0f, 0.3f); // top-left near - red
        frustrumPoints[camOffset + 1].col = glm::vec4(0.0f, 1.0f, 0.0f, 0.3f); // bottom-left near - green
        frustrumPoints[camOffset + 2].col = glm::vec4(0.0f, 0.0f, 1.0f, 0.3f); // top-right near - blue
        frustrumPoints[camOffset + 3].col = glm::vec4(1.0f, 1.0f, 0.0f, 0.3f); // bottom-right near - yellow

        // far plane (z = far)
        frustrumPoints[camOffset + 4].col = glm::vec4(1.0f, 0.5f, 0.5f, 0.3f); // top-left far - light red
        frustrumPoints[camOffset + 5].col = glm::vec4(0.5f, 1.0f, 0.5f, 0.3f); // bottom-left far - light green
        frustrumPoints[camOffset + 6].col = glm::vec4(0.5f, 0.5f, 1.0f, 0.3f); // top-right far - light blue
        frustrumPoints[camOffset + 7].col = glm::vec4(1.0f, 1.0f, 0.5f, 0.3f); // bottom-right far - light yellow
    }

    allocInfo = memManager.createBuffer(sizeof(Vertex) * frustrumPoints.size(),
                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_SHARING_MODE_EXCLUSIVE,
                                        stagingBuffer,
                                        VMA_MEMORY_USAGE_CPU_TO_GPU,
                                        stagingBufferMemory,
                                        frustrumPoints.data());
    memManager.copyBuffer(stagingBuffer, frustumVertexBuffers[currentFrame], sizeof(Vertex) * frustrumPoints.size());

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}