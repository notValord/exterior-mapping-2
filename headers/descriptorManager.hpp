#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <array>

extern const size_t MAX_FRAMES_IN_FLIGHT;

struct TextureSamplerView;
struct MeshUniforms;
class CamerasManager;
class UniformManager;
class RenderUniforms;
class NovelUniforms;
class PointCloudUniforms;
class RayDataUniforms;
class NovelSynthUniforms;
class NovelReconUniforms;

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
    void updateDescriptorSets(CamerasManager& camManager);
    void updateDescriptorSets(uint32_t currentFrame, CamerasManager& camManager);
    void updateDescriptorSets(uint32_t currentFrame, NovelReconUniforms& reconUniform);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};



class RenderDescriptors : public BaseDescriptors{
public:
    VkDescriptorSetLayout samplerDescriptorSetLayout;
    std::vector<VkDescriptorSet> samplerDescriptorSets;

    RenderDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~RenderDescriptors();

    void createDescriptorSets(const DescriptorBuilder& builder,
                              const RenderUniforms& renderUniforms,
                              const MeshUniforms& meshData);
    void updateSamplerDescriptorSets(const VkSampler& sampler, const std::vector<VkImageView>& materials);
    void updateMaterialDescriptosSets(VkBuffer materials);
    void updateMeshData(const MeshUniforms& meshData);
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
    void createSamplerDescriptorSets(const DescriptorBuilder& builder, const VkSampler& sampler, const std::vector<VkImageView>& materials);
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
    VkDescriptorSetLayout imageDescriptorSetLayout;
    std::vector<VkDescriptorSet> imageDescriptorSets;

    PointCloudDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~PointCloudDescriptors();

    void createDescriptorSets(const DescriptorBuilder& builder, const PointCloudUniforms& pointCloudUniforms, const std::vector<VkImageView>& metadataImageViews);
    void updateDescriptorSets(const PointCloudUniforms& pointCloudUniforms);        // update once both before running the shader

    void createImageDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkImageView>& metadataImageViews);
    void updateImageDescriptorSets(const std::vector<VkImageView>& metadataImageViews);
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

class ReduceDescriptors : public BaseDescriptors {
public:
    ReduceDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    void createDescriptorSets(const DescriptorBuilder& builder, VkSampler sampler, const std::array<VkImageView, 4>& depthImages);
    void updateDescriptorSets(VkSampler sampler, const std::array<VkImageView, 4>& depthImages);
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

class NovelSynthDescriptors : public BaseDescriptors {
public:
    VkDescriptorSetLayout debugDescriptorSetLayout;
    std::vector<VkDescriptorSet> debugDescriptorSets;

    NovelSynthDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~NovelSynthDescriptors();

    void createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& camArraySSBOIn, const NovelSynthUniforms& novelSynthUniforms);
    void updatePerCamDescriptorSets(const std::vector<VkBuffer>& camArraySSBOIn, const NovelSynthUniforms& novelSynthUniforms);
    void updatePerResizeDescriptorSets(const std::vector<VkImageView>& resultImageViews);
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

class RayDataDescriptors : public BaseDescriptors {
public:
    VkDescriptorSetLayout storageDescriptorSetLayout;
    std::vector<VkDescriptorSet> storageDescriptorSets;

    RayDataDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~RayDataDescriptors();

    void createDescriptorSets(const DescriptorBuilder& builder, const RayDataUniforms& novelSynthUniforms);
    void updateStorageDescriptorSets(const RayDataUniforms& novelSynthUniforms, uint32_t currentFrame);
private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

class NovelReconDescriptors : public BaseDescriptors {
public:
    VkDescriptorSetLayout resultDescriptorSetLayout;
    std::vector<VkDescriptorSet> resultDescriptorSets;

    NovelReconDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~NovelReconDescriptors();

    void createDescriptorSets(const DescriptorBuilder& builder, const NovelReconUniforms& reconUniforms, CamerasManager& camManager);
    void updateRefImageDescritporSets(CamerasManager& camManager);
    void updateResultDescriptorSets(const std::vector<VkImageView>& resultImageViews, uint32_t currentFrame);
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

    RayDataDescriptors rayDataDescriptors;
    ReduceDescriptors reduceDescriptors;
    NovelSynthDescriptors novelSynthDescriptors;
    NovelReconDescriptors novelReconDescriptors;

    DescriptorManager(const VkDevice device);
    ~DescriptorManager();

    void createDescriptorPoolSet(const UniformManager& uniformManager,
                                 const MeshUniforms& meshData,
                                 const TextureSamplerView& cubeSamplerView,
                                 CamerasManager& camManager);
private:
    VkDescriptorPool descriptorPool;

    // Vulkan handles
    VkDevice deviceHandle;

    void createDescriptorPool();
};