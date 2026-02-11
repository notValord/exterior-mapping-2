#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <array>

extern const size_t MAX_FRAMES_IN_FLIGHT;

struct TextureSamplerView;
class CamerasManager;
class UniformManager;
class RenderUniforms;
class NovelUniforms;
class PointCloudUniforms;

class DescriptorBuilder{
public:
    DescriptorBuilder(VkDevice device);
    ~DescriptorBuilder();

    void setDescriptorPool(VkDescriptorPool descriptorPool);

    void addDescriptorSetLayoutBinding(std::vector<VkDescriptorSetLayoutBinding>& inVector, uint32_t bind, VkDescriptorType type, VkShaderStageFlags stage, uint32_t count = 1) const;
    VkDescriptorSetLayout buildDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>& inVector) const;

    void buildDescriptorSets(std::vector<VkDescriptorSet>& outDescriptorSets, VkDescriptorSetLayout descriptorSetLayout, uint32_t count = MAX_FRAMES_IN_FLIGHT) const;
private:
    // Vulkan handles
    VkDevice deviceHandle;
    VkDescriptorPool descriptorPoolRef = VK_NULL_HANDLE;
};



class BaseDescriptors{
public:
    VkDescriptorSetLayout descriptorSetLayout{};
    std::vector<VkDescriptorSet> descriptorSets;        // automatically freed with descriptor pool

    BaseDescriptors(const VkDevice device);
    virtual ~BaseDescriptors();
protected:
    // Vulkan handles
    VkDevice deviceHandle;

    virtual void createDescriptorSetLayout(const DescriptorBuilder& builder) = 0;
};



class FrustumDescriptors : public BaseDescriptors{   // shared also for intersection shader
public:
    FrustumDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    void createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& uniformBuffers);
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};



class CamCubeDescriptors : public BaseDescriptors{
public:
    CamCubeDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    void createDescriptorSets(const DescriptorBuilder& builder, const TextureSamplerView& textureSamplerView);
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};



class OfflineDescriptors : public BaseDescriptors{
public:
    OfflineDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    void createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& offlineUniformBuffers, CamerasManager& camManager);
    void updateDescriptorSets(CamerasManager& camManager, uint32_t currentFrame);
    void setUpdateFlags();
private:
    std::vector<bool> toUpdate;

    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};



class RenderDescriptors : public BaseDescriptors{
public:
    RenderDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    void createDescriptorSets(const DescriptorBuilder& builder, const RenderUniforms& renderUniforms, const TextureSamplerView& textureSamplerView);
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};


class ComputeDescriptors : public BaseDescriptors {
public:
    VkDescriptorSetLayout sharedDescriptorSetLayout;
    VkDescriptorSet sharedDescriptorSet;                // shared with offline render most likely

    ComputeDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~ComputeDescriptors();

    void createDescriptorSets(const DescriptorBuilder& builder, const NovelUniforms& novelUniforms, CamerasManager& camManager);
    void updateDescriptorSets(uint32_t currentFrame, const std::vector<VkBuffer>& camArraySSBOIn, const std::vector<VkBuffer>& intersectionsSSBOOut);
    void updateSharedImageDescriptor(CamerasManager& camManager);
    void updateImageDescriptors(CamerasManager& camManager);
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

class PointCloudDescriptors : public BaseDescriptors {
public:

    PointCloudDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    void createDescriptorSets(const DescriptorBuilder& builder, const PointCloudUniforms& pointCloudUniforms);
    void updateDescriptorSets(const PointCloudUniforms& pointCloudUniforms);        // update once both before running the shader
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};


class DescriptorManager{
public:
    DescriptorBuilder builder;

    RenderDescriptors renderDescriptors;
    ComputeDescriptors computeDescriptors;
    FrustumDescriptors frustumDescriptors;
    CamCubeDescriptors camCubeDestriptors;
    OfflineDescriptors offlineDescriptors;
    PointCloudDescriptors pointCloudDescriptors;

    DescriptorManager(const VkDevice device);
    ~DescriptorManager();

    void createDescriptorPoolSet(const UniformManager& uniformManager, const TextureSamplerView& textureSamplerView, const TextureSamplerView& cubeSamplerView, CamerasManager& camManager);
private:
    VkDescriptorPool descriptorPool;

    // Vulkan handles
    VkDevice deviceHandle;

    void createDescriptorPool();
};