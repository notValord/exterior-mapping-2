#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fstream>
#include <vector>

struct AttachementsFormats;
class DescriptorManager;

enum class VertexInputFlags : uint32_t {
    POS_COL_UV,
    POS_COL,
    POS,
    NONE
};

struct DepthStencilFlags{
    VkBool32 testEnable;
    VkBool32 writeEnable;
};

struct RasterizationFlags{
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
};

struct GraphicShaders {
    std::string vert;
    std::string frag;
};

struct ComputeShader {
    std::string comp;
};


struct GraphicSetup {
    GraphicShaders shaderFiles;
    VertexInputFlags vertexInput;
    VkPrimitiveTopology topology;
    DepthStencilFlags depthFlags;
    RasterizationFlags rastFlags;
};

struct PipelineLayoutSetup {
    std::vector<VkDescriptorSetLayout> descriptorVec;
    uint32_t pushConstants = 0;

    PipelineLayoutSetup(std::initializer_list<VkDescriptorSetLayout> layouts, uint32_t push = 0)
        : descriptorVec(layouts), pushConstants(push) {}
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

    VkPipelineLayout createPipelineLayout(const PipelineLayoutSetup& layoutSetup) const;
    VkPipeline createComputePipeline(VkPipelineLayout pipelineLayout, const ComputeShader& shaderFile) const;
    VkRenderPass createRenderPass(const AttachementDescription& colorAttachment, const AttachementDescription& depthAttachment) const;
    VkPipeline createGraphicsPipeline(VkPipelineLayout pipelineLayout, VkRenderPass renderPass, const GraphicSetup& pipelineSetup) const;
private:
    VkDevice deviceHandle;

    VkShaderModule createShaderModule(const std::string& fileName) const;
};



class RenderPassManager {
public:
    VkRenderPass renderPass;
    VkRenderPass onTopRenderPass;

    RenderPassManager(VkDevice device, AttachementsFormats attachFormats, const PipelineBuilder& builder);
    ~RenderPassManager();
private:
    VkDevice deviceHandle;
};



class GraphicPipeline {
public:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    GraphicPipeline(VkDevice device, VkRenderPass renderPass, const PipelineLayoutSetup& layoutSetup, const GraphicSetup& pipelineSetup,
                    const PipelineBuilder& builder);
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

    ComputePipeline(VkDevice device, const PipelineLayoutSetup& layoutSetup, const ComputeShader& shaderFile, const PipelineBuilder& builder);
    ~ComputePipeline();

    // // Not allowing copy constructors
    // ComputePipeline(const ComputePipeline&) = delete;
    // ComputePipeline& operator=(const ComputePipeline&) = delete;

    // // Not allowing move constructors
    // ComputePipeline(ComputePipeline&&) noexcept = delete;
    // ComputePipeline& operator=(ComputePipeline&&) noexcept = delete;


private:
    const ComputeShader computeShader;

    // required Vulkan handle
    VkDevice deviceHandle;
};



class PipelineManager {
private:
    PipelineBuilder builder;        // needs to be initialized first

    static inline const GraphicShaders renderFiles    = GraphicShaders{"../shaders/vert.spv", "../shaders/frag.spv"};
    static inline const GraphicShaders frustumFiles   = GraphicShaders{"../shaders/frustumVert.spv", "../shaders/frustumFrag.spv"};
    static inline const GraphicShaders lineFiles      = GraphicShaders{"../shaders/lineVert.spv", "../shaders/lineFrag.spv"};
    static inline const GraphicShaders camCuberFiles  = GraphicShaders{"../shaders/camCubeVert.spv", "../shaders/camCubeFrag.spv"};
    static inline const GraphicShaders offlineFiles   = GraphicShaders{"../shaders/offlineVert.spv", "../shaders/offlineFrag.spv"};
    static inline const GraphicShaders pointRendFiles = GraphicShaders{"../shaders/pointVert.spv", "../shaders/pointFrag.spv"};
    static inline const ComputeShader intersectFile   =  ComputeShader{"../shaders/compute.spv"};
    static inline const ComputeShader pointCloudFile  =  ComputeShader{"../shaders/pointCloud.spv"};

    GraphicSetup setupRenderPipeline();
    GraphicSetup setupFrustumPipeline();
    GraphicSetup setupLinePipeline();
    GraphicSetup setupCamCubePipeline();
    GraphicSetup setupOfflinePipeline();
    GraphicSetup setupPointPipeline();

public:
    RenderPassManager renderPassMan;

    ComputePipeline intersectPipeline;
    ComputePipeline pointCloudPipeline;

    GraphicPipeline renderPipeline;
    GraphicPipeline frustumPipeline;
    GraphicPipeline linePipeline;
    GraphicPipeline camCubePipeline;
    GraphicPipeline offlinePipeline;
    GraphicPipeline pointPipeline;

    PipelineManager(VkDevice device, const AttachementsFormats& imageFormats, DescriptorManager& descrMan);
};
