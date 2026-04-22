#include <mesh.hpp>
#include <vertex.hpp>
#include <memManager.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

static const std::string dummyTexture = "dummyTexture.png";         // needs to bind to descritpors
static const std::string RESOURCE_DIR = "../resources/models/";
static const uint32_t MAX_MATERIALS_COUNT = 120;                     // arbitrary limit for textures descritpors

Mesh::Mesh(const VkDevice device, MemoryManager& memManager, const VkPhysicalDeviceProperties& prop)
    : deviceHandle(device), memManager(memManager), sampler(device, prop, VK_SAMPLER_ADDRESS_MODE_REPEAT) {
    textures.reserve(MAX_MATERIALS_COUNT);
    loadModels();
    if (loadedModels.empty()) {
        throw std::runtime_error("No models found in resources/models!");
    }

    std::cout << "Loading model: " << loadedModels[0] << std::endl;
    changeModel(0);   // load first model by default
    std::cout << "Mesh created" << std::endl;
}

Mesh::~Mesh() {
    if (vertexBuffer != VK_NULL_HANDLE) {
        memManager.destroyBuffer(vertexBuffer, vertexBufferMemory);
        vertexBuffer = VK_NULL_HANDLE;
    }

    if (indexBuffer != VK_NULL_HANDLE) {
        memManager.destroyBuffer(indexBuffer, indexBufferMemory);
        indexBuffer = VK_NULL_HANDLE;
    }

    if (materialBuffer != VK_NULL_HANDLE) {
        memManager.destroyBuffer(materialBuffer, materialBufferMemory);
        materialBuffer = VK_NULL_HANDLE;
    }
}

void Mesh::loadModels() {
    for (const auto& entry : std::filesystem::directory_iterator(RESOURCE_DIR)) {
        if (entry.is_regular_file() && entry.path().extension() == ".obj") {       // get .obj files only
            loadedModels.push_back(entry.path().stem().string());
        }
    }
}

const std::vector<std::string>& Mesh::getLoadedModels() const {
    return loadedModels;
}

uint32_t Mesh::getModelCount() const {
    return static_cast<uint32_t>(loadedModels.size());
}

void Mesh::changeModel(const uint32_t modelIndex) {
    if (modelIndex >= loadedModels.size()) {
        throw std::runtime_error("Model index out of range!");
    }

    loadMesh(RESOURCE_DIR + loadedModels[modelIndex] + ".obj");
    createDummyTexture();
    createVertexBuffer();
    createIndexBuffer();
    createMaterialBuffer();
}

void Mesh::createDummyTexture() {
    if (textures.empty()) {
        textures.emplace_back(dummyTexture, deviceHandle, memManager);
    }
}

uint32_t Mesh::getIndicesSize() {
    return static_cast<uint32_t>(indices.size());
}

uint32_t Mesh::getMaterialSize() {
    return static_cast<uint32_t>(lightProps.size());
}

glm::vec3 Mesh::getLightPos() {
    return lightPosition;
}

const VkSampler Mesh::getSampler() {
    return sampler.getSampler();
}

const std::vector<VkImageView> Mesh::getMaterialViews() {
    std::vector<VkImageView> materialViews;
    materialViews.reserve(textures.size());

    for (const auto& tex: textures) {
        materialViews.emplace_back(tex.textureImageView);
    }
    return materialViews;
}

const std::vector<SubMesh> Mesh::getSubMeshes() {
    return meshes;
}

const std::vector<SubMesh> Mesh::getTransparentSubMeshes() {
    return meshesT;
}

const MeshUniforms Mesh::getMeshUniforms() {
    return {sampler.getSampler(),
            getMaterialViews(),
            materialBuffer};
}

glm::vec3 Mesh::getLight() {
    return lightPosition;
}

glm::vec3& Mesh::getLightRef() {
    return lightPosition;
}

void Mesh::clearResources() {
    if (vertexBuffer != VK_NULL_HANDLE) {
        memManager.destroyBuffer(vertexBuffer, vertexBufferMemory);
        vertexBuffer = VK_NULL_HANDLE;
    }

    if (indexBuffer != VK_NULL_HANDLE) {
        memManager.destroyBuffer(indexBuffer, indexBufferMemory);
        indexBuffer = VK_NULL_HANDLE;
    }

    if (materialBuffer != VK_NULL_HANDLE) {
        memManager.destroyBuffer(materialBuffer, materialBufferMemory);
        materialBuffer = VK_NULL_HANDLE;
    }

    vertices.clear();
    indices.clear();
    meshes.clear();
    meshesT.clear();
    textures.clear();
    lightProps.clear();
}

