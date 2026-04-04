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

/**
 * @class DescriptorBuilder
 * @brief Helper class for creating Vulkan descriptor set layouts and sets.
 */
class DescriptorBuilder{
public:
    DescriptorBuilder(VkDevice device);
    ~DescriptorBuilder();

    /**
     * @brief Set the descriptor pool to use for allocations.
     * @param descriptorPool The descriptor pool handle.
     */
    void setDescriptorPool(VkDescriptorPool descriptorPool);

    /**
     * @brief Add a binding to a descriptor set layout vector.
     * @param inVector Vector to add the binding to.
     * @param bind Binding index.
     * @param type Descriptor type.
     * @param stage Shader stages that access this binding.
     * @param count Number of descriptors (default 1).
     */
    void addDescriptorSetLayoutBinding(std::vector<VkDescriptorSetLayoutBinding>& inVector, uint32_t bind, VkDescriptorType type, VkShaderStageFlags stage, uint32_t count = 1) const;

    /**
     * @brief Create a descriptor set layout from bindings.
     * @param inVector Bindings vector.
     * @return The created layout handle.
     */
    VkDescriptorSetLayout buildDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>& inVector) const;

    /**
     * @brief Allocate descriptor sets from the pool.
     * @param outDescriptorSets Vector to store allocated sets.
     * @param descriptorSetLayout Layout for the sets.
     * @param count Number of sets to allocate (default MAX_FRAMES_IN_FLIGHT).
     */
    void buildDescriptorSets(std::vector<VkDescriptorSet>& outDescriptorSets, VkDescriptorSetLayout descriptorSetLayout, uint32_t count = MAX_FRAMES_IN_FLIGHT) const;

private:
    // Vulkan handles
    VkDevice deviceHandle;
    VkDescriptorPool descriptorPoolRef = VK_NULL_HANDLE;
};



/**
 * @class BaseDescriptors
 * @brief Base class for descriptor set management.
 */
class BaseDescriptors{
public:
    VkDescriptorSetLayout descriptorSetLayout{};
    std::vector<VkDescriptorSet> descriptorSets;        // automatically freed with descriptor pool

    BaseDescriptors(const VkDevice device);
    virtual ~BaseDescriptors();

protected:
    // Vulkan handles
    VkDevice deviceHandle;

    /**
     * @brief Create the descriptor set layout (implemented by subclasses).
     * @param builder Descriptor builder instance.
     */
    virtual void createDescriptorSetLayout(const DescriptorBuilder& builder) = 0;
};



/**
 * @class FrustumDescriptors
 * @brief Descriptors for frustum rendering and intersection shaders.
 * Manages uniform buffers for camera frustums.
 */
class FrustumDescriptors : public BaseDescriptors{   // shared also for intersection shader
public:
    FrustumDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    /**
     * @brief Create and populate descriptor sets.
     * @param builder Descriptor builder.
     * @param uniformBuffers Uniform buffers for frustums.
     */
    void createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& uniformBuffers);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};



/**
 * @class CamCubeDescriptors
 * @brief Descriptors for camera cube visualization.
 * Manages texture samplers for cube maps.
 */
class CamCubeDescriptors : public BaseDescriptors{
public:
    CamCubeDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    /**
     * @brief Create descriptor sets for cube textures.
     * @param builder Descriptor builder.
     * @param textureSamplerView Texture and sampler views.
     */
    void createDescriptorSets(const DescriptorBuilder& builder, const TextureSamplerView& textureSamplerView);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};



/**
 * @class OfflineDescriptors
 * @brief Descriptors for offline rendering.
 * Manages uniform buffers and images for offline render passes.
 */
class OfflineDescriptors : public BaseDescriptors{
public:
    OfflineDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    /**
     * @brief Create descriptor sets for offline rendering.
     * @param builder Descriptor builder.
     * @param offlineUniformBuffers Uniform buffers.
     * @param camManager Camera manager for updates.
     */
    void createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& offlineUniformBuffers, CamerasManager& camManager);

    /**
     * @brief Update descriptor sets with camera data.
     * @param camManager Camera manager.
     */
    void updateDescriptorSets(CamerasManager& camManager);

    /**
     * @brief Update descriptor sets for a specific frame.
     * @param currentFrame Frame index.
     * @param camManager Camera manager.
     */
    void updateDescriptorSets(uint32_t currentFrame, CamerasManager& camManager);

    /**
     * @brief Update with reconstruction uniforms.
     * @param currentFrame Frame index.
     * @param reconUniform Reconstruction uniforms.
     */
    void updateDescriptorSets(uint32_t currentFrame, NovelReconUniforms& reconUniform);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};



