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
#include "memManager.hpp"
#include "camManager.hpp"
#include "inputManager.hpp"

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
    GraphicsPipeline graphicsPipeline;
    SyncManager syncManager;
    Textures textures;
    Mesh mesh;
    Uniforms uniforms;

    CamerasManager camManager;
    InputManager inputManager;

    uint32_t currentFrame = 0;

    void initVulkan();
    void mainLoop();
    void handleResize();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();
    void cleanup();
};