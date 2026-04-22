#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fstream>
#include <vector>

struct AttachementsFormats;
class DescriptorManager;

/**
 * @enum VertexInputFlags
 * @brief Configures vertex input formats used by different graphics pipelines.
 */
enum class VertexInputFlags : uint32_t {
    POS_COL_UV_NORM,
    POS_COL_CAMID,
    POS_COL,
    POS,
    NONE
};

/**
 * @struct DepthStencilFlags
 * @brief Depth and stencil state configuration for graphics pipelines.
 */
struct DepthStencilFlags{
    VkBool32 testEnable;
    VkBool32 writeEnable;
};

/**
 * @struct RasterizationFlags
 * @brief Controls rasterization behavior for graphics pipelines.
 */
struct RasterizationFlags{
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
};

/**
 * @struct GraphicShaders
 * @brief File paths for vertex and fragment shaders used by a graphics pipeline.
 */
struct GraphicShaders {
    std::string vert;
    std::string frag;
};

/**
 * @struct ComputeShader
 * @brief File path for a compute shader used by a compute pipeline.
 */
struct ComputeShader {
    std::string comp;
};


/**
 * @struct GraphicSetup
 * @brief Configuration for creating a graphics pipeline.
 */
struct GraphicSetup {
    GraphicShaders shaderFiles;
    VertexInputFlags vertexInput;
    VkPrimitiveTopology topology;
    DepthStencilFlags depthFlags;
    RasterizationFlags rastFlags;
};

/**
 * @struct PipelineLayoutSetup
 * @brief Descriptor set layouts and push constant configuration.
 */
struct PipelineLayoutSetup {
    std::vector<VkDescriptorSetLayout> descriptorVec;
    uint32_t pushConstants = 0;

    PipelineLayoutSetup(std::initializer_list<VkDescriptorSetLayout> layouts, uint32_t push = 0)
        : descriptorVec(layouts), pushConstants(push) {}
};

/**
 * @struct AttachementDescription
 * @brief Describes a render pass attachment and its layout transitions.
 */
struct AttachementDescription {
    VkFormat imageFormat;
    VkAttachmentLoadOp loadOp;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout;
};

/**
 * @class PipelineBuilder
 * @brief Helper for creating Vulkan pipeline layouts, render passes, and pipelines.
 */
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



/**
 * @class RenderPassManager
 * @brief Creates and owns the render passes used by the renderer.
 */
class RenderPassManager {
public:
    VkRenderPass renderPass;        ///< Main render pass.
    VkRenderPass onTopRenderPass;   ///< Secondary render pass for overlay rendering.

    RenderPassManager(VkDevice device, AttachementsFormats attachFormats, const PipelineBuilder& builder);
    ~RenderPassManager();
private:
    VkDevice deviceHandle; ///< Vulkan logical device used to destroy render passes.
};



/**
 * @class GraphicPipeline
 * @brief Encapsulates a Vulkan graphics pipeline and its layout.
 */
class GraphicPipeline {
public:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    GraphicPipeline(VkDevice device, VkRenderPass renderPass, const PipelineLayoutSetup& layoutSetup, const GraphicSetup& pipelineSetup,
                    const PipelineBuilder& builder);
    ~GraphicPipeline();

private:
    const GraphicShaders vertFragShaders;               // Shader file paths for this pipeline.
    VkRenderPass renderPass = VK_NULL_HANDLE;

    // required Vulkan handle
    VkDevice deviceHandle;
};



/**
 * @class ComputePipeline
 * @brief Encapsulates a Vulkan compute pipeline and its layout.
 */
class ComputePipeline {
public:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    ComputePipeline(VkDevice device, const PipelineLayoutSetup& layoutSetup, const ComputeShader& shaderFile, const PipelineBuilder& builder);
    ~ComputePipeline();

private:
    const ComputeShader computeShader;          // Compute shader file path

    // required Vulkan handle
    VkDevice deviceHandle;
};



/**
 * @class PipelineManager
 * @brief Builds and owns the application's graphics and compute pipelines.
 */
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

    static inline const ComputeShader rayDataFile     =  ComputeShader{"../shaders/rayData.spv"};
    static inline const ComputeShader reduceDepthFile =  ComputeShader{"../shaders/reduce.spv"};
    static inline const ComputeShader novelSynthFile  =  ComputeShader{"../shaders/novelSynth.spv"};
    static inline const ComputeShader novelReconFile  =  ComputeShader{"../shaders/novelReconstruct.spv"};

    GraphicSetup setupRenderPipeline();
    GraphicSetup setupRenderTransparentPipeline();
    GraphicSetup setupFrustumPipeline();
    GraphicSetup setupLinePipeline();
    GraphicSetup setupCamCubePipeline();
    GraphicSetup setupOfflinePipeline();
    GraphicSetup setupPointPipeline();

public:
    RenderPassManager renderPassMan;

    ComputePipeline intersectPipeline;
    ComputePipeline pointCloudPipeline;
    ComputePipeline rayDataPipeline;
    ComputePipeline reduceDepthPipeline;
    ComputePipeline novelSynthPipeline;
    ComputePipeline novelReconPipeline;

    GraphicPipeline renderPipeline;
    GraphicPipeline renderTransparentPipeline;
    GraphicPipeline frustumPipeline;
    GraphicPipeline linePipeline;
    GraphicPipeline camCubePipeline;
    GraphicPipeline offlinePipeline;
    GraphicPipeline pointPipeline;

    PipelineManager(VkDevice device, const AttachementsFormats& imageFormats, DescriptorManager& descrMan);
};