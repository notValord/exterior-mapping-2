#pragma once

#include "window.hpp"
#include "vulkanContext.hpp"
#include "swapchain.hpp"
#include "pipeline.hpp"
#include "commandManager.hpp"
#include "syncManager.hpp"
#include "util.hpp"
#include "mesh.hpp"
#include "descriptorManager.hpp"
#include "textures.hpp"
#include "uniforms.hpp"

class App {
public:
    App();
    ~App();
    void run();

private:
    Window appWindow;
    VulkanContext vulkanContext;
    SwapChain swapchain;
    DescriptorManager descripManager;
    GraphicsPipeline graphicsPipeline;
    CommandManager commandManager;
    SyncManager syncManager;
    MemoryManager memManager;
    Textures textures;
    Mesh mesh;
    Uniforms uniforms;

    uint32_t currentFrame = 0;

    void initVulkan();
    void mainLoop();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();
    void cleanup();
};