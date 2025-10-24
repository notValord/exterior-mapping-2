#include "mesh.hpp"
#include "vertex.hpp"
#include "util.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

Mesh::Mesh(const std::string& modelPath, const VkDevice& device, MemoryManager& memManager)
    : deviceHandle(device), memManager(memManager) {
    loadMesh(modelPath);
    createVertexBuffer();
    createIndexBuffer();
}

Mesh::~Mesh() {
    vkDestroyBuffer(deviceHandle, vertexBuffer, nullptr);
    vkDestroyBuffer(deviceHandle, indexBuffer, nullptr);
    vkFreeMemory(deviceHandle, vertexBufferMemory, nullptr);
    vkFreeMemory(deviceHandle, indexBufferMemory, nullptr);
}

uint32_t Mesh::getIndicesSize() {
    return static_cast<uint32_t>(indices.size());
}

void Mesh::loadMesh(const std::string& modelPath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
        throw std::runtime_error(err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3*index.vertex_index + 0],
                attrib.vertices[3*index.vertex_index + 1],
                attrib.vertices[3*index.vertex_index + 2]
            };
            vertex.texCoords = {
                attrib.texcoords[2*index.texcoord_index + 0],
                1.0f - attrib.texcoords[2*index.texcoord_index + 1]
            };
            vertex.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(vertex) == 0 || true) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
    }
    std::cout << "Loaded " << vertices.size()  << " vertices."  << std::endl;
}

void Mesh::createVertexBuffer() {
    VkDeviceSize vertexSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMem;
    memManager.createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferMem);

    void* data;
    vkMapMemory(deviceHandle, stagingBufferMem, 0, vertexSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)vertexSize);
    vkUnmapMemory(deviceHandle, stagingBufferMem);

    memManager.createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE,
                    vertexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferMemory);

    memManager.copyBuffer(stagingBuffer, vertexBuffer, vertexSize);

    vkDestroyBuffer(deviceHandle, stagingBuffer, nullptr);
    vkFreeMemory(deviceHandle, stagingBufferMem, nullptr);
}

void Mesh::createIndexBuffer() {
    VkDeviceSize bufferSize = indices.size() * sizeof(indices[0]);
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBufferMemory);

    void* data;
    vkMapMemory(deviceHandle, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data , indices.data(), bufferSize);
    vkUnmapMemory(deviceHandle, stagingBufferMemory);

    memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, indexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferMemory);

    memManager.copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(deviceHandle, stagingBuffer, nullptr);
    vkFreeMemory(deviceHandle, stagingBufferMemory, nullptr);
}