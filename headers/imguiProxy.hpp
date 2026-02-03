#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

// system include
#include <iostream>
#include <vector>

extern const size_t MAX_FRAMES_IN_FLIGHT;

struct AttachementsFormats;
struct PhysicalDeviceInstance;
struct QueueFamilyIndices;
class CamerasManager;
class InputManager;
class MemoryManager;

class ImguiProxy {
public:
    ImguiProxy(const AttachementsFormats& imageFormats, const std::vector<VkImageView>& swapChainImageViews,
     const PhysicalDeviceInstance& physicalDeviceInstance, GLFWwindow* window, const VkQueue graphicsQueue, const QueueFamilyIndices& familyIndices,
     VkExtent2D& swapChainExtent, MemoryManager& memMan);
    ~ImguiProxy();

    void rebuildUI(float fps, CamerasManager& camManager, InputManager* inputManager);
    void recreateFramebuffers(const std::vector<VkImageView>& swapChainImageViews, const VkExtent2D& swapChainExtent);
    VkCommandBuffer recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex);
    
private:
    VkDescriptorPool descriptorPool;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // UI variables
    std::string snapshotsFiles = "Image";

    // Vulkan handles
    VkDevice deviceHandle;
    VkExtent2D& swapChainExtentHandle;
    MemoryManager& memManager;

    void createDescriptorPool();
    void createRenderPass(const AttachementsFormats& imageFormats);
    void createFramebuffers(const std::vector<VkImageView>& swapChainImageViews);
    void createCommandBuffers(const QueueFamilyIndices& familyIndices);

    void drawUI(float fps, CamerasManager& camManager, InputManager* inputManager);

    void uiActiveCam(CamerasManager& camManager);
    void uiNovelCam(CamerasManager& camManager, InputManager* inputManager);
    void uiObserver(CamerasManager& camManager);
    void uiCamArray(CamerasManager& camManager, InputManager* inputManager);
    void uiOfflineRender(CamerasManager& camManager, InputManager* inputManager);
    void uiNovelRender(CamerasManager& camManager, InputManager* inputManager);
    void uiDebugInfo(float fps, InputManager* inputManager);
};