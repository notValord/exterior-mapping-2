#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fstream>
#include <vector>

#include <structs.hpp>

struct AttachementsFormats;
class DescriptorManager;

class GraphicsPipeline{
public:
    VkPipeline graphicsPipeline;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;

    VkPipeline frustumPipeline = VK_NULL_HANDLE;
    VkRenderPass onTopRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout frustumPipelineLayout = VK_NULL_HANDLE;

    VkPipeline intersectionPipeline = VK_NULL_HANDLE;               // uses onTopRenderPass
    VkPipelineLayout intersectionPipelineLayout = VK_NULL_HANDLE;

    VkPipeline camCubePipeline = VK_NULL_HANDLE;                    // uses onTopRenderPass
    VkPipelineLayout camCubePipelineLayout = VK_NULL_HANDLE;

    VkPipeline offlineRenderPipeline = VK_NULL_HANDLE;              // uses graphics renderpass
    VkPipelineLayout offlineRenderPipelineLayout = VK_NULL_HANDLE;


    GraphicsPipeline(const VkDevice device, const AttachementsFormats& imageFormats, const VkDescriptorSetLayout descriptorSL,
        const std::string& vertexFile = "../shaders/vert.spv", const std::string& fragFile = "../shaders/frag.spv");
    ~GraphicsPipeline();

    void setupFrustumPipeline(const VkDescriptorSetLayout descriptorSL, const AttachementsFormats& imageFormats,
         const std::string& vertexFile = "../shaders/frustumVert.spv", const std::string& fragFile = "../shaders/frustumFrag.spv");

    void setupIntersectionPipeline(const VkDescriptorSetLayout descriptorSL, const AttachementsFormats& imageFormats,
         const std::string& vertexFile = "../shaders/lineVert.spv", const std::string& fragFile = "../shaders/lineFrag.spv");
    
    void setupCamCubePipeline(const VkDescriptorSetLayout descriptorSL, const AttachementsFormats& imageFormats,
         const std::string& vertexFile = "../shaders/camCubeVert.spv", const std::string& fragFile = "../shaders/camCubeFrag.spv");

    void setupOfflinePipeline(const VkDescriptorSetLayout descriptorSL, const VkDescriptorSetLayout sharedDescriptorSL,
         const std::string& vertexFile = "../shaders/offlineVert.spv", const std::string& fragFile = "../shaders/offlineFrag.spv");
private:
    std::string vertexShader;
    std::string fragmentShader;

    // required handles
    VkDevice deviceHandle;

    VkPipeline createGraphicsPipeline(const std::string& vertexShader, const std::string& fragmentShader, VkPipelineLayout& pipelineLayout,
         VkRenderPass& renderPass, VertexInputFlags vertexInput, VkPrimitiveTopology topology, DepthStencilFlags depthFlags, RasterizationFlags rastFlags);
    VkRenderPass createRenderPass(const AttachementsFormats& imageFormats);
    VkRenderPass createOnTopRenderPass(const AttachementsFormats& imageFormats);

    void createPipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSL, VkPipelineLayout& pipelineLayout, uint32_t pushConstant = 0);
};

// class ComputePipeline {
// public:
//     VkPipeline computePipeline;
//     VkPipelineLayout pipelineLayout;

//     VkPipeline pointCloudPipeline;
//     VkPipelineLayout pointCloudPipelineLayout;

//     ComputePipeline(const VkDevice device, const VkDescriptorSetLayout descriptorSL, const VkDescriptorSetLayout sharedDescriptorSL, const VkDescriptorSetLayout cloudDescriptorSL,
//                     const std::string& computeFile = "../shaders/compute.spv", const std::string& pointCloudFile = "../shaders/pointCloud.spv");
//     ~ComputePipeline();

// private:
//     std::string computeShader;

//     // required Vulkan handle
//     VkDevice deviceHandle;
//     void createComputePipeline(VkPipelineLayout& pipelineLayout, std::string shaderFile, VkPipeline& pipeline);
//     void createPipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSL, VkPipelineLayout& pipelineLayout, uint32_t pushConstant = 0);
// };