void Mesh::loadMesh(const std::string& modelFile) {
    clearResources();

    tinyobj::attrib_t attrib;                       // Vertex data
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materialsTiny;
    std::string warn;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materialsTiny, &warn, &err, modelFile.c_str(), RESOURCE_DIR.c_str(), true)) {
        if (!warn.empty()) {
            std::cerr << warn << std::endl;
        }
        throw std::runtime_error(err);
    }

    std::vector<Material> materials = loadMaterials(materialsTiny);         // load all materials
    std::vector<std::vector<uint32_t>> submeshIndices;
    submeshIndices.resize(materials.size());

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
        uint32_t indexOffset = 0;
        const auto& indexVec = shape.mesh.indices;

        for (uint32_t face = 0; face < shape.mesh.num_face_vertices.size(); face++) {        // points to indices -> indices points to vertex
            int indexCount = shape.mesh.num_face_vertices[face];           // should be 3, triangulate
            int materialID = shape.mesh.material_ids[face];
            if (materialID < 0 || materialID >= materials.size()) {         // missing materials?
                throw std::runtime_error("Material out of range!");
            }

            glm::vec4 matColor = materials[materialID].diffuseColor;

            for (uint32_t indexID = 0; indexID < indexCount; indexID++){
                tinyobj::index_t index = shape.mesh.indices[indexOffset + indexID];
                Vertex vertex = loadVertex(attrib, index, matColor);

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                
                submeshIndices[materialID].push_back(uniqueVertices[vertex]);
            }

            indexOffset += indexCount;       // should be just factor of 3
        }
    }

    std::cout << "Loaded " << vertices.size()  << " vertices."  << std::endl;
    uint32_t totalIndices = 0;
    for (const auto& submesh: submeshIndices) {
        totalIndices += submesh.size();
    }
    indices.reserve(totalIndices);

    meshes.reserve(materials.size());
    meshesT.reserve(materials.size());
    lightProps.reserve(materials.size());
    for (uint32_t i = 0; i < submeshIndices.size(); i++) {
        const auto& subMesh = submeshIndices[i];

        if (subMesh.empty()){
            continue;
        }

        int textureId = -1;
        if (materials[i].hasTexture()) {
            textureId = textures.size();
            textures.emplace_back(materials[i].textureFile, deviceHandle, memManager);
            // materials[i].isTransparent |= textures[textureId].isTransparent();
        }
        lightProps.push_back(materials[i].lightProp);

        if (materials[i].isTransparent) {
            std::cout << "Got transparent material " << materials[i].name << std::endl;
            meshesT.emplace_back(SubMesh{ static_cast<uint32_t>(subMesh.size()),
                                     static_cast<uint32_t>(indices.size()),
                                     textureId,
                                     i });      // material index
        }
        else {
            meshes.emplace_back(SubMesh{ static_cast<uint32_t>(subMesh.size()),
                                     static_cast<uint32_t>(indices.size()),
                                     textureId,
                                     i });
        }
        // meshes[meshes.size()-1].print();
        indices.insert(indices.end(), subMesh.begin(), subMesh.end());
    }
}

Vertex Mesh::loadVertex(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index, const glm::vec4& matColor) const {
    Vertex vertex{};

    vertex.pos = {
        attrib.vertices[3*index.vertex_index + 0],
        attrib.vertices[3*index.vertex_index + 1],
        attrib.vertices[3*index.vertex_index + 2]
    };

    vertex.texCoords = glm::vec2(0.0f);
    if (index.texcoord_index >= 0) {
        vertex.texCoords = { attrib.texcoords[2*index.texcoord_index + 0],
                             1.0f - attrib.texcoords[2*index.texcoord_index + 1] };
    }

    vertex.color = matColor;

    if (index.normal_index >= 0) {
        vertex.normal = {
            attrib.normals[3*index.normal_index + 0],
            attrib.normals[3*index.normal_index + 1],
            attrib.normals[3*index.normal_index + 2],
        };
    }
    else {
       vertex.normal = glm::vec3(-1.0f);
    }

    return vertex;
}

std::vector<Material> Mesh::loadMaterials(const std::vector<tinyobj::material_t>& materialsTiny) const {
    std::vector<Material> materials;
    materials.reserve(materialsTiny.size());

    std::cout << "Material count: " << materialsTiny.size() << std::endl;
    if (materialsTiny.size() > MAX_MATERIALS_COUNT) {
        throw std::runtime_error("Too many materials to load!");
    }

    for (uint32_t i = 0; i < materialsTiny.size(); i++) {
        Material material{};

        material.name = materialsTiny[i].name;
        material.diffuseColor = glm::vec4(materialsTiny[i].diffuse[0],
                                          materialsTiny[i].diffuse[1],
                                          materialsTiny[i].diffuse[2],
                                          materialsTiny[i].dissolve);
        material.textureFile = materialsTiny[i].diffuse_texname;
        material.isTransparent = materialsTiny[i].dissolve < 1.0f;

        RenderMaterialObject prop{};
        prop.ambient = glm::vec3(materialsTiny[i].ambient[0],
                                 materialsTiny[i].ambient[1],
                                 materialsTiny[i].ambient[2]);
        prop.specular = glm::vec3(materialsTiny[i].specular[0],
                                  materialsTiny[i].specular[1],
                                  materialsTiny[i].specular[2]);
        prop.shininess = materialsTiny[i].shininess;
        material.lightProp = prop;

        materials.push_back(material);
        // material.print();
    }

    return materials;
}

void Mesh::createVertexBuffer() {
    VkDeviceSize vertexSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMem;

    memManager.createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBufferMem, vertices.data());
    memManager.createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, vertexBuffer, VMA_MEMORY_USAGE_GPU_ONLY, vertexBufferMemory);

    memManager.copyBuffer(stagingBuffer, vertexBuffer, vertexSize);

    memManager.destroyBuffer(stagingBuffer, stagingBufferMem);
}

void Mesh::createIndexBuffer() {
    VkDeviceSize bufferSize = indices.size() * sizeof(indices[0]);
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;

    memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, 
            VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBufferMemory, indices.data());
    memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, indexBuffer, VMA_MEMORY_USAGE_GPU_ONLY, indexBufferMemory);

    memManager.copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}

void Mesh::createMaterialBuffer() {
    VkDeviceSize bufferSize = lightProps.size() * sizeof(RenderMaterialObject);
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferMemory;

    memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU,
                            stagingBufferMemory, lightProps.data());
    memManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, materialBuffer, VMA_MEMORY_USAGE_GPU_ONLY, materialBufferMemory);

    memManager.copyBuffer(stagingBuffer, materialBuffer, bufferSize);

    memManager.destroyBuffer(stagingBuffer, stagingBufferMemory);
}

 void Mesh::setLight(glm::vec3 newPos) {
    lightPosition = newPos;
 }