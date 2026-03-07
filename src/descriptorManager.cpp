#include <descriptorManager.hpp>
#include <structs.hpp>
#include <textures.hpp>
#include <camManager.hpp>
#include <uniforms.hpp>

static const uint32_t MAX_MATERIALS_COUNT = 60;
static const uint32_t SETS_COUNT = MAX_MATERIALS_COUNT + 7;   // 2xMAX_MATERIAL_COUNT, render, compute, shared, frustum+line, cam cube, offline, pointCloud

struct DescriptorWriter {
    static VkDescriptorBufferInfo makeBufferInfo(VkBuffer buffer, VkDeviceSize size, uint32_t offset = 0) {
        return VkDescriptorBufferInfo{ buffer, offset, size };
    }

    static VkDescriptorImageInfo makeImageInfo(VkSampler sampler, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        return VkDescriptorImageInfo{ sampler, view, layout };
    }

    static VkWriteDescriptorSet makeWrite(VkDescriptorSet dstSet,
                                          uint32_t binding,
                                          VkDescriptorType type,
                                          VkDescriptorBufferInfo* bufferInfo = nullptr,
                                          VkDescriptorImageInfo* imageInfo = nullptr,
                                          uint32_t count = 1) {
        return VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = dstSet,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = count,
            .descriptorType = type,
            .pImageInfo = imageInfo,
            .pBufferInfo = bufferInfo,
            .pTexelBufferView = nullptr
        };
    }
};



DescriptorManager::DescriptorManager(const VkDevice device)
    : builder(device),
      deviceHandle(device),
      renderDescriptors(builder, device),
      computeDescriptors(builder, device),
      frustumDescriptors(builder, device), 
      camCubeDestriptors(builder, device),
      offlineDescriptors(builder, device),
      pointCloudDescriptors(builder, device) {

}

DescriptorManager::~DescriptorManager() {
    vkDestroyDescriptorPool(deviceHandle, descriptorPool, nullptr);
}

void DescriptorManager::createDescriptorPoolSet(const UniformManager& uniformManager,
                                                const MeshUniforms& meshData,
                                                const TextureSamplerView& cubeSamplerView, 
                                                CamerasManager& camManager) {
    createDescriptorPool();
    builder.setDescriptorPool(descriptorPool);

    renderDescriptors.createDescriptorSets(builder, uniformManager.renderUniforms, meshData);
    computeDescriptors.createDescriptorSets(builder, uniformManager.novelUnifroms, camManager);

    // Debug utils
    frustumDescriptors.createDescriptorSets(builder, uniformManager.renderUniforms.uniformBuffers);
    camCubeDestriptors.createDescriptorSets(builder, cubeSamplerView);
    offlineDescriptors.createDescriptorSets(builder, uniformManager.offlineUniforms.uniformBuffers, camManager);
    pointCloudDescriptors.createDescriptorSets(builder, uniformManager.pointCloudUniforms);
}

