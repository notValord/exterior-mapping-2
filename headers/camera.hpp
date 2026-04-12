#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE     // GLM uses openGl depth range -1 to 1
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// system include
#include <iostream>
#include <array>
#include <vector>

#include <structs.hpp>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

class MemoryManager;
struct CamArrayData;
struct AttachementsFormats;
struct CamJson;

inline constexpr float CAM_MAX_SPEED = 50.0f;
inline constexpr float CAM_MIN_SPEED = 0.5f;

const static glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

/**
 * @class Camera
 * @brief Base camera class providing basic camera functionality for 3D rendering.
 *
 * It supports first-person camera controls with yaw/pitch rotation and movement in all directions.
 */
class Camera{
public:
    /**
     * @brief Constructs a Camera with the given aspect ratio.
     * @param extentRatio The initial aspect ratio (width/height) for the projection matrix.
     */
    Camera(float extentRatio);
    virtual ~Camera();

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec2 getNearFar() const;
    float getFOV() const;

    /**
     * @brief Updates the aspect ratio for the projection matrix.
     * @param newRatio The new aspect ratio.
     */
    void updateRatio(float newRatio);

    /**
     * @brief Updates the yaw and pitch angles with delta and recalculates camera vectors.
     * @param yaw The change in yaw angle (in degrees).
     * @param pitch The change in pitch angle (in degrees).
     */
    void updateYawPitch(float yaw, float pitch);

    void setYawPitch(float newYaw, float newPitch);
    void setPosition(const glm::vec3& newPos);

    void moveForward(float deltaTime);
    void moveBackward(float deltaTime);
    void moveLeft(float deltaTime);
    void moveRight(float deltaTime);
    void moveUp(float deltaTime);
    void moveDown(float deltaTime);

    void speedUp(float deltaTime);
    void speedDown(float deltaTime);

    /**
     * @brief Gets a reference to the camera position vector.
     * @return Reference to the position vec3.
     */
    glm::vec3& getPositionRef();
    float& getYawRef();
    float& getPitchRef();
    float& getSpeedRef();
    float& getSpeedStepRef();
    glm::vec3 getPosition() const;

    /**
     * @brief Recalculates the front, right, and up vectors based on yaw and pitch.
     */
    void recalculateVectors();

    /**
     * @brief Creates JSON setup for the camera.
     * @return Camera setup in JSON format.
     */
    CamJson jsonCam() const;

private:
    glm::vec3 pos = glm::vec3(0.0f, 0.0f, 2.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);

    float speed = 2.5f;
    float speedStep = 1.0f;

    float fov = glm::radians(45.0f);
    float aspectRatio;
    float nearPlane = 1.0f;
    float farPlane = 100.0f;

    float yaw = 90.0f;
    float pitch = 0.0f;

};

/**
 * @brief Specialized camera class for novel view synthesis.
 *
 * Extends the base Camera class to handle resources for novel view generation.
 */
class NovelCamera : public Camera {
public:
    std::vector<VkImage> novelImage;

    /**
     * @brief Constructs a NovelCamera with device and memory manager references.
     * @param extentRatio Initial aspect ratio.
     * @param device Vulkan device handle.
     * @param memMan Reference to the memory manager.
     */
    NovelCamera(float extentRatio, VkDevice device, MemoryManager& memMan);
    ~NovelCamera();

    /**
     * @brief Creates novel view images with specified extent and format.
     * @param novelExtent The extent (width, height) of the images.
     * @param colorFormat The Vulkan format for the images (default RGBA8_UNORM).
     */
    void createNovelImage(VkExtent2D novelExtent, const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM);

    /**
     * @brief Transitions the image layout for rendering or presentation.
     * @param currentFrame The current frame index.
     * @param targetLayout The target image layout.
     * @param commandBuffer The command buffer for the transition.
     * @param colorFormat The image format.
     */
    void swapTransferLayoutRenderPresent(uint32_t currentFrame, VkImageLayout targetLayout, VkCommandBuffer commandBuffer, const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM);
    VkImageView getImageView(uint32_t currentIndex);

private:
    std::vector<VmaAllocation> novelImageMemory;
    std::vector<VkImageView> novelImageView;
    std::vector<VkImageLayout> novelImageLayout;

    // Vulkan Handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void cleanupResources();
};

/**
 * @brief Specialized camera class for offline rendering and frustum culling.
 *
 * Extends the base Camera class to manage Vulkan framebuffers and image views for offline rendering.
 */
class OfflineCamera : public Camera {
public:
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    /**
     * @brief Constructs an OfflineCamera with Vulkan device and rendering parameters.
     * @param extentRatio Initial aspect ratio.
     * @param device Vulkan device handle.
     * @param memMan Reference to the memory manager.
     * @param swapChainExtent The extent of the swapchain/render target.
     * @param colorFormat Vulkan format for color attachments.
     * @param depthFormat Vulkan format for depth attachments.
     * @param renderpass The Vulkan render pass for framebuffer creation.
     */
    OfflineCamera(float extentRatio, VkDevice device, MemoryManager& memMan, VkExtent2D& swapChainExtent, const VkFormat colorFormat, const VkFormat depthFormat, VkRenderPass renderpass);
    ~OfflineCamera();

    /**
     * @brief Recreates offline resources, typically after swapchain resize.
     * @param colorImage The new color attachment image.
     * @param depthImage The new depth attachment image.
     * @param camID Camera identifier.
     * @param swapChainExtent New extent.
     * @param renderpass The render pass.
     * @param colorFormat Color format.
     * @param depthFormat Depth format.
     */
    void recreateOfflineResources(VkImage colorImage, VkImage depthImage, uint32_t camID, VkExtent2D& swapChainExtent, VkRenderPass renderpass, const VkFormat colorFormat, const VkFormat depthFormat);

    /**
     * @brief Computes and returns camera data including view/projection matrices and frustum planes.
     * @return A CamArrayData struct with matrices and frustum information.
     */
    CamArrayData getCamData();

    /**
     * @brief Gets the image view for the specified attachment type.
     * @param type The type of image view (COLOR or DEPTH).
     * @return The Vulkan image view.
     */
    VkImageView getImageView(ImageViewType type);
    
private:
    VkImageView colorImageView = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    // Vulkan Handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    /**
     * @brief Creates offline rendering resources including framebuffers and image views.
     * @param colorImage The color attachment image.
     * @param depthImage The depth attachment image.
     * @param camID Identifier for the camera (used in image view creation).
     * @param renderPass The Vulkan render pass.
     * @param colorFormat Format of the color image.
     * @param depthFormat Format of the depth image.
     * @param swapChainExtent Extent of the images.
     */
    void createOfflineResources(VkImage colorImage, VkImage depthImage, uint32_t camID, const VkRenderPass renderPass, const VkFormat colorFormat, const VkFormat depthFormat, VkExtent2D& swapChainExtent);
    void cleanupOfflineResources();
};