#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <vector>
#include <iostream>

#include <textures.hpp>
#include <structs.hpp>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

struct Vertex;
struct RenderMaterialObject;
namespace tinyobj {
    struct material_t;
    struct attrib_t;
    struct index_t;
}

class MemoryManager;

struct Material {
    std::string name;
    glm::vec4 diffuseColor;
    RenderMaterialObject lightProp;
    std::string textureFile;

    void print() const {
        std::cout << "Material " << ": " << name << std::endl;
        std::cout << "  Diffuse color: " << diffuseColor.x << ", " << diffuseColor.y << ", " << diffuseColor.z << ", " << diffuseColor.w << std::endl;
        std::cout << "  Ambient color: " << lightProp.ambient.x << ", " << lightProp.ambient.y << ", " << lightProp.ambient.z << std::endl;
        std::cout << "  Specular color: " << lightProp.specular.x << ", " << lightProp.specular.y << ", " << lightProp.specular.z << std::endl;
        std::cout << "  Shiny: " << lightProp.shininess << std::endl;
        std::cout << "  Diffuse texture: " << textureFile << std::endl;
    }

    bool hasTexture() const {
        return !textureFile.empty();
    }
};

struct SubMesh {
    uint32_t indexCount;
    uint32_t indexOffset;
    int textureID;

    bool hasTexture() const {
        return textureID != -1;
    }

    void print() const {
        std::cout << "Texture: " << textureID << std::endl;
        std::cout << "  Index offset: " << indexOffset << std::endl;
        std::cout << "  Index count: " << indexCount << std::endl;
    }
};

class Mesh{
public:
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkBuffer materialBuffer = VK_NULL_HANDLE;

    Mesh(const std::string& modelPath, const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop);
    ~Mesh();

    void changeModel(const std::string& modelPath);

    uint32_t getIndicesSize();
    uint32_t getMaterialSize();
    glm::vec3 getLightPos();

    const VkSampler getSampler();
    const std::vector<VkImageView> getMaterialViews();
    const std::vector<SubMesh> getSubMeshes();
    const MeshUniforms getMeshUniforms();

    glm::vec3 getLight();
    glm::vec3& getLightRef();
private:
    VmaAllocation vertexBufferMemory;
    VmaAllocation indexBufferMemory;
    VmaAllocation materialBufferMemory;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::vector<SubMesh> meshes;
    std::vector<RenderMaterialObject> lightProps;       // the same index as the meshes;
    std::vector<Texture> textures;
    Sampler sampler;

    glm::vec3 lightPosition = glm::vec3(0.0f, 3.0f, 7.0f);

    // Vulkan handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void loadMesh(const std::string& modelPath);
    Vertex loadVertex(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index, const glm::vec4& matColor) const;
    std::vector<Material> loadMaterials(const std::vector<tinyobj::material_t>& materialsTiny) const;
    void createVertexBuffer();
    void createIndexBuffer();
    void createMaterialBuffer();

    void clearResources();
    void createDummyTexture();
};