void DescriptorManager::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0] = {
        .type  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // max model material, render 2xubo, compute ubo, line + frustum ubo, offline ubo, pointCloud
        .descriptorCount = 6*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };
    poolSizes[1] = {
        .type  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // max model texture, cam cube, offline image, 2 x shared
        .descriptorCount = 2*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) + 2 + MAX_MATERIALS_COUNT
    };
    poolSizes[2] = {
        .type  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 3x compute ssbo, 3x pointCloud ssbo, render materials
        .descriptorCount = 7*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };
    poolSizes[3] = {
        .type  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // novel view
        .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = SETS_COUNT*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    
    if (vkCreateDescriptorPool(deviceHandle, &descriptorPoolCI, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}



DescriptorBuilder::DescriptorBuilder(VkDevice device)
    : deviceHandle(device) {

}

DescriptorBuilder::~DescriptorBuilder() {
    
}

void DescriptorBuilder::setDescriptorPool(VkDescriptorPool descriptorPool) {
    descriptorPoolRef = descriptorPool;
}

void DescriptorBuilder::addDescriptorSetLayoutBinding(std::vector<VkDescriptorSetLayoutBinding>& inVector,
                                                      uint32_t bind,
                                                      VkDescriptorType type, 
                                                      VkShaderStageFlags stage,
                                                      uint32_t count) const {
    VkDescriptorSetLayoutBinding descriptorSLB{
        .binding = bind,
        .descriptorType = type,
        .descriptorCount = count,
        .stageFlags = stage,
        .pImmutableSamplers = nullptr,
    };

    inVector.push_back(descriptorSLB);
}

VkDescriptorSetLayout DescriptorBuilder::buildDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>& inVector) const {
    VkDescriptorSetLayoutCreateInfo descriptorSLCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(inVector.size()),
        .pBindings = inVector.data(),
    };
    VkDescriptorSetLayout returnLayout;

    if (vkCreateDescriptorSetLayout(deviceHandle, &descriptorSLCI, nullptr, &returnLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    return returnLayout;
}

void DescriptorBuilder::buildDescriptorSets(std::vector<VkDescriptorSet>& outDescriptorSets,
                                            VkDescriptorSetLayout descriptorSetLayout,
                                            uint32_t count) const {
    if (descriptorPoolRef == VK_NULL_HANDLE) {
        throw std::runtime_error("Descriptor pool was not initialized!");
    }
    if (outDescriptorSets.size() < count) {
        throw std::runtime_error("Not enough space in descriptor sets!");
    }

    std::vector<VkDescriptorSetLayout> layoutSets;
    for (uint32_t i = 0; i < count; i++) {
        layoutSets.emplace_back(descriptorSetLayout);
    }

    VkDescriptorSetAllocateInfo descriptorSetAI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPoolRef,
        .descriptorSetCount = static_cast<uint32_t>(layoutSets.size()),
        .pSetLayouts = layoutSets.data(),
    };

    if (vkAllocateDescriptorSets(deviceHandle, &descriptorSetAI, outDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }
}



BaseDescriptors::BaseDescriptors(const VkDevice device)
        : deviceHandle(device) {
}

BaseDescriptors::~BaseDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayout, nullptr);
}



FrustumDescriptors::FrustumDescriptors(const DescriptorBuilder& builder, const VkDevice device) 
        : BaseDescriptors(device) {
    createDescriptorSetLayout(builder);
}

void FrustumDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;

    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void FrustumDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& uniformBuffers) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(uniformBuffers[i], sizeof(MVPBufferObject));
        VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }
}



CamCubeDescriptors::CamCubeDescriptors(const DescriptorBuilder& builder, const VkDevice device)
        : BaseDescriptors(device)  {
    createDescriptorSetLayout(builder);
}

void CamCubeDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;

    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void CamCubeDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const TextureSamplerView& textureSamplerView) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(textureSamplerView.textureSampler, textureSamplerView.textureImageView);
        VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(descriptorSets[i],
                                                                            0,
                                                                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                            nullptr,
                                                                            &descriptorImageI);

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }
}



OfflineDescriptors::OfflineDescriptors(const DescriptorBuilder& builder, const VkDevice device)
        : BaseDescriptors(device)  {
    createDescriptorSetLayout(builder);
}

void OfflineDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;

    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void OfflineDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& offlineRenderBuffers, CamerasManager& camManager) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(offlineRenderBuffers[i], sizeof(OfflineBufferObject));

        // bind to dummy at the start
        VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(camManager.getSampler(ImageViewType::COLOR), camManager.novelView.getImageView(i));

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);
        descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &descriptorImageI);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}

void OfflineDescriptors::updateDescriptorSets(CamerasManager& camManager) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(camManager.getSampler(ImageViewType::COLOR),      // the same sampler as for the samplerArray
                                                                             camManager.novelView.getImageView(i));

        VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(descriptorSets[i],
                                                                            1,
                                                                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                            nullptr,
                                                                            &descriptorImageI);

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }
}




