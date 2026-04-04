#include <descriptorManager.hpp>
#include <structs.hpp>
#include <textures.hpp>
#include <camManager.hpp>
#include <uniforms.hpp>

static const uint32_t MAX_MATERIALS_COUNT = 60;
static const uint32_t SETS_COUNT = MAX_MATERIALS_COUNT + 13;   // 2xMAX_MATERIAL_COUNT, render, compute, shared, frustum+line, cam cube, offline, pointCloud, pointCLoudDraw, reduceCompute, 2 novelSynth, 2 novelRecon

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
      camCubeDescriptors(builder, device),
      offlineDescriptors(builder, device),
      pointCloudDescriptors(builder, device),
      rayDataDescriptors(builder, device),
      reduceDescriptors(builder, device),
      novelSynthDescriptors(builder, device),
      novelReconDescriptors(builder, device) {

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

    rayDataDescriptors.createDescriptorSets(builder, uniformManager.rayDataUniforms);
    renderDescriptors.createDescriptorSets(builder, uniformManager.renderUniforms, meshData);
    computeDescriptors.createDescriptorSets(builder, uniformManager.novelUniforms, camManager);

    // Debug utils
    frustumDescriptors.createDescriptorSets(builder, uniformManager.renderUniforms.uniformBuffers);
    camCubeDescriptors.createDescriptorSets(builder, cubeSamplerView);
    offlineDescriptors.createDescriptorSets(builder, uniformManager.offlineUniforms.uniformBuffers, camManager);
    pointCloudDescriptors.createDescriptorSets(builder, uniformManager.pointCloudUniforms, camManager.getNovelStorageViews());

    reduceDescriptors.createDescriptorSets(builder, camManager.getSampler(ImageViewType::DEPTH), camManager.getReduceDepthViews());
    novelSynthDescriptors.createDescriptorSets(builder, uniformManager.novelUniforms.camArraySSBOIn, uniformManager.novelSynthUniforms);
    novelReconDescriptors.createDescriptorSets(builder, uniformManager.novelReconUniforms, camManager);
}

void DescriptorManager::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0] = {
        .type  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // max model material, render 2xubo, compute ubo, line + frustum ubo, offline ubo, pointCloud, novelRecon
        .descriptorCount = 7*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };
    poolSizes[1] = {
        .type  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // max model texture, cam cube, offline image, 2 x shared, 1 reduce, novelRecon
        .descriptorCount = 3*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) + 3 + MAX_MATERIALS_COUNT
    };
    poolSizes[2] = {
        .type  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 3x compute ssbo, 3x pointCloud ssbo, render materials, 2x rayData, novelSyth
        .descriptorCount = 10*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };
    poolSizes[3] = {
        .type  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // novel view, metadata, depth reduce 3x, novel synth 2x, novel recon
        .descriptorCount = 5*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) + 3
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

void OfflineDescriptors::updateDescriptorSets(uint32_t currentFrame, CamerasManager& camManager) {
    VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(camManager.getSampler(ImageViewType::COLOR),      // the same sampler as for the samplerArray
                                                                            camManager.novelView.getImageView(currentFrame));

    VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(descriptorSets[currentFrame],
                                                                        1,
                                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                        nullptr,
                                                                        &descriptorImageI);

    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
}

void OfflineDescriptors::updateDescriptorSets(uint32_t currentFrame, NovelReconUniforms& reconUniform) {
    VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(reconUniform.colorSampler.getSampler(),      // the same sampler as for the samplerArray
                                                                                reconUniform.resultImageView[currentFrame]);

    VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(descriptorSets[currentFrame],
                                                                        1,
                                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                        nullptr,
                                                                        &descriptorImageI);

    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
}




RenderDescriptors::RenderDescriptors(const DescriptorBuilder& builder, const VkDevice device)
        : BaseDescriptors(device)  {
    createDescriptorSetLayout(builder);
}

RenderDescriptors::~RenderDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, samplerDescriptorSetLayout, nullptr);
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

    createSamplerDescriptorSets(builder, meshData.sampler, meshData.textures);
}

void RenderDescriptors::updateMaterialDescriptosSets(VkBuffer materials) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI2 = DescriptorWriter::makeBufferInfo(materials, VK_WHOLE_SIZE);
        std::array<VkWriteDescriptorSet, 1> descriptorWrites;
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferI2);

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

