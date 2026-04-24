#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE     // GLM uses openGl depth range -1 to 1

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

/**
 * @struct Material
 * @brief Stores material properties parsed from OBJ materials.
 *
 * Includes the diffuse color, lighting properties, and optional diffuse texture file.
 */
struct Material {
    std::string name;
    glm::vec4 diffuseColor;
    RenderMaterialObject lightProp;
    std::string textureFile;
    bool isTransparent = false;

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

/**
 * @struct SubMesh
 * @brief Defines a range of indices for a material subset of the mesh.
 */
struct SubMesh {
    uint32_t indexCount;
    uint32_t indexOffset;
    int textureID;
    uint32_t materialID;

    bool hasTexture() const {
        return textureID != -1;
    }

    void print() const {
        std::cout << "Texture: " << textureID << std::endl;
        std::cout << "  Index offset: " << indexOffset << std::endl;
        std::cout << "  Index count: " << indexCount << std::endl;
    }
};

/**
 * @class Mesh
 * @brief Loads OBJ geometry and creates GPU-side buffers for rendering.
 *
 * Handles vertex/index buffer creation, material loading, and optional texture binding.
 */
class Mesh{
public:
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkBuffer materialBuffer = VK_NULL_HANDLE;

    float scale = 1.0f;

    /**
     * @brief Constructs a mesh by loading an OBJ file and creating Vulkan resources.
     * @param device Logical Vulkan device.
     * @param memManager Memory manager used for buffer and image allocation.
     * @param prop Physical device properties used for sampler creation.
     */
    Mesh(const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop);
    ~Mesh();

    /**
     * @brief Reloads a different model into this Mesh instance.
     * @param modelIndex Index of the model to load.
     */
    void changeModel(const uint32_t modelIndex);

    uint32_t getIndicesSize();
    uint32_t getMaterialSize();
    glm::vec3 getLightPos();

    const VkSampler getSampler();
    const std::vector<VkImageView> getMaterialViews();
    const std::vector<SubMesh> getSubMeshes();
    const std::vector<SubMesh> getTransparentSubMeshes();
    const MeshUniforms getMeshUniforms();

    glm::vec3 getLight();
    glm::vec3& getLightRef();

    const std::vector<std::string>& getLoadedModels() const;
    uint32_t getModelCount() const;

    void setLight(glm::vec3 newPos);
private:
    VmaAllocation vertexBufferMemory;
    VmaAllocation indexBufferMemory;
    VmaAllocation materialBufferMemory;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::vector<SubMesh> meshes;
    std::vector<SubMesh> meshesT;                       // transparent meshes
    std::vector<RenderMaterialObject> lightProps;
    std::vector<Texture> textures;
    Sampler sampler;

    glm::vec3 lightPosition = glm::vec3(0.0f, 3.0f, 7.0f);

    std::vector<std::string> loadedModels;

    // Vulkan handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void loadMesh(const std::string& modelPath);
    Vertex loadVertex(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index, const glm::vec4& matColor) const;
    std::vector<Material> loadMaterials(const std::vector<tinyobj::material_t>& materialsTiny) const;
    void createVertexBuffer();
    void createIndexBuffer();
    void createMaterialBuffer();

    void loadModels();

    void clearResources();
    void createDummyTexture();
};