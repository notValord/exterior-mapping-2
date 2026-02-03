#include <descriptorManager.hpp>
#include <uniforms.hpp>
#include <textures.hpp>
#include <camManager.hpp>

static const uint32_t SETS_COUNT = 7;   // render, 2xcompute, frustum+line, cam cube, 2xoffline

DescriptorManager::DescriptorManager(const VkDevice device)
    : renderDescriptors(device), computeDescriptors(device), deviceHandle(device), frustumDescriptors(device), camCubeDestriptors(device), offlineDescriptors(device){

}

DescriptorManager::~DescriptorManager() {
    vkDestroyDescriptorPool(deviceHandle, descriptorPool, nullptr);
}

void DescriptorManager::createDescriptorPoolSet(const std::vector<VkBuffer>& uniformBuffers, const TextureSamplerView& textureSamplerView, const std::vector<VkBuffer>& novelUniformBuffers,
    const std::vector<VkBuffer>& camArraySSBOIn, const std::vector<VkBuffer>& intersectionsSSBOOut, const std::vector<VkBuffer>& vertexCountSSBOOut, const TextureSamplerView& cubeSamplerView, 
    CamerasManager& camManager) {
    createDescriptorPool();
    renderDescriptors.createDescriptorSets(descriptorPool, uniformBuffers, textureSamplerView);

    // Debug utils
    frustumDescriptors.createDescriptorSets(descriptorPool, uniformBuffers);
    camCubeDestriptors.createDescriptorSets(descriptorPool, cubeSamplerView);
    offlineDescriptors.createDescriptorSets(descriptorPool);

    computeDescriptors.createDescriptorSets(descriptorPool, novelUniformBuffers, camArraySSBOIn, intersectionsSSBOOut, vertexCountSSBOOut, camManager);
}

void DescriptorManager::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0] = {
        .type  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // render ubo, compute ubo, line + frustum ubo, 
        .descriptorCount = 3*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };
    poolSizes[1] = {
        .type  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // model texture, cam cube, 2xoffline image, novel view
        .descriptorCount = 4*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) + 1
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

FrustumDescriptors::FrustumDescriptors(const VkDevice device) : deviceHandle(device) {
    createDescriptiorSetLayout();
}

FrustumDescriptors::~FrustumDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayout, nullptr);
}

void FrustumDescriptors::createDescriptiorSetLayout() {
    VkDescriptorSetLayoutBinding descriptorSLB{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr,
    };

    VkDescriptorSetLayoutCreateInfo descriptorSLCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &descriptorSLB,
    };

    if (vkCreateDescriptorSetLayout(deviceHandle, &descriptorSLCI, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Faield to create descriptor set layout!");
    }
}

void FrustumDescriptors::createDescriptorSets(VkDescriptorPool descriptorPool, const std::vector<VkBuffer>& uniformBuffers) {
    VkDescriptorSetLayout layoutSets[] = {descriptorSetLayout, descriptorSetLayout};

    VkDescriptorSetAllocateInfo descriptorSetAI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layoutSets,
    };

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(deviceHandle, &descriptorSetAI, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI{
            .buffer = uniformBuffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject)
        };

        VkWriteDescriptorSet descriptorWrites {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptorBufferI,
            .pTexelBufferView = nullptr,
        };

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }
}

CamCubeDescriptors::CamCubeDescriptors(const VkDevice device) : deviceHandle(device) {
    createDescriptiorSetLayout();
}

CamCubeDescriptors::~CamCubeDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayout, nullptr);
}

void CamCubeDescriptors::createDescriptiorSetLayout() {
    VkDescriptorSetLayoutBinding samplerDescriptorSLB{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo descriptorSLCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &samplerDescriptorSLB,
    };

    if (vkCreateDescriptorSetLayout(deviceHandle, &descriptorSLCI, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Faield to create descriptor set layout!");
    }
}

void CamCubeDescriptors::createDescriptorSets(VkDescriptorPool descriptorPool, const TextureSamplerView& textureSamplerView) {
    VkDescriptorSetLayout layoutSets[] = {descriptorSetLayout, descriptorSetLayout};

    VkDescriptorSetAllocateInfo descriptorSetAI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layoutSets,
    };

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(deviceHandle, &descriptorSetAI, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorImageI{
            .sampler = textureSamplerView.textureSampler,
            .imageView = textureSamplerView.textureImageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkWriteDescriptorSet descriptorWrites {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &descriptorImageI,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }
}

OfflineDescriptors::OfflineDescriptors(const VkDevice device) : deviceHandle(device) {
    createDescriptiorSetLayout();
}

OfflineDescriptors::~OfflineDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, samplerDescriptorSetLayout, nullptr);
}

