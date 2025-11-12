#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

struct Vertex;

class MemoryManager;

class Mesh{
public:
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;

    Mesh(const std::string& modelPath, const VkDevice device, MemoryManager& memManager);
    ~Mesh();

    uint32_t getIndicesSize();
private:
    VmaAllocation vertexBufferMemory;
    VmaAllocation indexBufferMemory;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // vulkan handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void loadMesh(const std::string& modelPath);
    void createVertexBuffer();
    void createIndexBuffer();
};