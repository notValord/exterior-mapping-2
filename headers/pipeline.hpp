#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fstream>
#include <vector>

struct AttachementsFormats;

class GraphicsPipeline{
public:
    VkPipeline graphicsPipeline;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;        // specifies the uniform variables in shader, todo rpivate?

    VkPipeline frustumPipeline = VK_NULL_HANDLE;
    VkRenderPass onTopRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout frustumPipelineLayout = VK_NULL_HANDLE;        // specifies the uniform variables in shader, todo rpivate?

    VkPipeline intersectionPipeline = VK_NULL_HANDLE;               // uses onTopRenderPass
    VkPipelineLayout intersectionPipelineLayout = VK_NULL_HANDLE;


    GraphicsPipeline(const VkDevice device, const AttachementsFormats& imageFormats, const VkDescriptorSetLayout descriptorSL,
        const std::string& vertexFile = "../shaders/vert.spv", const std::string& fragFile = "../shaders/frag.spv");
    ~GraphicsPipeline();

    void setupFrustumPipeline(const VkDescriptorSetLayout descriptorSL, const AttachementsFormats& imageFormats,
         const std::string& vertexFile = "../shaders/frustumVert.spv", const std::string& fragFile = "../shaders/frustumFrag.spv");

    void setupIntersectionPipeline(const VkDescriptorSetLayout descriptorSL, const AttachementsFormats& imageFormats,
         const std::string& vertexFile = "../shaders/lineVert.spv", const std::string& fragFile = "../shaders/lineFrag.spv");
private:
    std::string vertexShader;
    std::string fragmentShader;

    // required handles
    VkDevice deviceHandle;

    VkPipeline createGraphicsPipeline(const VkDescriptorSetLayout descriptorSL, const std::string& vertexShader, const std::string& fragmentShader, 
    VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass);
    VkPipeline createLinePipeline(const VkDescriptorSetLayout descriptorSL, const std::string& vertexShader, const std::string& fragmentShader, 
    VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass);
    VkRenderPass createRenderPass(const AttachementsFormats& imageFormats);
    VkRenderPass createOnTopRenderPass(const AttachementsFormats& imageFormats);
};

class ComputePipeline {
public:
    VkPipeline computePipeline;
    VkPipelineLayout pipelineLayout;

    ComputePipeline(const VkDevice device, const VkDescriptorSetLayout descriptorSL, const std::string& computeFile = "../shaders/compute.spv");
    ~ComputePipeline();

private:
    std::string computeShader;

    // required Vulkan handle
    VkDevice deviceHandle;
    void createComputePipeline(const VkDescriptorSetLayout descriptorSL);
};
