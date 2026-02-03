#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// system include
#include <iostream>
#include <chrono>

// user include
#include <imguiProxy.hpp>

class CamerasManager;
enum class ImageViewType;

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
    // UI public flags and variables
    bool presentOfflineFlag = false;
    bool changeOfflineImage = false;
    ImageViewType presentType;
    bool renderOfflineFlag = false;

    bool debugGrayscale = false;
    bool debugFrustum = false;
    bool debugCamCube = false;
    bool debugIntersection = false;

    bool novelRender = false;
    bool startSynthesis = false;

    InputManager(GLFWwindow* window, CamerasManager& camManager, const AttachementsFormats& imageFormats, const std::vector<VkImageView>& swapChainImageViews,
     const PhysicalDeviceInstance& physicalDeviceInstance, const VkQueue graphicsQueue, const QueueFamilyIndices& familyIndices, VkExtent2D& swapChainExtent,
     MemoryManager& memMan);
    ~InputManager();

    void setCallbacks();

    void frame();
    VkCommandBuffer recordUI(uint32_t currentFrame, uint32_t imageIndex);

    void imguiResize(const std::vector<VkImageView>& swapChainImageViews, const VkExtent2D& swapChainExtent);

private:
    FPSCounter fpsCnt;
    ImguiProxy imguiProxy;

    // vulkan handless
    GLFWwindow* windowHandle;
    CamerasManager& camManagerHandle;

    float lastFrameTime = 0.0f;
    double lastX = 0.0, lastY = 0.0;

    bool firstMouse = false;

    void processInput();
};