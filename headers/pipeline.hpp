#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fstream>
#include <vector>

struct AttachementsFormats;

enum class VertexInputFlags {
    POS_COL_UV,
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

    void setupOfflinePipeline(const VkDescriptorSetLayout descriptorSL,
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

class ComputePipeline {
public:
    VkPipeline computePipeline;
    VkPipelineLayout pipelineLayout;

    ComputePipeline(const VkDevice device, const VkDescriptorSetLayout descriptorSL, const VkDescriptorSetLayout sharedDescriptorSL, const std::string& computeFile = "../shaders/compute.spv");
    ~ComputePipeline();

private:
    std::string computeShader;

    // required Vulkan handle
    VkDevice deviceHandle;
    void createComputePipeline(const VkDescriptorSetLayout descriptorSL, const VkDescriptorSetLayout sharedDescriptorSL);
};
