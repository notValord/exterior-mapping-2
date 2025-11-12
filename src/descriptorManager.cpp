#include "descriptorManager.hpp"
#include "uniforms.hpp"
#include "textures.hpp"

static const uint32_t SHADER_COUNT = 3;

DescriptorManager::DescriptorManager(const VkDevice device)
    : renderDescriptors(device), computeDescriptors(device), deviceHandle(device), frustumDescriptors(device){

}

DescriptorManager::~DescriptorManager() {
    vkDestroyDescriptorPool(deviceHandle, descriptorPool, nullptr);
}

void DescriptorManager::createDescriptorPoolSet(const std::vector<VkBuffer>& uniformBuffers, const TextureSamplerView& textureSamplerView, const std::vector<VkBuffer>& novelUniformBuffers,
     const VkBuffer camArraySSBOIn, const std::vector<VkBuffer>& intersectionsSSBOOut, const std::vector<VkBuffer>& vertexCountSSBOOut) {
    createDescriptorPool();
    renderDescriptors.createDescriptorSets(descriptorPool, uniformBuffers, textureSamplerView);
    frustumDescriptors.createDescriptorSets(descriptorPool, uniformBuffers);
    computeDescriptors.createDescriptorSets(descriptorPool, novelUniformBuffers, camArraySSBOIn, intersectionsSSBOOut, vertexCountSSBOOut);
}

void DescriptorManager::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0] = {
        .type  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 3*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };
    poolSizes[1] = {
        .type  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };
    poolSizes[2] = {
        .type  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 3*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = SHADER_COUNT*static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
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
}

void ComputeDescriptors::createDescriptiorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 4> descriptorSLB{};
    for (uint32_t i = 0; i < descriptorSLB.size(); i++) {
        descriptorSLB[i].binding = i;
        if (i == 0) {
            descriptorSLB[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        else {
            descriptorSLB[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        descriptorSLB[i].descriptorCount = 1;
        descriptorSLB[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        descriptorSLB[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo descriptorSLCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(descriptorSLB.size()),
        .pBindings = descriptorSLB.data(),
    };

    if (vkCreateDescriptorSetLayout(deviceHandle, &descriptorSLCI, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Faield to create descriptor set layout!");
    }
}

void ComputeDescriptors::createDescriptorSets(VkDescriptorPool descriptorPool, const std::vector<VkBuffer>& novelUniformBuffers, const VkBuffer camArraySSBOIn,
         const std::vector<VkBuffer>& intersectionsSSBOOut, const std::vector<VkBuffer>& vertexCountSSBOOut) {
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
            .buffer = camArraySSBOIn,
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

        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
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

        vkUpdateDescriptorSets(deviceHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}