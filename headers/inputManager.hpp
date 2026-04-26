#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// system include
#include <chrono>

// user include
#include <imguiProxy.hpp>
#include <structs.hpp>

class CamerasManager;
class Window;

/// Simple frame rate counter for UI display and timing feedback.
class FPSCounter {
public:
    /// Current frames per second value.
    double fps = 0.0;

    /// Update the FPS counter once per rendered frame.
    void frame();

private:
    std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();
    uint32_t frameCount = 0;
};

/// Handles keyboard, mouse, and ImGui input plus UI command recording.
class InputManager {
public:
    /// Whether an offline render should be presented instead of the live swapchain.
    bool presentOfflineFlag = false;

    /// Whether the offline image is being prepared.
    bool setupOfflineImage = false;

    /// Which image view should be displayed in the UI / renderer.
    ImageViewType presentType;

    /// Whether the offline render pass should run.
    bool renderOfflineFlag = false;

    /// Visualization toggles handled by the UI.
    bool debugGrayscale = false;
    bool debugFrustum = false;
    bool debugCamCube = false;
    bool debugIntersection = false;
    bool debugPointCloud = false;

    /// Novel view rendering options.
    bool novelRender = false;
    bool newNovelRender = false;
    DebugCompute novelDebug;
    NovelHeuristic novelHeuristic;
    bool startSynthesis = false;

    uint32_t bestNCount = 1;

    /// Ray marching / intersection options.
    DistanceType distance;
    uint32_t neighbourCount = 1;
    ConeMarching coneMarching;
    float inConePercentage = 0.0f;

    /// Scene selection state.
    int sceneSelected = 0;
    bool sceneChanged = false;

    int screenRes[2];
    bool changeRes = false;

    bool saveCamSnapshots = false;
    std::string snapshotCamFile = "Snapshot";

    bool saveSetupFlag = false;
    std::string setupName = "Config";

    bool loadSetupFlag = false;
    std::string setupNameLoad;

    bool timeRender = false;
    std::vector<bool> timingStarted;
    bool saveGT = false;
    bool compareToGT = false;

    bool testStep = false;

    InputManager(Window& window,
                 CamerasManager& camManager,
                 const AttachementsFormats& imageFormats,
                 const std::vector<VkImageView>& swapChainImageViews,
                 const PhysicalDeviceInstance& physicalDeviceInstance,
                 const VkQueue graphicsQueue,
                 const QueueFamilyIndices& familyIndices,
                 VkExtent2D& swapChainExtent,
                 MemoryManager& memMan,
                 Mesh& mesh);
    ~InputManager();

    /// Install GLFW input callbacks for this manager.
    void setCallbacks();

    /// Poll input state, update camera movement, and rebuild UI when needed.
    void frame();

    /// Record the ImGui command buffer for the current swapchain image.
    VkCommandBuffer recordUI(uint32_t currentFrame, uint32_t imageIndex);

    /// Recreate ImGui framebuffers after swapchain recreation.
    void imguiResize(const std::vector<VkImageView>& swapChainImageViews,
                     const VkExtent2D& swapChainExtent);

    void turnUIoff();

private:
    FPSCounter fpsCnt;
    ImguiProxy imguiProxy;

    // Vulkan handless
    Window& windowHandle;
    CamerasManager& camManagerHandle;
    Mesh& meshRef;

    float lastFrameTime = 0.0f;
    double lastX = 0.0;
    double lastY = 0.0;
    bool firstMouse = false;

    /// Process raw GLFW input state every frame.
    void processInput();
};