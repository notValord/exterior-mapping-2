#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE     // GLM uses openGl depth range -1 to 1
#include <glm/glm.hpp>

// system includes
#include <vector>
#include <iostream>
#include <array>

extern const size_t MAX_FRAMES_IN_FLIGHT;
struct QueueFamilyIndices;
struct SubMesh;

class CamerasManager;
class GraphicPipeline;
class ComputePipeline;

/**
 * @class CommandManager
 * @brief Manages Vulkan command pools and command buffers.
 */
class CommandManager{
public:
    // maybe have specilized commandpool for different queues
    std::vector<VkCommandBuffer> commandBuffers;        // Freed with commandPool
    std::vector<VkCommandBuffer> offlineCommandBuffers;

    /**
     * @brief Initialize command manager with device and queue family info.
     * @param familyIndices Queue family indices for command pool creation.
     * @param device Vulkan logical device.
     */
    CommandManager(const QueueFamilyIndices& familyIndices, const VkDevice device);

    ~CommandManager();

    /**
     * @brief Get the command pool for graphics and transfer operations.
     * 
     * Currently only one command pool is used for both graphics and transfer commands, but this allows for future separation if needed.
     * @return The Vulkan command pool handle.
     */
    VkCommandPool getTransferCommandPool();

private:
    VkCommandPool graphicsCommandPool;

    // Vulkan handles
    VkDevice deviceHandle;

    void createCommandPool(const QueueFamilyIndices& familyIndices);
    void createCommandBuffers();
};

/**
 * @struct ComputeDataBuffer
 * @brief Holds data and counter buffers for compute operations.
 */
struct ComputeDataBuffer{
    VkBuffer dataBuffer = VK_NULL_HANDLE;
    VkBuffer counterBuffer = VK_NULL_HANDLE;

    ComputeDataBuffer() = default;

    /**
     * @brief Construct with specific buffers.
     * @param data Data buffer handle.
     * @param counter Counter buffer handle.
     */
    ComputeDataBuffer(VkBuffer data, VkBuffer counter) : dataBuffer(data), counterBuffer(counter) {}

    bool isEmpty() const {
        return dataBuffer == VK_NULL_HANDLE || counterBuffer == VK_NULL_HANDLE;
    }
};

/**
 * @class CommandRecorder
 * @brief Utility for recording Vulkan command buffers with common operations.
 * 
 * Provides methods to set pipelines, buffers, and record render/compute commands with proper barriers and state management.
 */
class CommandRecorder{
public:
    CommandRecorder() {};
    ~CommandRecorder() {};

    /**
     * @brief Set graphics pipeline and render pass for recording.
     * @param pipeline Graphics pipeline to use.
     * @param render Render pass for the pipeline.
     * @param extent Swapchain extent for viewport/scissor.
     */
    void setPipeline(const GraphicPipeline& pipeline, VkRenderPass render, VkExtent2D extent);

    /**
     * @brief Set compute pipeline for recording.
     * @param pipeline Compute pipeline to use.
     */
    void setPipeline(const ComputePipeline& pipeline);

    /**
     * @brief Set vertex and index buffers for rendering.
     * @param vertexBuffer Vertex buffer handle.
     * @param indexCount Number of indices.
     * @param indexBuffer Index buffer handle.
     */
    void setRenderBuffers(VkBuffer vertexBuffer, uint32_t indexCount, VkBuffer indexBuffer);

    /**
     * @brief Set data and counter buffers for compute operations.
     * @param dataBuffer Compute data buffer.
     * @param countBuffer Atomic counter buffer.
     */
    void setComputeBuffer(VkBuffer dataBuffer, VkBuffer countBuffer);

    /**
     * @brief Clear a storage image to zero.
     * @param commandBuffer Command buffer to record into.
     * @param storageImage Image to clear.
     */
    void clearStorageImage(VkCommandBuffer commandBuffer, VkImage storageImage);

    /**
     * @brief Insert memory barrier for storage images.
     * @param commandBuffer Command buffer to record into.
     * @param storageImages Images to barrier.
     * @param layerCount Number of array layers.
     */
    void barrierStorageImage(VkCommandBuffer commandBuffer, const std::vector<VkImage>& storageImages, uint32_t layerCount);

    /**
     * @brief Insert memory barrier for storage buffer.
     * @param commandBuffer Command buffer to record into.
     * @param storageBuffer Buffer to barrier.
     * @param srcMask Source access mask.
     * @param dstMask Destination access mask.
     * @param srcStage Source pipeline stage.
     * @param dstStage Destination pipeline stage.
     */
    void barrierStorageBuffer(VkCommandBuffer commandBuffer, VkBuffer storageBuffer, VkAccessFlags srcMask, VkAccessFlags dstMask, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage);

    /**
     * @brief Record a basic scene render pass.
     * @param commandBuffer Command buffer to record into.
     * @param descriptorSets Descriptor sets to bind.
     * @param framebuffer Framebuffer to render to.
     * @param pushConstants Optional push constants (model-view-projection matrices).
     */
    void recordRenderScene(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet>& descriptorSets, VkFramebuffer framebuffer, const std::vector<glm::mat4>& pushConstants = std::vector<glm::mat4>{});

    /**
     * @brief Record multi-mesh scene rendering with materials.
     * @param commandBuffer Command buffer to record into.
     * @param descriptorSets Descriptor sets to bind.
     * @param framebuffer Framebuffer to render to.
     * @param pushConstants Push constants for transforms.
     * @param meshes Sub-meshes to render.
     * @param materialSets Material descriptor sets.
     */
    void recordMultiScene(VkCommandBuffer commandBuffer,
                          const std::vector<VkDescriptorSet>& descriptorSets,
                          VkFramebuffer framebuffer,
                          const std::vector<glm::mat4>& pushConstants,
                          const std::vector<SubMesh>& meshes,
                          const std::vector<VkDescriptorSet>& materialSets,
                          bool clear = true);
    
    /**
     * @brief Record camera cube visualization.
     * @param commandBuffer Command buffer to record into.
     * @param descriptorSets Descriptor sets to bind.
     * @param framebuffer Framebuffer to render to.
     * @param camManager Camera manager for view matrices.
     */
    void recordRenderCamCube(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet>& descriptorSets, VkFramebuffer framebuffer, const CamerasManager& camManager);

    /**
     * @brief Record compute dispatch.
     * @param commandBuffer Command buffer to record into.
     * @param descriptorSets Descriptor sets to bind.
     * @param dispatchSize Workgroup dispatch dimensions (x, y).
     */
    void recordCompute(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet>& descriptorSets, std::pair<uint32_t, uint32_t> dispatchSize, VkQueryPool timestampQuery, uint32_t queryId = -1);

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