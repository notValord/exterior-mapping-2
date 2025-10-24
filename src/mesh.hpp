#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>

struct Vertex;

class MemoryManager;

class Mesh{
public:
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;

    Mesh(const std::string& modelPath, const VkDevice& device, MemoryManager& memManager);
    ~Mesh();

    uint32_t getIndicesSize();
private:
    VkDeviceMemory vertexBufferMemory;
    VkDeviceMemory indexBufferMemory;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // vulkan handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void loadMesh(const std::string& modelPath);
    void createVertexBuffer();
    void createIndexBuffer();
};