/**
 * @class RenderDescriptors
 * @brief Descriptors for main rendering pipeline.
 * Manages uniform buffers, samplers, and material textures.
 */
class RenderDescriptors : public BaseDescriptors{
public:
    VkDescriptorSetLayout samplerDescriptorSetLayout;
    std::vector<VkDescriptorSet> samplerDescriptorSets;

    RenderDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~RenderDescriptors();

    /**
     * @brief Create descriptor sets for rendering.
     * @param builder Descriptor builder.
     * @param renderUniforms Render uniforms.
     * @param meshData Mesh data.
     */
    void createDescriptorSets(const DescriptorBuilder& builder,
                              const RenderUniforms& renderUniforms,
                              const MeshUniforms& meshData);

    /**
     * @brief Update sampler descriptor sets.
     * @param sampler Sampler handle.
     * @param materials Material image views.
     */
    void updateSamplerDescriptorSets(const VkSampler& sampler, const std::vector<VkImageView>& materials);

    /**
     * @brief Update material descriptor sets.
     * @param materials Material buffer.
     */
    void updateMaterialDescriptosSets(VkBuffer materials);

    /**
     * @brief Update mesh data in descriptors.
     * @param meshData Mesh uniforms.
     */
    void updateMeshData(const MeshUniforms& meshData);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
    void createSamplerDescriptorSets(const DescriptorBuilder& builder, const VkSampler& sampler, const std::vector<VkImageView>& materials);
};


/**
 * @class ComputeDescriptors
 * @brief Descriptors for compute shaders.
 * Manages SSBOs and images for novel view synthesis.
 */
class ComputeDescriptors : public BaseDescriptors {
public:
    VkDescriptorSetLayout sharedDescriptorSetLayout;
    VkDescriptorSet sharedDescriptorSet;                // shared with offline render most likely

    ComputeDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~ComputeDescriptors();

    /**
     * @brief Create descriptor sets for compute.
     * @param builder Descriptor builder.
     * @param novelUniforms Novel uniforms.
     * @param camManager Camera manager.
     */
    void createDescriptorSets(const DescriptorBuilder& builder, const NovelUniforms& novelUniforms, CamerasManager& camManager);

    /**
     * @brief Update descriptor sets for a frame.
     * @param currentFrame Frame index.
     * @param camArraySSBOIn Input SSBOs.
     * @param intersectionsSSBOOut Output SSBOs.
     */
    void updateDescriptorSets(uint32_t currentFrame, const std::vector<VkBuffer>& camArraySSBOIn, const std::vector<VkBuffer>& intersectionsSSBOOut);

    /**
     * @brief Update shared image descriptors.
     * @param camManager Camera manager.
     */
    void updateSharedImageDescriptor(CamerasManager& camManager);

    /**
     * @brief Update image descriptors.
     * @param camManager Camera manager.
     */
    void updateImageDescriptors(CamerasManager& camManager);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

/**
 * @class PointCloudDescriptors
 * @brief Descriptors for point cloud rendering and processing.
 * Manages uniforms and metadata images for point clouds.
 */
class PointCloudDescriptors : public BaseDescriptors {
public:
    VkDescriptorSetLayout imageDescriptorSetLayout;
    std::vector<VkDescriptorSet> imageDescriptorSets;

    PointCloudDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~PointCloudDescriptors();

    /**
     * @brief Create descriptor sets for point clouds.
     * @param builder Descriptor builder.
     * @param pointCloudUniforms Point cloud uniforms.
     * @param metadataImageViews Metadata image views.
     */
    void createDescriptorSets(const DescriptorBuilder& builder, const PointCloudUniforms& pointCloudUniforms, const std::vector<VkImageView>& metadataImageViews);

    /**
     * @brief Update descriptor sets with uniforms.
     * @param pointCloudUniforms Point cloud uniforms.
     */
    void updateDescriptorSets(const PointCloudUniforms& pointCloudUniforms);

    /**
     * @brief Create image descriptor sets.
     * @param builder Descriptor builder.
     * @param metadataImageViews Metadata views.
     */
    void createImageDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkImageView>& metadataImageViews);