RenderDescriptors::RenderDescriptors(const DescriptorBuilder& builder, const VkDevice device)
        : BaseDescriptors(device)  {
    createDescriptorSetLayout(builder);
}

RenderDescriptors::~RenderDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, samplerDescriptorSetLayout, nullptr);
    // vkDestroyDescriptorSetLayout(deviceHandle, materialDescriptorSetLayout, nullptr);
}

void RenderDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;

    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);

    descriptorSLBs.clear();
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    samplerDescriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);

    // descriptorSLBs.clear();
    // builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    // materialDescriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void RenderDescriptors::createDescriptorSets(const DescriptorBuilder& builder,
                                             const RenderUniforms& renderUniforms,
                                             const MeshUniforms& meshData) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(renderUniforms.fragmentUniformBuffers[i], sizeof(RenderFragmentObject));
        VkDescriptorBufferInfo descriptorBufferI1 = DescriptorWriter::makeBufferInfo(renderUniforms.uniformBuffers[i], sizeof(glm::mat4));
        VkDescriptorBufferInfo descriptorBufferI2 = DescriptorWriter::makeBufferInfo(meshData.materials, VK_WHOLE_SIZE);
        std::array<VkWriteDescriptorSet, 3> descriptorWrites;
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);
        descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[i], 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI1);
        descriptorWrites[2] = DescriptorWriter::makeWrite(descriptorSets[i], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferI2);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    // std::cout << meshData.materialCount << std::endl;
    createSamplerDescriptorSets(builder, meshData.sampler, meshData.textures);
    // createMaterialDescriptorSets(builder, meshData.materialCount, meshData.materials);
}

void RenderDescriptors::updateMaterialDescriptosSets(VkBuffer materials) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI2 = DescriptorWriter::makeBufferInfo(materials, VK_WHOLE_SIZE);
        std::array<VkWriteDescriptorSet, 1> descriptorWrites;
        descriptorWrites[2] = DescriptorWriter::makeWrite(descriptorSets[i], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferI2);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}

void RenderDescriptors::createSamplerDescriptorSets(const DescriptorBuilder& builder, const VkSampler& sampler, const std::vector<VkImageView>& textures) {
    samplerDescriptorSets.resize(MAX_MATERIALS_COUNT);      // number of materials
    builder.buildDescriptorSets(samplerDescriptorSets, samplerDescriptorSetLayout, MAX_MATERIALS_COUNT);

    updateSamplerDescriptorSets(sampler, textures);
}

void RenderDescriptors::updateSamplerDescriptorSets(const VkSampler& sampler, const std::vector<VkImageView>& textures) {
    uint32_t matCount = textures.size();
    if (matCount > MAX_MATERIALS_COUNT) {
        throw std::runtime_error("Needs to allocate more textures " + std::to_string(matCount) + "!");
    }

    for (uint32_t materialID = 0; materialID < matCount; materialID++) {
        VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(sampler, textures[materialID]);
        VkWriteDescriptorSet descriptorWrite = DescriptorWriter::makeWrite(samplerDescriptorSets[materialID],
                                                                            0,
                                                                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                            nullptr,
                                                                            &descriptorImageI);

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrite, 0, nullptr);
    }
}

void RenderDescriptors::updateMeshData(const MeshUniforms& meshData) {
    updateMaterialDescriptosSets(meshData.materials);
    updateSamplerDescriptorSets(meshData.sampler, meshData.textures);
}

// void RenderDescriptors::createMaterialDescriptorSets(const DescriptorBuilder& builder, const uint32_t materialCount, VkBuffer materialBuffer) {
//     materialDescriptorSets.resize(MAX_MATERIALS_COUNT);         // number of materials
//     builder.buildDescriptorSets(materialDescriptorSets, materialDescriptorSetLayout, MAX_MATERIALS_COUNT);

//     std::cout << materialCount << std::endl;
//     updateMaterialDescriptorSets(materialCount, materialBuffer);
// }