void OfflineDescriptors::createDescriptiorSetLayout() {
    //add another sampler and a UBO -> need a buffer for a ubo
    VkDescriptorSetLayoutBinding samplerDescriptorSLB{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo samplerDescriptorSLCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &samplerDescriptorSLB,
    };

    if (vkCreateDescriptorSetLayout(deviceHandle, &samplerDescriptorSLCI, nullptr, &samplerDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Faield to create descriptor set layout!");
    }
}

void OfflineDescriptors::createDescriptorSets(VkDescriptorPool descriptorPool) {
    std::array<VkDescriptorSetLayout, 2> layoutSets = {samplerDescriptorSetLayout, samplerDescriptorSetLayout};

    VkDescriptorSetAllocateInfo samplerDescriptorSetAI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layoutSets.size()),
        .pSetLayouts = layoutSets.data(),
    };

    samplerDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkAllocateDescriptorSets(deviceHandle, &samplerDescriptorSetAI, samplerDescriptorSets[i].data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }
    }
}

void OfflineDescriptors::updateDescriptorSets(const TextureSamplerView& textureSamplerView) {
    inUse = (inUse+1) % 2;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorImageI{
        .sampler = textureSamplerView.textureSampler,
        .imageView = textureSamplerView.textureImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet descriptorWrites{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = samplerDescriptorSets[i][inUse],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &descriptorImageI,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrites, 0, nullptr);
    }
}

uint32_t OfflineDescriptors::getIndexInUse() {
    return inUse;
}

RenderDescriptors::RenderDescriptors(const VkDevice device) : deviceHandle(device) {
    createDescriptiorSetLayout();
}

RenderDescriptors::~RenderDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayout, nullptr);
}

