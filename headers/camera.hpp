#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE     // GLM uses openGl depth range -1 to 1
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <array>

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

class MemoryManager;
struct CamArrayData;

static float CAM_MAX_SPEED = 50.0f;
static float CAM_MIN_SPEED = 0.5f;

const static glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

class Camera{
public:
    Camera(float extentRatio);
    virtual ~Camera();

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec2 getNearFar() const;

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
private:
    glm::vec3 pos = glm::vec3(0.0f, 0.0f, 2.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);

    float speed = 2.5f;
    float speedStep = 1.0f;

    float fov = glm::radians(45.0f);
    float aspectRatio;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    float yaw = 90.0f;
    float pitch = 0.0f;
};

struct AttachementsFormats;

class OfflineCamera : public Camera {
public:
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    VkImage colorImage;
    VkImage depthImage;

    OfflineCamera(float extentRatio, VkDevice device, MemoryManager& memMan, VkExtent2D& swapChainExtent, const VkFormat colorFormat, const VkFormat depthFormat, VkRenderPass renderpass);
    ~OfflineCamera();

    void recreateOfflineResources(VkExtent2D& swapChainExtent, VkRenderPass renderpass, const VkFormat colorFormat, const VkFormat depthFormat);

    CamArrayData getCamData();
private:
    VmaAllocation colorImageMemory;
    VmaAllocation depthImageMemory;

    VkImageView colorImageView;
    VkImageView depthImageView;

    // Vulkan Handles
    VkDevice deviceHandle;
    MemoryManager& memManager;

    void createOfflineResources(const VkRenderPass renderPass, const VkFormat colorFormat, const VkFormat depthFormat, VkExtent2D& swapChainExtent);
    void cleanupOfflineResources();
};