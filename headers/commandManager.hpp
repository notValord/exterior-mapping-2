#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// system includes
#include <vector>
#include <iostream>
#include <array>

extern const size_t MAX_FRAMES_IN_FLIGHT;
struct QueueFamilyIndices;

class CamerasManager;
class GraphicPipeline;
class ComputePipeline;

class CommandManager{
public:
    // maybe have specilized commandpool for different queues
    std::vector<VkCommandBuffer> commandBuffers;        // Freed with commandPool
    std::vector<VkCommandBuffer> offlineCommandBuffers;

    CommandManager(const QueueFamilyIndices& familyIndices, const VkDevice device);
    ~CommandManager();

    VkCommandPool getTransferCommandPool();

private:
    VkCommandPool graphicsCommandPool;

    // Vulkan handles
    VkDevice deviceHandle;

    void createCommandPool(const QueueFamilyIndices& familyIndices);
    void createCommandBuffers();
};

struct ComputeDataBuffer{
    VkBuffer dataBuffer = VK_NULL_HANDLE;
    VkBuffer counterBuffer = VK_NULL_HANDLE;

    ComputeDataBuffer() = default;
    ComputeDataBuffer(VkBuffer data, VkBuffer counter) : dataBuffer(data), counterBuffer(counter) {}

    bool isEmpty() const {
        return dataBuffer == VK_NULL_HANDLE || counterBuffer == VK_NULL_HANDLE;
    }
};

class CommandRecorder{
public:
    CommandRecorder() {};
    ~CommandRecorder() {};

    void setPipeline(const GraphicPipeline& pipeline, VkRenderPass render, VkExtent2D extent);
    void setPipeline(const ComputePipeline& pipeline);

    void setRenderBuffers(VkBuffer vertexBuffer, uint32_t indexCount, VkBuffer indexBuffer);
    void setComputeBuffer(VkBuffer dataBuffer, VkBuffer countBuffer);

    void recordRenderScene(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet>& descriptorSets, VkFramebuffer framebuffer, const std::vector<glm::mat4>& pushConstants = std::vector<glm::mat4>{});
    
    void recordRenderCamCube(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet>& descriptorSets, VkFramebuffer framebuffer, const CamerasManager& camManager);


    void recordCompute(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet>& descriptorSets, std::pair<uint32_t, uint32_t> dispatchSize);
private:
    VkPipeline graphicPipeline = VK_NULL_HANDLE;
    VkPipelineLayout graphicPipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkExtent2D swapchainExtent = {0, 0};

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    uint32_t indexCount = 0;

    VkPipeline computePipeline = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    ComputeDataBuffer computeBuffer;

    void clearComputeBuffer(VkCommandBuffer commandBuffer);
    void barrierComputeBuffer(VkCommandBuffer commandBuffer,
                              VkAccessFlags srcMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                              VkAccessFlags dstMask = VK_ACCESS_SHADER_WRITE_BIT,
                              VkPipelineStageFlagBits srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VkPipelineStageFlagBits dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT); 
};