// void RenderDescriptors::updateMaterialDescriptorSets(const uint32_t materialCount, VkBuffer materialBuffer) {
//     std::cout << "Final: " << materialCount << std::endl;
//     if (materialCount > MAX_MATERIALS_COUNT) {
//         throw std::runtime_error("Needs to allocate more materials " + std::to_string(materialCount) + "!");
//     }

//     for (uint32_t materialID = 0; materialID < materialCount; materialID++) {
//         VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(materialBuffer, sizeof(RenderMaterialObject), materialID * sizeof(RenderMaterialObject));
//         VkWriteDescriptorSet descriptorWrite = DescriptorWriter::makeWrite(materialDescriptorSets[materialID], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);

//         vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrite, 0, nullptr);
//     }
// }


ComputeDescriptors::ComputeDescriptors(const DescriptorBuilder& builder, const VkDevice device)
        : BaseDescriptors(device)  {
    createDescriptorSetLayout(builder);
}

ComputeDescriptors::~ComputeDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, sharedDescriptorSetLayout, nullptr);
}

void ComputeDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;
    descriptorSLBs.reserve(5);

    VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    for (uint32_t i = 0; i < descriptorSLBs.capacity(); i++) {      // use capacity instead of size
        if (i == 0) {
            descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        else if (i == 4) {
            descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        else {
            descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }

        builder.addDescriptorSetLayoutBinding(descriptorSLBs, i, descriptorType, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);

    // shared descritpor set
    descriptorSLBs.clear();
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    sharedDescriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void ComputeDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const NovelUniforms& novelUnifroms, CamerasManager& camManager) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(novelUnifroms.uniformBuffers[i], sizeof(NovelBufferObject));

        VkDescriptorBufferInfo descriptorStorageBufferI1 = DescriptorWriter::makeBufferInfo(novelUnifroms.camArraySSBOIn[i], VK_WHOLE_SIZE);
        VkDescriptorBufferInfo descriptorStorageBufferI2 = DescriptorWriter::makeBufferInfo(novelUnifroms.intersectionsSSBOOut[i], VK_WHOLE_SIZE);
        VkDescriptorBufferInfo descriptorStorageBufferI3 = DescriptorWriter::makeBufferInfo(novelUnifroms.intersectCountSSBOOut[i], sizeof(uint32_t));

        // uses a dummy resource until the reconstruction is set to go
        VkDescriptorImageInfo descriptorImageStorageI = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE,
                                                                                        camManager.novelView.getImageView(i),      // novel image per frame
                                                                                        VK_IMAGE_LAYOUT_GENERAL);

        std::array<VkWriteDescriptorSet, 5> descriptorWrites{};
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);
        descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[i], 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI1);
        descriptorWrites[2] = DescriptorWriter::makeWrite(descriptorSets[i], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI2);
        descriptorWrites[3] = DescriptorWriter::makeWrite(descriptorSets[i], 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI3);
        descriptorWrites[4] = DescriptorWriter::makeWrite(descriptorSets[i], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &descriptorImageStorageI);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    std::vector<VkDescriptorSet> tmpSet;
    tmpSet.resize(1);
    builder.buildDescriptorSets(tmpSet, sharedDescriptorSetLayout, 1);
    sharedDescriptorSet = tmpSet[0];            // shared descriptor set for reference cam samplers

    // ready to go from the start
    updateSharedImageDescriptor(camManager);
}

void ComputeDescriptors::updateDescriptorSets(uint32_t currentFrame, const std::vector<VkBuffer>& camArraySSBOIn,
                                                const std::vector<VkBuffer>& intersectionsSSBOOut) {
    std::array<VkDescriptorBufferInfo, 2> descriptorBuffers{};
    descriptorBuffers[0] = DescriptorWriter::makeBufferInfo(camArraySSBOIn[currentFrame], VK_WHOLE_SIZE);
    descriptorBuffers[1] = DescriptorWriter::makeBufferInfo(intersectionsSSBOOut[currentFrame], VK_WHOLE_SIZE);

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[currentFrame], 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBuffers[0]);
    descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[currentFrame], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBuffers[1]);

    vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}

