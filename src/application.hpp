#pragma once

#include "window.hpp"
#include "vulkanContext.hpp"
#include "swapchain.hpp"

class App {
public:
    Window appWindow;
    VulkanContext vulkanContext;
    SwapChain swapchain;

    App();
    ~App();
    void run();

private:
    void initVulkan();
    void mainLoop();
    void cleanup();
};