PointCloudDescriptors::~PointCloudDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, imageDescriptorSetLayout, nullptr);
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

    descriptorSLBs.clear();
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
    imageDescriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void PointCloudDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const PointCloudUniforms& pointCloudUniforms, const std::vector<VkImageView>& metadataImageViews) {
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

    createImageDescriptorSets(builder, metadataImageViews);
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

void PointCloudDescriptors::createImageDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkImageView>& metadataImageViews) {
    imageDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(imageDescriptorSets, imageDescriptorSetLayout);

    updateImageDescriptorSets(metadataImageViews);
}

void PointCloudDescriptors::updateImageDescriptorSets(const std::vector<VkImageView>& metadataImageViews) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorImageStorageI = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, metadataImageViews[i], VK_IMAGE_LAYOUT_GENERAL);

        VkWriteDescriptorSet descriptorWrite = DescriptorWriter::makeWrite(imageDescriptorSets[i], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &descriptorImageStorageI);

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrite, 0, nullptr);
    }
}




ReduceDescriptors::ReduceDescriptors(const DescriptorBuilder& builder, const VkDevice device)
    : BaseDescriptors(device) {
    createDescriptorSetLayout(builder);
}

void ReduceDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;
    descriptorSLBs.reserve(4);

    VkDescriptorType descriptorType;
    for (uint32_t i = 0; i < descriptorSLBs.capacity(); i++) {      // use capacity instead of size
        if (i == 0) {
            descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        else {
            descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }

        builder.addDescriptorSetLayoutBinding(descriptorSLBs, i, descriptorType, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void ReduceDescriptors::createDescriptorSets(const DescriptorBuilder& builder, VkSampler sampler, const std::array<VkImageView, 4>& depthImages) {
    descriptorSets.resize(1);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout, 1);

    updateDescriptorSets(sampler, depthImages);
}

void ReduceDescriptors::updateDescriptorSets(VkSampler sampler, const std::array<VkImageView, 4>& depthImages) {
    VkDescriptorImageInfo descriptorsImageInfo0 = DescriptorWriter::makeImageInfo(sampler, depthImages[0]);
    VkDescriptorImageInfo descriptorsImageInfo1 = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, depthImages[1], VK_IMAGE_LAYOUT_GENERAL);
    VkDescriptorImageInfo descriptorsImageInfo2 = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, depthImages[2], VK_IMAGE_LAYOUT_GENERAL);
    VkDescriptorImageInfo descriptorsImageInfo3 = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, depthImages[3], VK_IMAGE_LAYOUT_GENERAL);

    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
    descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &descriptorsImageInfo0);
    descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &descriptorsImageInfo1);
    descriptorWrites[2] = DescriptorWriter::makeWrite(descriptorSets[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &descriptorsImageInfo2);
    descriptorWrites[3] = DescriptorWriter::makeWrite(descriptorSets[0], 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &descriptorsImageInfo3);

    vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}



NovelSynthDescriptors::NovelSynthDescriptors(const DescriptorBuilder& builder, const VkDevice device)
    : BaseDescriptors(device) {
    createDescriptorSetLayout(builder);
}

NovelSynthDescriptors::~NovelSynthDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, debugDescriptorSetLayout, nullptr);
}

void NovelSynthDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;

    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);

    descriptorSLBs.clear();
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    debugDescriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void NovelSynthDescriptors::createDescriptorSets(const DescriptorBuilder& builder,
                                                 const std::vector<VkBuffer>& camArraySSBOIn,
                                                 const NovelSynthUniforms& novelSynthUniforms) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorStorageBufferI1 = DescriptorWriter::makeBufferInfo(camArraySSBOIn[i], VK_WHOLE_SIZE);
        VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI1);

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }

    debugDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(debugDescriptorSets, debugDescriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorsImageInfo1 = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, novelSynthUniforms.debugImageView[i], VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageInfo descriptorsImageInfo2 = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, novelSynthUniforms.resultImageView[i], VK_IMAGE_LAYOUT_GENERAL);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites;
        descriptorWrites[0] = DescriptorWriter::makeWrite(debugDescriptorSets[i], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr,  &descriptorsImageInfo1);
        descriptorWrites[1] = DescriptorWriter::makeWrite(debugDescriptorSets[i], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr,  &descriptorsImageInfo2);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}

