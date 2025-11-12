#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <array>

extern const int MAX_FRAMES_IN_FLIGHT;

struct TextureSamplerView;

class FrustumDescriptors{
public:
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> descriptorSets;        // automatically freed with descriptor pool

    FrustumDescriptors(const VkDevice device);
    ~FrustumDescriptors();

    void createDescriptorSets(VkDescriptorPool descriptorPool, const std::vector<VkBuffer>& uniformBuffers);
private:
    // Vulkan handles
    VkDevice deviceHandle;

    void createDescriptiorSetLayout();
};

class RenderDescriptors{
public:
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> descriptorSets;        // automatically freed with descriptor pool

    RenderDescriptors(const VkDevice device);
    ~RenderDescriptors();

    void createDescriptorSets(VkDescriptorPool descriptorPool, const std::vector<VkBuffer>& uniformBuffers, const TextureSamplerView& textureSamplerView);
private:
    // Vulkan handles
    VkDevice deviceHandle;

    void createDescriptiorSetLayout();
};


class ComputeDescriptors{
public:
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> descriptorSets;        // automatically freed with descriptor pool

    ComputeDescriptors(const VkDevice device);
    ~ComputeDescriptors();

    void createDescriptorSets(VkDescriptorPool descriptorPool, const std::vector<VkBuffer>& novelUniformBuffers, const VkBuffer camArraySSBOIn,
         const std::vector<VkBuffer>& intersectionsSSBOOut, const std::vector<VkBuffer>& vertexCountSSBOOut);
private:
    // Vulkan handles
    VkDevice deviceHandle;

    void createDescriptiorSetLayout();
};


class DescriptorManager{
public:
    RenderDescriptors renderDescriptors;
    ComputeDescriptors computeDescriptors;
    FrustumDescriptors frustumDescriptors;

    DescriptorManager(const VkDevice device);
    ~DescriptorManager();

    void createDescriptorPoolSet(const std::vector<VkBuffer>& uniformBuffers, const TextureSamplerView& textureSamplerView, const std::vector<VkBuffer>& novelUniformBuffers,
         const VkBuffer camArraySSBOIn, const std::vector<VkBuffer>& intersectionsSSBOOut, const std::vector<VkBuffer>& vertexCountSSBOOut);
private:
    VkDescriptorPool descriptorPool;

    // Vulkan handles
    VkDevice deviceHandle;

    void createDescriptorPool();
};