void RenderDescriptors::createDescriptiorSetLayout() {
    VkDescriptorSetLayoutBinding descriptorSLB{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr,
    };
    VkDescriptorSetLayoutBinding samplerDescriptorSLB{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {descriptorSLB, samplerDescriptorSLB};

    VkDescriptorSetLayoutCreateInfo descriptorSLCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    if (vkCreateDescriptorSetLayout(deviceHandle, &descriptorSLCI, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Faield to create descriptor set layout!");
    }
}

void RenderDescriptors::createDescriptorSets(VkDescriptorPool descriptorPool, const std::vector<VkBuffer>& uniformBuffers, const TextureSamplerView& textureSamplerView) {
    VkDescriptorSetLayout layoutSets[] = {descriptorSetLayout, descriptorSetLayout};

    VkDescriptorSetAllocateInfo descriptorSetAI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layoutSets,
    };

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(deviceHandle, &descriptorSetAI, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI{
            .buffer = uniformBuffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject)
        };

        VkDescriptorImageInfo descriptorImageI{
            .sampler = textureSamplerView.textureSampler,
            .imageView = textureSamplerView.textureImageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptorBufferI,
            .pTexelBufferView = nullptr,
        };
        descriptorWrites[1] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &descriptorImageI,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}

ComputeDescriptors::ComputeDescriptors(const VkDevice device) : deviceHandle(device) {
    createDescriptiorSetLayout();
}

ComputeDescriptors::~ComputeDescriptors() {
    vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(deviceHandle, sharedDescriptorSetLayout, nullptr);
}

void ComputeDescriptors::createDescriptiorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 5> descriptorSLB{};
    for (uint32_t i = 0; i < descriptorSLB.size(); i++) {
        descriptorSLB[i].binding = i;
        descriptorSLB[i].descriptorCount = 1;
        descriptorSLB[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        descriptorSLB[i].pImmutableSamplers = nullptr;

        if (i == 0) {
            descriptorSLB[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        else if (i == 4) {
            descriptorSLB[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        else {
            descriptorSLB[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
    }

    VkDescriptorSetLayoutCreateInfo descriptorSLCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(descriptorSLB.size()),
        .pBindings = descriptorSLB.data(),
    };

    if (vkCreateDescriptorSetLayout(deviceHandle, &descriptorSLCI, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Faield to create descriptor set layout!");
    }

    VkDescriptorSetLayoutBinding sharedDescriptorSLB = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = nullptr,
    };

    descriptorSLCI = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sharedDescriptorSLB,
    };

    if (vkCreateDescriptorSetLayout(deviceHandle, &descriptorSLCI, nullptr, &sharedDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Faield to create descriptor set layout!");
    }
}

void ComputeDescriptors::createDescriptorSets(VkDescriptorPool descriptorPool, const std::vector<VkBuffer>& novelUniformBuffers, const std::vector<VkBuffer>& camArraySSBOIn,
         const std::vector<VkBuffer>& intersectionsSSBOOut, const std::vector<VkBuffer>& vertexCountSSBOOut, CamerasManager& camManager) {
    VkDescriptorSetLayout layoutSets[] = {descriptorSetLayout, descriptorSetLayout};

    VkDescriptorSetAllocateInfo descriptorSetAI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layoutSets,
    };

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(deviceHandle, &descriptorSetAI, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferI{
            .buffer = novelUniformBuffers[i],
            .offset = 0,
            .range = sizeof(NovelImageObject)
        };

        VkDescriptorBufferInfo descriptorStorageBufferI1{
            .buffer = camArraySSBOIn[i],
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };

        VkDescriptorBufferInfo descriptorStorageBufferI2{
            .buffer = intersectionsSSBOOut[i],
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };

        VkDescriptorBufferInfo descriptorStorageBufferI3{
            .buffer = vertexCountSSBOOut[i],
            .offset = 0,
            .range = sizeof(uint32_t)
        };

        // uses a dummy resource until the reconstruction is set to go
        VkDescriptorImageInfo descriptorImageStorageI{
            .sampler = VK_NULL_HANDLE,
            .imageView = camManager.novelView.getImageView(i),      // novel image per frame
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        std::array<VkWriteDescriptorSet, 5> descriptorWrites{};
        descriptorWrites[0] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptorBufferI,
            .pTexelBufferView = nullptr,
        };
        descriptorWrites[1] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptorStorageBufferI1,
            .pTexelBufferView = nullptr,
        };
        descriptorWrites[2] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptorStorageBufferI2,
            .pTexelBufferView = nullptr,
        };
        descriptorWrites[3] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptorStorageBufferI3,
            .pTexelBufferView = nullptr,
        };

        descriptorWrites[4] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 4,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &descriptorImageStorageI,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    // shared descriptor set for reference cam samplers
    descriptorSetAI = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &sharedDescriptorSetLayout,
    };
    if (vkAllocateDescriptorSets(deviceHandle, &descriptorSetAI, &sharedDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    // ready to go from the start
    VkDescriptorImageInfo descriptorImageSamplerI = {
        .sampler = camManager.imageSampler.getSampler(),
        .imageView = camManager.getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet descriptorWritesShared = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = sharedDescriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &descriptorImageSamplerI,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };
    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWritesShared, 0, nullptr);
}

void ComputeDescriptors::updateDescriptorSets(uint32_t currentFrame, const std::vector<VkBuffer>& camArraySSBOIn,
                                                const std::vector<VkBuffer>& intersectionsSSBOOut) {
    std::array<VkDescriptorBufferInfo, 2> descriptorBuffers{};
    descriptorBuffers[0] = {
        .buffer = camArraySSBOIn[currentFrame],
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };

    descriptorBuffers[1] = {
        .buffer = intersectionsSSBOOut[currentFrame],
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSets[currentFrame],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &descriptorBuffers[0],
        .pTexelBufferView = nullptr,
    };
    descriptorWrites[1] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSets[currentFrame],
        .dstBinding = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &descriptorBuffers[1],
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}

void ComputeDescriptors::updateImageDescriptors(CamerasManager& camManager) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo descriptorImageStorageI{
            .sampler = VK_NULL_HANDLE,
            .imageView = camManager.novelView.getImageView(i),      // stuck with the current resolution
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkWriteDescriptorSet descriptorWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 4,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &descriptorImageStorageI,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrite, 0, nullptr);
    }

    VkDescriptorImageInfo descriptorImageSamplerI = {
        .sampler = camManager.imageSampler.getSampler(),
        .imageView = camManager.getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet descriptorWritesShared = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = sharedDescriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &descriptorImageSamplerI,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };
    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWritesShared, 0, nullptr);
}