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
    VkPipelineLayout pipelineLayout;        // specifies the uniform variables in shader

    GraphicsPipeline(const VkDevice& device, const AttachementsFormats& imageFormats, const VkDescriptorSetLayout& descriptorSL,
        const std::string& vertexFile = "../shaders/vert.spv", const std::string& fragFile = "../shaders/frag.spv");
    ~GraphicsPipeline();

private:
    std::string vertexShader;
    std::string fragmentShader;

    // required handles
    VkDevice deviceHandle;

    VkShaderModule createShaderModule(const std::string& fileName);
    void createGraphicsPipeline(const VkDescriptorSetLayout& descriptorSL);
    void createRenderPass(const AttachementsFormats& imageFormats);
};
