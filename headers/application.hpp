#pragma once

// User defined headers
#include <window.hpp>
#include <vulkanContext.hpp>
#include <swapchain.hpp>
#include <pipeline.hpp>
#include <commandManager.hpp>
#include <syncManager.hpp>
#include <util.hpp>
#include <mesh.hpp>
#include <descriptorManager.hpp>
#include <textures.hpp>
#include <uniforms.hpp>
#include <memManager.hpp>
#include <camManager.hpp>
#include <inputManager.hpp>
#include <debug.hpp>

class App {
public:
    App();
    ~App();
    void run();

private:
    Window appWindow;
    VulkanContext vulkanContext;
    CommandManager commandManager;
    MemoryManager memManager;
    SwapChain swapchain;
    DescriptorManager descripManager;
    PipelineManager pipelineManager;
    // GraphicsPipeline graphicsPipeline;
    // ComputePipeline computePipeline;
    SyncManager syncManager;
    TexturesManager textureManager;
    Mesh mesh;

    CamerasManager camManager;
    UniformManager uniformManager;
    InputManager inputManager;
    DebugUtil debugUtil;

    uint32_t currentFrame = 0;

    void mainLoop();
    
    void drawFrame();
    void renderOfflineImages();

    void handleResize();

    // Command buffer recordings
    void recordCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
    void recordFrustumCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
    void recordCamCubeCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
    void recordOfflineCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
    void recordLineCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
    void recordComputeCommandBuffer(VkCommandBuffer commandBuffer);
    void recordPointCloudCommandBuffer(VkCommandBuffer commandBuffer);
    void recordPointCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
};