void ComputeDescriptors::updateSharedImageDescriptor(CamerasManager& camManager) {
    VkDescriptorImageInfo descriptorImageSamplerI = DescriptorWriter::makeImageInfo(camManager.getSampler(ImageViewType::COLOR), camManager.getImageView(ImageViewType::COLOR));
    VkDescriptorImageInfo descriptorImageSamplerI2 = DescriptorWriter::makeImageInfo(camManager.getSampler(ImageViewType::DEPTH), camManager.getImageView(ImageViewType::DEPTH));
    std::array<VkWriteDescriptorSet, 2> descriptorWritesShared;
    descriptorWritesShared[0] = DescriptorWriter::makeWrite(sharedDescriptorSet,
                                                            0,
                                                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                            nullptr,
                                                            &descriptorImageSamplerI);
    descriptorWritesShared[1] = DescriptorWriter::makeWrite(sharedDescriptorSet,
                                                            1,
                                                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                            nullptr,
                                                            &descriptorImageSamplerI2);
                                                            

    vkUpdateDescriptorSets(deviceHandle, descriptorWritesShared.size(), descriptorWritesShared.data(), 0, nullptr);
}

void ComputeDescriptors::updateImageDescriptors(CamerasManager& camManager) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorImageStorageI = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE,
                                                                                        camManager.novelView.getImageView(i),      // stuck with the current resolution
                                                                                        VK_IMAGE_LAYOUT_GENERAL);
        VkWriteDescriptorSet descriptorWrite = DescriptorWriter::makeWrite(descriptorSets[i], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &descriptorImageStorageI);

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrite, 0, nullptr);
    }

    // updateSharedImageDescriptor(camManager);
}

PointCloudDescriptors::PointCloudDescriptors(const DescriptorBuilder& builder, const VkDevice device)
        : BaseDescriptors(device) {
    createDescriptorSetLayout(builder);
}

void PointCloudDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;
    descriptorSLBs.reserve(4);

    VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    for (uint32_t i = 0; i < descriptorSLBs.capacity(); i++) {      // use capacity instead of size
        if (i == 0) {
            descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        else {
            descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }

        builder.addDescriptorSetLayoutBinding(descriptorSLBs, i, descriptorType, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void PointCloudDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const PointCloudUniforms& pointCloudUniforms) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(pointCloudUniforms.uniformBuffers[i], sizeof(PointCloudObject));

        VkDescriptorBufferInfo descriptorStorageBufferI1 = DescriptorWriter::makeBufferInfo(pointCloudUniforms.camMatricesSSBOIn[i], VK_WHOLE_SIZE);
        VkDescriptorBufferInfo descriptorStorageBufferI2 = DescriptorWriter::makeBufferInfo(pointCloudUniforms.pointCloudSSBOOut[i], VK_WHOLE_SIZE);
        VkDescriptorBufferInfo descriptorStorageBufferI3 = DescriptorWriter::makeBufferInfo(pointCloudUniforms.pointCountSSBOOut[i], sizeof(uint32_t));

        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);
        descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[i], 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI1);
        descriptorWrites[2] = DescriptorWriter::makeWrite(descriptorSets[i], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI2);
        descriptorWrites[3] = DescriptorWriter::makeWrite(descriptorSets[i], 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI3);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}

void PointCloudDescriptors::updateDescriptorSets(const PointCloudUniforms& pointCloudUniforms) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorStorageBufferI1 = DescriptorWriter::makeBufferInfo(pointCloudUniforms.camMatricesSSBOIn[i], VK_WHOLE_SIZE);
        VkDescriptorBufferInfo descriptorStorageBufferI2 = DescriptorWriter::makeBufferInfo(pointCloudUniforms.pointCloudSSBOOut[i], VK_WHOLE_SIZE);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI1);
        descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[i], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI2);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}