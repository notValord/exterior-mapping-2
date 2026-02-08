#include <descriptorManager.hpp>
#include <uniforms.hpp>
#include <textures.hpp>
#include <camManager.hpp>

static const uint32_t SETS_COUNT = 7;   // render, compute + shared, frustum+line, cam cube, offline


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
      offlineDescriptors(builder, device) {

}

DescriptorManager::~DescriptorManager() {
    vkDestroyDescriptorPool(deviceHandle, descriptorPool, nullptr);
}

void DescriptorManager::createDescriptorPoolSet(const std::vector<VkBuffer>& uniformBuffers,
                                                const TextureSamplerView& textureSamplerView,
                                                const std::vector<VkBuffer>& novelUniformBuffers,
                                                const std::vector<VkBuffer>& camArraySSBOIn,
                                                const std::vector<VkBuffer>& intersectionsSSBOOut,
                                                const std::vector<VkBuffer>& vertexCountSSBOOut,
                                                const TextureSamplerView& cubeSamplerView, 
                                                CamerasManager& camManager,
                                                const std::vector<VkBuffer>& offlineRenderBuffers) {
    createDescriptorPool();
    builder.setDescriptorPool(descriptorPool);

    renderDescriptors.createDescriptorSets(builder, uniformBuffers, textureSamplerView);
    computeDescriptors.createDescriptorSets(builder, novelUniformBuffers, camArraySSBOIn, intersectionsSSBOOut, vertexCountSSBOOut, camManager);

    // Debug utils
    frustumDescriptors.createDescriptorSets(builder, uniformBuffers);
    camCubeDestriptors.createDescriptorSets(builder, cubeSamplerView);
    offlineDescriptors.createDescriptorSets(builder, offlineRenderBuffers, camManager);
}

void DescriptorManager::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0] = {
        .type  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // render ubo, compute ubo, line + frustum ubo, offline ubo
        .descriptorCount = 4*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };
    poolSizes[1] = {
        .type  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // model texture, cam cube, offline image, novel view
        .descriptorCount = 3*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) + 1
    };
    poolSizes[2] = {
        .type  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 3x compute ssbo
        .descriptorCount = 3*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
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
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(uniformBuffers[i], sizeof(UniformBufferObject));
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

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        toUpdate.push_back(false);
    }
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
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(offlineRenderBuffers[i], sizeof(OfflineRenderBuffer));

        // bind to dummy at the start
        VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(camManager.imageSampler.getSampler(), camManager.novelView.getImageView(i));

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);
        descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &descriptorImageI);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}

void OfflineDescriptors::updateDescriptorSets(CamerasManager& camManager, uint32_t currentFrame) {
    if (toUpdate[currentFrame] == false) {
        return;
    }

    VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(camManager.imageSampler.getSampler(),      // the same sampler as for the samplerArray
                                                                             camManager.novelView.getImageView(currentFrame));

    VkWriteDescriptorSet descriptorWrites = DescriptorWriter::makeWrite(descriptorSets[currentFrame],
                                                                        1,
                                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                        nullptr,
                                                                        &descriptorImageI);

    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    toUpdate[currentFrame] = false;
}

void OfflineDescriptors::setUpdateFlags() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        toUpdate[i] = true;
    }
}



RenderDescriptors::RenderDescriptors(const DescriptorBuilder& builder, const VkDevice device)
        : BaseDescriptors(device)  {
    createDescriptorSetLayout(builder);
}

void RenderDescriptors::createDescriptorSetLayout(const DescriptorBuilder& builder) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSLBs;

    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.addDescriptorSetLayoutBinding(descriptorSLBs, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void RenderDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& uniformBuffers, const TextureSamplerView& textureSamplerView) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(uniformBuffers[i], sizeof(UniformBufferObject));
        VkDescriptorImageInfo descriptorImageI = DescriptorWriter::makeImageInfo(textureSamplerView.textureSampler, textureSamplerView.textureImageView);


        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0] = DescriptorWriter::makeWrite(descriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferI);
        descriptorWrites[1] = DescriptorWriter::makeWrite(descriptorSets[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &descriptorImageI);

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
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
    sharedDescriptorSetLayout = builder.buildDescriptorSetLayout(descriptorSLBs);
}

void ComputeDescriptors::createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& novelUniformBuffers, const std::vector<VkBuffer>& camArraySSBOIn,
         const std::vector<VkBuffer>& intersectionsSSBOOut, const std::vector<VkBuffer>& vertexCountSSBOOut, CamerasManager& camManager) {
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    builder.buildDescriptorSets(descriptorSets, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

        VkDescriptorBufferInfo descriptorBufferI = DescriptorWriter::makeBufferInfo(novelUniformBuffers[i], sizeof(NovelImageObject));

        VkDescriptorBufferInfo descriptorStorageBufferI1 = DescriptorWriter::makeBufferInfo(camArraySSBOIn[i], VK_WHOLE_SIZE);
        VkDescriptorBufferInfo descriptorStorageBufferI2 = DescriptorWriter::makeBufferInfo(intersectionsSSBOOut[i], VK_WHOLE_SIZE);
        VkDescriptorBufferInfo descriptorStorageBufferI3 = DescriptorWriter::makeBufferInfo(vertexCountSSBOOut[i], sizeof(uint32_t));

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
    VkDescriptorImageInfo descriptorImageSamplerI = DescriptorWriter::makeImageInfo(camManager.imageSampler.getSampler(), camManager.getImageView());
    VkWriteDescriptorSet descriptorWritesShared = DescriptorWriter::makeWrite(sharedDescriptorSet,
                                                                              0,
                                                                              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                              nullptr,
                                                                              &descriptorImageSamplerI);

    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWritesShared, 0, nullptr);
}

void ComputeDescriptors::updateImageDescriptors(CamerasManager& camManager) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorImageStorageI = DescriptorWriter::makeImageInfo(VK_NULL_HANDLE,
                                                                                        camManager.novelView.getImageView(i),      // stuck with the current resolution
                                                                                        VK_IMAGE_LAYOUT_GENERAL);
        VkWriteDescriptorSet descriptorWrite = DescriptorWriter::makeWrite(descriptorSets[i], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &descriptorImageStorageI);

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrite, 0, nullptr);
    }

    updateSharedImageDescriptor(camManager);
}