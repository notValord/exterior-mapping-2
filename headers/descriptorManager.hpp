#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <array>

extern const size_t MAX_FRAMES_IN_FLIGHT;

struct TextureSamplerView;
class CamerasManager;

class FrustumDescriptors{   // shared also for intersection shader
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

class CamCubeDescriptors{
public:
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> descriptorSets;        // automatically freed with descriptor pool

    CamCubeDescriptors(const VkDevice device);
    ~CamCubeDescriptors();

    void createDescriptorSets(VkDescriptorPool descriptorPool, const TextureSamplerView& textureSamplerView);
private:
    // Vulkan handles
    VkDevice deviceHandle;

    void createDescriptiorSetLayout();
};

class OfflineDescriptors{
public:
    VkDescriptorSetLayout samplerDescriptorSetLayout;
    std::vector<std::array<VkDescriptorSet, 2>> samplerDescriptorSets;        // automatically freed with descriptor pool

    OfflineDescriptors(const VkDevice device);
    ~OfflineDescriptors();

    void createDescriptorSets(VkDescriptorPool descriptorPool);
    void updateDescriptorSets(const TextureSamplerView& textureSamplerView);

    uint32_t getIndexInUse();
private:
    uint32_t inUse = 0;

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

    VkDescriptorSetLayout sharedDescriptorSetLayout;
    VkDescriptorSet sharedDescriptorSet;

    ComputeDescriptors(const VkDevice device);
    ~ComputeDescriptors();

    void createDescriptorSets(VkDescriptorPool descriptorPool, const std::vector<VkBuffer>& novelUniformBuffers, const std::vector<VkBuffer>& camArraySSBOIn,
         const std::vector<VkBuffer>& intersectionsSSBOOut, const std::vector<VkBuffer>& vertexCountSSBOOut, CamerasManager& camManager);
    void updateDescriptorSets(uint32_t currentFrame, const std::vector<VkBuffer>& camArraySSBOIn, const std::vector<VkBuffer>& intersectionsSSBOOut);
    void updateImageDescriptors(CamerasManager& camManager);
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
    CamCubeDescriptors camCubeDestriptors;
    OfflineDescriptors offlineDescriptors;

    DescriptorManager(const VkDevice device);
    ~DescriptorManager();

    void createDescriptorPoolSet(const std::vector<VkBuffer>& uniformBuffers, const TextureSamplerView& textureSamplerView, const std::vector<VkBuffer>& novelUniformBuffers,
         const std::vector<VkBuffer>& camArraySSBOIn, const std::vector<VkBuffer>& intersectionsSSBOOut, const std::vector<VkBuffer>& vertexCountSSBOOut, const TextureSamplerView& cubeSamplerView, 
         CamerasManager& camManager);
private:
    VkDescriptorPool descriptorPool;

    // Vulkan handles
    VkDevice deviceHandle;

    void createDescriptorPool();
};