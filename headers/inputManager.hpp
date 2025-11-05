#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <chrono>

#include "ui.hpp"

class CamerasManager;

class FPSCounter{
public:
    double fps = 0.0;
    
    void frame();
private:
    std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();
    uint32_t frameCount = 0;
};

class InputManager {
public:
    InputManager(GLFWwindow* window, CamerasManager& camManager, const AttachementsFormats& imageFormats, const std::vector<VkImageView>& swapChainImageViews,
     const PhysicalDeviceInstance& physicalDeviceInstance, const VkQueue& graphicsQueue, const QueueFamilyIndices& familyIndices, VkExtent2D& swapChainExtent);
    ~InputManager();

    void setCallbacks();

    void frame();
    VkCommandBuffer recordUI(uint32_t currentFrame, uint32_t imageIndex);

    void imguiResize(const std::vector<VkImageView>& swapChainImageViews, const VkExtent2D& swapChainExtent);

private:
    FPSCounter fpsCnt;
    ImguiProxy imguiProxy;

    GLFWwindow* windowHandle;
    CamerasManager& camManagerHandle;

    float lastFrameTime = 0.0f;
    double lastX = 0.0, lastY = 0.0;

    bool firstMouse = false;

    void processInput();
};