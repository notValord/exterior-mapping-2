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

inline constexpr float CAM_MAX_SPEED = 50.0f;
inline constexpr float CAM_MIN_SPEED = 0.5f;

const static glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

class Camera{
public:
    Camera(float extentRatio);
    virtual ~Camera();

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec2 getNearFar() const;
    float getFOV() const;

    void updateRatio(float newRatio);
    void updateYawPitch(float yaw, float pitch);

    void moveForward(float deltaTime);
    void moveBackward(float deltaTime);
    void moveLeft(float deltaTime);
    void moveRight(float deltaTime);
    void moveUp(float deltaTime);
    void moveDown(float deltaTime);

    void speedUp(float deltaTime);
    void speedDown(float deltaTime);

    glm::vec3& getPositionRef();
    float& getYawRef();
    float& getPitchRef();
    float& getSpeedRef();
    float& getSpeedStepRef();
    glm::vec3 getPosition() const;

    void recalculateVectors();

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

class NovelCamera : public Camera {
public:
    std::vector<VkImage> novelImage;

    NovelCamera(float extentRatio, VkDevice device, MemoryManager& memMan);
    ~NovelCamera();

    void createNovelImage(VkExtent2D novelExtent, const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM);
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

class OfflineCamera : public Camera {
public:
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    // VkImage colorImage = VK_NULL_HANDLE;
    // VkImage depthImage = VK_NULL_HANDLE;

    OfflineCamera(float extentRatio, VkDevice device, MemoryManager& memMan, VkExtent2D& swapChainExtent, const VkFormat colorFormat, const VkFormat depthFormat, VkRenderPass renderpass);
    ~OfflineCamera();

    void recreateOfflineResources(VkImage colorImage, VkImage depthImage, uint32_t camID, VkExtent2D& swapChainExtent, VkRenderPass renderpass, const VkFormat colorFormat, const VkFormat depthFormat);

    CamArrayData getCamData();
    VkImageView getImageView(ImageViewType type);
    
private:
    // VmaAllocation colorImageMemory;
    // VmaAllocation depthImageMemory;

    VkImageView colorImageView = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    // Vulkan Handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void createOfflineResources(VkImage colorImage, VkImage depthImage, uint32_t camID, const VkRenderPass renderPass, const VkFormat colorFormat, const VkFormat depthFormat, VkExtent2D& swapChainExtent);
    void cleanupOfflineResources();
};