    /**
     * @brief Update image descriptor sets.
     * @param metadataImageViews Metadata views.
     */
    void updateImageDescriptorSets(const std::vector<VkImageView>& metadataImageViews);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

/**
 * @class ReduceDescriptors
 * @brief Descriptors for depth reduction compute shaders.
 * Manages samplers and images for mip reduction.
 */
class ReduceDescriptors : public BaseDescriptors {
public:
    ReduceDescriptors(const DescriptorBuilder& builder, const VkDevice device);

    /**
     * @brief Create descriptor sets for reduction.
     * @param builder Descriptor builder.
     * @param sampler Sampler for depth.
     * @param depthImages Depth image views.
     */
    void createDescriptorSets(const DescriptorBuilder& builder, VkSampler sampler, const std::array<VkImageView, 4>& depthImages);

    /**
     * @brief Update descriptor sets.
     * @param sampler Sampler.
     * @param depthImages Depth views.
     */
    void updateDescriptorSets(VkSampler sampler, const std::array<VkImageView, 4>& depthImages);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

/**
 * @class NovelSynthDescriptors
 * @brief Descriptors for novel view synthesis.
 * Manages SSBOs and images for synthesis shaders.
 */
class NovelSynthDescriptors : public BaseDescriptors {
public:
    VkDescriptorSetLayout debugDescriptorSetLayout;
    std::vector<VkDescriptorSet> debugDescriptorSets;

    NovelSynthDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~NovelSynthDescriptors();

    /**
     * @brief Create descriptor sets for synthesis.
     * @param builder Descriptor builder.
     * @param camArraySSBOIn Input SSBOs.
     * @param novelSynthUniforms Synth uniforms.
     */
    void createDescriptorSets(const DescriptorBuilder& builder, const std::vector<VkBuffer>& camArraySSBOIn, const NovelSynthUniforms& novelSynthUniforms);

    /**
     * @brief Update per-camera descriptor sets.
     * @param camArraySSBOIn Input SSBOs.
     * @param novelSynthUniforms Synth uniforms.
     */
    void updatePerCamDescriptorSets(const std::vector<VkBuffer>& camArraySSBOIn, const NovelSynthUniforms& novelSynthUniforms);

    /**
     * @brief Update per-resize descriptor sets.
     * @param resultImageViews Result image views.
     */
    void updatePerResizeDescriptorSets(const std::vector<VkImageView>& resultImageViews);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

/**
 * @class RayDataDescriptors
 * @brief Descriptors for ray data processing.
 */
class RayDataDescriptors : public BaseDescriptors {
public:
    VkDescriptorSetLayout storageDescriptorSetLayout;
    std::vector<VkDescriptorSet> storageDescriptorSets;

    RayDataDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~RayDataDescriptors();

    /**
     * @brief Create descriptor sets for ray data.
     * @param builder Descriptor builder.
     * @param novelSynthUniforms Synth uniforms.
     */
    void createDescriptorSets(const DescriptorBuilder& builder, const RayDataUniforms& novelSynthUniforms);

    /**
     * @brief Update storage descriptor sets.
     * @param novelSynthUniforms Synth uniforms.
     * @param currentFrame Frame index.
     */
    void updateStorageDescriptorSets(const RayDataUniforms& novelSynthUniforms, uint32_t currentFrame);

private:
    void createDescriptorSetLayout(const DescriptorBuilder& builder) override;
};

/**
 * @class NovelReconDescriptors
 * @brief Manages images and buffers for reconstruction.
 */
class NovelReconDescriptors : public BaseDescriptors {
public:
    VkDescriptorSetLayout resultDescriptorSetLayout;
    std::vector<VkDescriptorSet> resultDescriptorSets;

    NovelReconDescriptors(const DescriptorBuilder& builder, const VkDevice device);
    ~NovelReconDescriptors();

    /**
     * @brief Create descriptor sets for reconstruction.
     * @param builder Descriptor builder.
     * @param reconUniforms Recon uniforms.
     * @param camManager Camera manager.
     */
    void createDescriptorSets(const DescriptorBuilder& builder, const NovelReconUniforms& reconUniforms, CamerasManager& camManager);

    /**
     * @brief Update reference image descriptors.
     * @param camManager Camera manager.
     */
    void updateRefImageDescritporSets(CamerasManager& camManager);

    /**
     * @brief Update result descriptor sets.
     * @param resultImageViews Result views.
     * @param currentFrame Frame index.
     */
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
    CamCubeDescriptors camCubeDescriptors;
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