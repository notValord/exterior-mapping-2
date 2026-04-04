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

/**
 * @brief Main application class for the Vulkan-based 3D renderer with novel view synthesis.
 */
class App {
public:
    App();
    ~App();
    void run();

private:
    /** @brief Window manager for GLFW window handling. */
    Window appWindow;
    /** @brief Vulkan context for device and instance management. */
    VulkanContext vulkanContext;
    /** @brief Command buffer manager for recording and submitting commands. */
    CommandManager commandManager;
    /** @brief Memory manager for Vulkan memory allocation. */
    MemoryManager memManager;
    /** @brief Swapchain manager for presentation. */
    SwapChain swapchain;
    /** @brief Descriptor set manager for Vulkan descriptors. */
    DescriptorManager descripManager;
    /** @brief Pipeline manager for graphics and compute pipelines. */
    PipelineManager pipelineManager;
    /** @brief Synchronization manager for fences and semaphores. */
    SyncManager syncManager;
    /** @brief Texture manager for loading and managing textures. */
    TexturesManager textureManager;
    /** @brief Mesh manager for 3D model loading and rendering. */
    Mesh mesh;

    /** @brief Camera manager for handling multiple cameras and novel views. */
    CamerasManager camManager;
    /** @brief Uniform buffer manager for shader uniforms. */
    UniformManager uniformManager;
    /** @brief Input manager for user input and ImGui. */
    InputManager inputManager;
    /** @brief Debug utilities for visualization. */
    DebugUtil debugUtil;

    /** @brief Command recorder for simplifying command buffer operations. */
    CommandRecorder commandRecorder;

    /** @brief Current frame index for frame-in-flight management. */
    uint32_t currentFrame = 0;

    void mainLoop();
    
    void drawFrame();
    void renderOfflineImages();

    void handleResize();
    void changeScene();

    // Command buffer recordings

    /**
     * @brief Records commands to compute reduced depth pyramid for novel synthesis.
     * @param commandBuffer The command buffer to record into.
     */
    void computeReducedDepthPyramid(VkCommandBuffer commandBuffer);

    /**
     * @brief Records commands to draw offline rendering to a framebuffer.
     * @param commandBuffer The command buffer to record into.
     * @param framebuffer The target framebuffer.
     */
    void drawOffline(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);

    /**
     * @brief Records commands to draw the main scene.
     * @param commandBuffer The command buffer to record into.
     * @param framebuffer The target framebuffer.
     * @param renderView The camera view for rendering.
     */
    void drawScene(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, const Camera& renderView);

    /**
     * @brief Records commands for novel view synthesis computation.
     * @param commandBuffer The command buffer to record into.
     */
    void computeNovel(VkCommandBuffer commandBuffer);

    /**
     * @brief Records commands for the new novel view synthesis pipeline.
     * @param commandBuffer The command buffer to record into.
     */
    void computeNewNovel(VkCommandBuffer commandBuffer);

    /**
     * @brief Records commands to compute point cloud data.
     * @param commandBuffer The command buffer to record into.
     */
    void computePointCloud(VkCommandBuffer commandBuffer);

    /**
     * @brief Records commands for debug visualization.
     * @param framebuffer The target framebuffer.
     */
    void drawDebug(VkFramebuffer framebuffer);

    /**
     * @brief Records commands to draw frustum debug visualization.
     * @param commandBuffer The command buffer to record into.
     * @param framebuffer The target framebuffer.
     */
    void drawFrustumDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);

    /**
     * @brief Records commands to draw camera cube debug visualization.
     * @param commandBuffer The command buffer to record into.
     * @param framebuffer The target framebuffer.
     */
    void drawCamCubeDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);

    /**
     * @brief Records commands to draw intersection debug visualization.
     * @param commandBuffer The command buffer to record into.
     * @param framebuffer The target framebuffer.
     */
    void drawIntersectionDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);

    /**
     * @brief Records commands to draw point cloud debug visualization.
     * @param commandBuffer The command buffer to record into.
     * @param framebuffer The target framebuffer.
     */
    void drawPointCloudDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
};