struct GraphicSetup {
    VertexInputFlags vertexInput;
    DepthStencilFlags depthFlags;
    RasterizationFlags rastFlags;
    VkPrimitiveTopology topology;
};

class GraphicPipeline {
public:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    GraphicPipeline(const VkDevice device, const VkRenderPass renderPass, const VkDescriptorSetLayout descriptorSL, 
                     const GraphicShaders& shaderFiles, const GraphicSetup& pipelineSetup, const PipelineBuilder& builder);
    ~GraphicPipeline();

private:
    const GraphicShaders vertFragShaders;
    VkRenderPass renderPass = VK_NULL_HANDLE;

    // required Vulkan handle
    VkDevice deviceHandle;
};

class ComputePipeline {
public:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    ComputePipeline(const VkDevice device, const VkDescriptorSetLayout descriptorSL, const VkDescriptorSetLayout sharedDescriptorSL, const ComputeShader& shaderFile,
                    const PipelineBuilder& builder);
    ~ComputePipeline();

    // Not allowing copy constructors
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    // Not allowing move constructors
    ComputePipeline(ComputePipeline&&) noexcept = delete;
    ComputePipeline& operator=(ComputePipeline&&) noexcept = delete;


private:
    const ComputeShader computeShader;

    // required Vulkan handle
    VkDevice deviceHandle;
};

struct AttachementDescription {
    VkFormat imageFormat;
    VkAttachmentLoadOp loadOp;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout;
};

class PipelineBuilder {
public:
    PipelineBuilder(const VkDevice device);

    VkPipelineLayout createPipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSL, uint32_t pushConstant = 0) const;
    VkPipeline createComputePipeline(VkPipelineLayout pipelineLayout, const ComputeShader& shaderFile) const;
    VkRenderPass createRenderPass(const AttachementDescription& colorAttachment, const AttachementDescription& depthAttachment) const;
    VkPipeline createGraphicsPipeline(const GraphicShaders& shaderFiles, VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass, const GraphicSetup& pipelineSetup) const;
private:
    VkDevice deviceHandle;

    VkShaderModule createShaderModule(const std::string& fileName) const;
};

class RenderPassManager {
public:
    VkRenderPass renderPass;
    VkRenderPass onTopRenderPass;

    RenderPassManager(const VkDevice device, AttachementsFormats attachFormats, PipelineBuilder& builder);
    ~RenderPassManager();
private:
    VkDevice deviceHandle;
};

class PipelineManager {
public:
    RenderPassManager renderPassMan;

    ComputePipeline intersectPipeline;
    ComputePipeline pointCloudPipeline;

    GraphicPipeline renderPipeline;
    GraphicPipeline frustumPipeline;
    GraphicPipeline intersectionPipeline;

    PipelineManager(const VkDevice device, const AttachementsFormats& imageFormats, DescriptorManager& descrMan);
private:
    PipelineBuilder builder;

    const GraphicShaders renderFiles    = GraphicShaders{"../shaders/vert.spv", "../shaders/frag.spv"};
    const GraphicShaders frustumFiles   = GraphicShaders{"../shaders/frustumVert.spv", "./shaders/frustumFrag.spv"};
    const GraphicShaders lineFiles      = GraphicShaders{"../shaders/lineVert.spv", "../shaders/lineFrag.spv"};
    const GraphicShaders camCuberFiles  = GraphicShaders{"../shaders/camCubeVert.spv", "../shaders/camCubeFrag.spv"};
    const GraphicShaders offlineFiles   = GraphicShaders{"../shaders/offlineVert.spv", "../shaders/offlineFrag.spv"};
    const ComputeShader intersectFile   =  ComputeShader{"../shaders/compute.spv"};
    const ComputeShader pointCloudFile  =  ComputeShader{"../shaders/pointCloud.spv"};
};
