#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <string>
#include <vector>
#include <filesystem>

extern const size_t MAX_FRAMES_IN_FLIGHT;
extern const std::string JSON_DIR;

struct AttachementsFormats;
struct PhysicalDeviceInstance;
struct QueueFamilyIndices;
class CamerasManager;
class InputManager;
class MemoryManager;
class Mesh;

/**
 * @class ImguiProxy
 * @brief Proxy for ImGui Vulkan integration and UI rendering.
 */
class ImguiProxy {
public:
    /**
     * @brief Create ImGui support for a Vulkan swapchain.
     * @param imageFormats Formats used by the host swapchain attachments.
     * @param swapChainImageViews The current swapchain image views.
     * @param physicalDeviceInstance Vulkan instance/device handles.
     * @param window GLFW window used by ImGui.
     * @param graphicsQueue Vulkan queue used to submit ImGui commands.
     * @param familyIndices Queue family indices for command buffer allocation.
     * @param swapChainExtent Swapchain extent reference for framebuffer sizing.
     * @param memMan Memory manager used by the host application.
     */
    ImguiProxy(const AttachementsFormats& imageFormats,
               const std::vector<VkImageView>& swapChainImageViews,
               const PhysicalDeviceInstance& physicalDeviceInstance,
               GLFWwindow* window,
               const VkQueue graphicsQueue,
               const QueueFamilyIndices& familyIndices,
               VkExtent2D& swapChainExtent,
               MemoryManager& memMan);

    /**
     * @brief Shutdown ImGui and destroy Vulkan resources.
     */
    ~ImguiProxy();

    /**
     * @brief Begin a new ImGui frame and build the UI hierarchy.
     * @param fps Current frames per second for display.
     * @param camManager Reference to the camera manager for UI controls.
     * @param inputManager Pointer to input manager for UI state.
     * @param mesh Reference to mesh for scene controls.
     * @param scrWidth Current screen width.
     * @param scrHeight Current screen height.
     */
    void rebuildUI(float fps, CamerasManager& camManager, InputManager* inputManager, Mesh& mesh, const int scrWidth, const int scrHeight);

    /**
     * @brief Recreate framebuffers after swapchain resize or recreation.
     * @param swapChainImageViews Updated swapchain image views.
     * @param swapChainExtent New swapchain extent.
     */
    void recreateFramebuffers(const std::vector<VkImageView>& swapChainImageViews,
                              const VkExtent2D& swapChainExtent);

    /**
     * @brief Record the ImGui command buffer for the current frame/image.
     * @param currentFrame Current frame index for command buffer selection.
     * @param imageIndex Swapchain image index for framebuffer selection.
     * @return The recorded command buffer.
     */
    VkCommandBuffer recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex);

private:
    VkDescriptorPool descriptorPool;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // UI variables
    std::string offlineSnapshotsFiles = "Image";
    std::vector<std::string> setupNames;

    // Vulkan handles
    VkDevice deviceHandle;
    VkExtent2D& swapChainExtentHandle;
    MemoryManager& memManager;

    void createDescriptorPool();
    void createRenderPass(const AttachementsFormats& imageFormats);
    void createFramebuffers(const std::vector<VkImageView>& swapChainImageViews);
    void createCommandBuffers(const QueueFamilyIndices& familyIndices);

    void drawUI(float fps, CamerasManager& camManager, InputManager* inputManager, Mesh& mesh, const int scrWidth, const int scrHeight);

    void uiCamera(CamerasManager& camManager, InputManager* inputManager);
    void uiNovelCam(CamerasManager& camManager, InputManager* inputManager);
    void uiSetup(InputManager* inputManager);
    void uiGeneral(InputManager* inputManager, Mesh& mesh, const int scrWidth, const int scrHeight);
    void uiCamArray(CamerasManager& camManager, InputManager* inputManager);
    void uiOfflineRender(CamerasManager& camManager, InputManager* inputManager);
    void uiNovelRender(CamerasManager& camManager, InputManager* inputManager);
    void uiDebugInfo(float fps, InputManager* inputManager, bool offlineRendred);
};