void NovelSynthDescriptors::updatePerCamDescriptorSets(const std::vector<VkBuffer>& camArraySSBOIn,
                                                       const NovelSynthUniforms& novelSynthUniforms) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorStorageBufferI1 = DescriptorWriter::makeBufferInfo(camArraySSBOIn[i], VK_WHOLE_SIZE);

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorStorageBufferI1);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorsImageInfo1 = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, novelSynthUniforms.debugImageView[i], VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageInfo descriptorsImageInfo2 = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, novelSynthUniforms.resultImageView[i], VK_IMAGE_LAYOUT_GENERAL);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites;
        descriptorWrites[0] = DescriptorWriter::makeWrite(debugDescriptorSets[i], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr,  &descriptorsImageInfo1);
        descriptorWrites[1] = DescriptorWriter::makeWrite(debugDescriptorSets[i], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr,  &descriptorsImageInfo2);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}

void NovelSynthDescriptors::updatePerResizeDescriptorSets(const std::vector<VkImageView>& resultImageViews) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorsImageInfo = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, resultImageViews[i], VK_IMAGE_LAYOUT_GENERAL);

        VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(debugDescriptorSets[i], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr,  &descriptorsImageInfo);

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }
}



RayDataDescriptors::RayDataDescriptors(const DescriptorBuilder& builder, const VkDevice device)
    : BaseDescriptors(device) {
    createDescriptorSetLayout(builder);
}

RayDataDescriptors::~RayDataDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, storageDescriptorSetLayout, nullptr);
}

void RayDataDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const RayDataUniforms& rayDataUniforms) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(rayDataUniforms.uniformBuffers[i], sizeof(RayDataObject));

        VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);
        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }

    storageDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(storageDescriptorSets, storageDescriptorSetLayout);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        updateStorageDescriptorSets(rayDataUniforms, i);
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(rayDataUniforms.rayCountSSBOIn[i], sizeof(uint32_t));

        VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(storageDescriptorSets[i], 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferI);
        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }
}

void RayDataDescriptors::updateStorageDescriptorSets(const RayDataUniforms& rayDataUniforms, uint32_t currentFrame) {
    VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(rayDataUniforms.rayDataSSBOIn[currentFrame], VK_WHOLE_SIZE);

    VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(storageDescriptorSets[currentFrame], 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferI);
    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
}

void RayDataDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;

    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
    
    // storage DSL
    descriptorSLBs.clear();
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    storageDescriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}




NovelReconDescriptors::NovelReconDescriptors(const DescriptorBuilder& builder, const VkDevice device)
    : BaseDescriptors(device) {
    createDescriptorSetLayout(builder);
}

NovelReconDescriptors::~NovelReconDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, resultDescriptorSetLayout, nullptr);
}

void NovelReconDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;

    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
    
    // storage DSL
    descriptorSLBs.clear();
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    resultDescriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void NovelReconDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const NovelReconUniforms& reconUniforms, CamerasManager& camManager) {
    VkDescriptorImageInfo descriptorImageSamplerI = DescriptorWriter::makeImageInfo(camManager.getSampler(ImageViewType::COLOR), camManager.getImageView(ImageViewType::COLOR));
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(reconUniforms.uniformBuffers[i], sizeof(ReconDataObject));
        VkDescriptorImageInfo samplerDescriptorImageI = DescriptorWriter::makeImageInfo(camManager.getSampler(ImageViewType::COLOR), camManager.getImageView(ImageViewType::COLOR));


        std::array<VkWriteDescriptorSet, 2> descriptorWrites;
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);
        descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &samplerDescriptorImageI);
        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    resultDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(resultDescriptorSets, resultDescriptorSetLayout);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        updateResultDescriptorSets(reconUniforms.resultImageView, i);
    }
}

void NovelReconDescriptors::updateRefImageDescritporSets(CamerasManager& camManager) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo samplerDescriptorImageI = DescriptorWriter::makeImageInfo(camManager.getSampler(ImageViewType::COLOR), camManager.getImageView(ImageViewType::COLOR));


        VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(descriptorSets[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &samplerDescriptorImageI);
        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }
}

void NovelReconDescriptors::updateResultDescriptorSets(const std::vector<VkImageView>& resultImageViews, uint32_t currentFrame) {
    VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE, resultImageViews[currentFrame], VK_IMAGE_LAYOUT_GENERAL);
    VkWriteDescriptorSet descriptorWrite = DescriptorWriter::makeWrite(resultDescriptorSets[currentFrame], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &descriptorImageI);

    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrite, 0, nullptr);
}