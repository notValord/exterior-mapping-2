#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <array>

extern const int MAX_FRAMES_IN_FLIGHT;

struct TextureSamplerView;

class DescriptorManager{
public:
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> descriptorSets;        // automatically freed with descriptor pool

    DescriptorManager(const VkDevice& device);
    ~DescriptorManager();

    void createDescriptorPoolSet(const std::vector<VkBuffer>& uniformBuffers, const TextureSamplerView& textureSamplerView);
private:
    VkDescriptorPool descriptorPool;

    // Vulkan handles
    VkDevice deviceHandle;

    void createDescriptiorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, const TextureSamplerView& textureSamplerView);
};