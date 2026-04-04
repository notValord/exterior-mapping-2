#include <camera.hpp>

#include <memManager.hpp>
#include <util.hpp>
#include <swapchain.hpp>
#include <uniforms.hpp>

static float wrapYaw(float yaw) {
    yaw = fmod(yaw + 180.0f, 360.0f);
    if (yaw < 0) yaw += 360.0f;
    return yaw - 180.0f;
}

Camera::Camera(float extentRatio)
    : aspectRatio(extentRatio) {
    ;
}

Camera::~Camera() {
    ;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(pos, pos + front, up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    glm::mat4 projection =  glm::perspective(fov, aspectRatio, nearPlane, farPlane);
    projection[1][1] *= -1;       // compensation for the Y coordinate being inverted

    return projection;
}

glm::vec2 Camera::getNearFar() const {
    return glm::vec2(nearPlane, farPlane);
}

float Camera::getFOV() const {
    return fov;
}
 
void Camera::updateRatio(float newRatio){
    aspectRatio = newRatio;
}

void Camera::recalculateVectors() {
    glm::vec3 new_front;
    new_front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    new_front.y = -sin(glm::radians(pitch));
    new_front.z = -sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    front = glm::normalize(new_front);

    // Recalculate right and up to keep an orthogonal basis
    glm::vec3 right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

void Camera::updateYawPitch(float yaw_delta, float pitch_delta) {
    yaw    = wrapYaw(yaw+yaw_delta);
    pitch += pitch_delta;

    // Clamp pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    recalculateVectors();
}

void Camera::moveForward(float deltaTime) {
    pos += front * speed * deltaTime;
}

void Camera::moveBackward(float deltaTime) {
    pos -= front * speed * deltaTime;
}

void Camera::moveLeft(float deltaTime) {
    pos -=  glm::normalize(glm::cross(front, up)) * speed * deltaTime;
}

void Camera::moveRight(float deltaTime) {
    pos +=  glm::normalize(glm::cross(front, up)) * speed * deltaTime;
}

void Camera::moveUp(float deltaTime) {
    pos += up * speed * deltaTime;
}

void Camera::moveDown(float deltaTime) {
    pos -= up * speed * deltaTime;
}

// change to exponential?
void Camera::speedUp(float deltaTime) {
    speed = glm::min(speed + speedStep * deltaTime, CAM_MAX_SPEED);
}

void Camera::speedDown(float deltaTime) {
    speed = glm::max(speed - speedStep * deltaTime, CAM_MIN_SPEED);
}

glm::vec3& Camera::getPositionRef() {
    return pos;
}

float& Camera::getYawRef() {
    return yaw;
}

float& Camera::getPitchRef() {
    return pitch;
}

float& Camera::getSpeedRef() {
    return speed;
}

float& Camera::getSpeedStepRef() {
    return speedStep;
}

glm::vec3 Camera::getPosition() const {
    return pos;
}

NovelCamera::NovelCamera(float extentRatio, VkDevice device, MemoryManager& memMan)
    : Camera(extentRatio), deviceHandle(device), memManager(memMan) {
    novelImage.resize(MAX_FRAMES_IN_FLIGHT);        // should initialize to VK_NULL_HANDLE
    novelImageMemory.resize(MAX_FRAMES_IN_FLIGHT);
    novelImageView.resize(MAX_FRAMES_IN_FLIGHT);
    novelImageLayout.resize(MAX_FRAMES_IN_FLIGHT);

    createNovelImage({1, 1});     // setup a dummy
}

NovelCamera::~NovelCamera() {
    cleanupResources();
}

VkImageView NovelCamera::getImageView(uint32_t currentIndex) {
    return novelImageView[currentIndex];
}

void NovelCamera::cleanupResources() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (novelImage[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(deviceHandle, novelImageView[i], nullptr);
            memManager.destroyImage(novelImage[i], novelImageMemory[i]);
        }
    }
}

void NovelCamera::createNovelImage(VkExtent2D novelExtent, const VkFormat colorFormat) {
    cleanupResources();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        memManager.createImage(novelExtent.width, novelExtent.height, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
        novelImage[i], VMA_MEMORY_USAGE_GPU_ONLY, novelImageMemory[i]);
        memManager.transitionImageLayout(novelImage[i], colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        novelImageLayout[i] = VK_IMAGE_LAYOUT_GENERAL;

        novelImageView[i] = createImageView(novelImage[i], colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);
    }
}

void NovelCamera::swapTransferLayoutRenderPresent(uint32_t currentFrame, VkImageLayout targetLayout, VkCommandBuffer commandBuffer, const VkFormat colorFormat) {
    if (targetLayout == novelImageLayout[currentFrame]) {
        return;
    }

    if (targetLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && novelImageLayout[currentFrame] == VK_IMAGE_LAYOUT_GENERAL) {
        memManager.transitionImageLayout(novelImage[currentFrame], colorFormat, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, commandBuffer);
        novelImageLayout[currentFrame] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    else if (targetLayout == VK_IMAGE_LAYOUT_GENERAL && novelImageLayout[currentFrame] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        memManager.transitionImageLayout(novelImage[currentFrame], colorFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1 , commandBuffer);
        novelImageLayout[currentFrame] = VK_IMAGE_LAYOUT_GENERAL;
    }
    else {
        throw std::runtime_error("Unknown image layout!");
    }
}


OfflineCamera::OfflineCamera(float extentRatio, VkDevice device, MemoryManager& memMan, VkExtent2D& swapChainExtent, const VkFormat colorFormat, const VkFormat depthFormat, VkRenderPass renderpass)
    : Camera(extentRatio), deviceHandle(device), memManager(memMan) {
    // createOfflineResources(renderpass, colorFormat, depthFormat, swapChainExtent);
}

OfflineCamera::~OfflineCamera() {
    cleanupOfflineResources();
}

void OfflineCamera::createOfflineResources(VkImage colorImage, VkImage depthImage, uint32_t camID, const VkRenderPass renderPass, const VkFormat colorFormat, const VkFormat depthFormat, VkExtent2D& swapChainExtent) {
    colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle, 1, camID);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, deviceHandle, 1, camID);

    std::array<VkImageView, 2> attachements = {
        colorImageView,
        depthImageView
    };

    VkFramebufferCreateInfo framebufferCI{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = static_cast<uint32_t>(attachements.size()),
        .pAttachments = attachements.data(),
        .width = swapChainExtent.width,
        .height = swapChainExtent.height,
        .layers = 1
    };

    if (vkCreateFramebuffer(deviceHandle, &framebufferCI, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create frambutters!");
    }
}

void OfflineCamera::cleanupOfflineResources() {
    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }

    if (colorImageView != VK_NULL_HANDLE && depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(deviceHandle, colorImageView, nullptr);
        vkDestroyImageView(deviceHandle, depthImageView, nullptr);

        colorImageView = VK_NULL_HANDLE;
        depthImageView = VK_NULL_HANDLE;
    }
}

void OfflineCamera::recreateOfflineResources(VkImage colorImage, VkImage depthImage, uint32_t camID, VkExtent2D& swapChainExtent, VkRenderPass renderpass, const VkFormat colorFormat, const VkFormat depthFormat) {
    cleanupOfflineResources();

    createOfflineResources(colorImage, depthImage, camID, renderpass, colorFormat, depthFormat, swapChainExtent);
}

CamArrayData OfflineCamera::getCamData() {
    CamArrayData camData{};

    glm::mat4 viewProjMat = glm::transpose(getProjectionMatrix() * getViewMatrix());

    // Define the frustum planes
    // Left
    camData.frustumPlanes[0] = viewProjMat[3] + viewProjMat[0];
    // Right
    camData.frustumPlanes[1] = viewProjMat[3] - viewProjMat[0];
    // Bottom
    camData.frustumPlanes[2] = viewProjMat[3] + viewProjMat[1];
    // Top
    camData.frustumPlanes[3] = viewProjMat[3] - viewProjMat[1];
    // Near
    camData.frustumPlanes[4] = viewProjMat[2];
    // Far
    camData.frustumPlanes[5] = viewProjMat[3] - viewProjMat[2];

    // Testing cube
    // // Left
    // camData.frustumPlanes[0] = glm::vec4(1.0f, 0.0f, 0.0f, 3.0f);
    // // Right
    // camData.frustumPlanes[1] = glm::vec4(-1.0f, 0.0f, 0.0f, 3.0f);
    // // Bottom
    // camData.frustumPlanes[2] = glm::vec4(0.0f, 1.0f, 0.0f, 3.0f);
    // // Top
    // camData.frustumPlanes[3] = glm::vec4(0.0f, -1.0f, 0.0f, 3.0f);
    // // Near
    // camData.frustumPlanes[4] = glm::vec4(0.0f, 0.0f, -1.0f, 3.0f);
    // // Far
    // camData.frustumPlanes[5] = glm::vec4(0.0f, 0.0f, 1.0f, 3.0f);

    // Normalize planes
    for (uint32_t p = 0; p < 6; ++p) {
        glm::vec3 n = glm::vec3(camData.frustumPlanes[p]);
        float len = glm::length(n);
        camData.frustumPlanes[p] /= len;
    }

    camData.viewMat = getViewMatrix();
    camData.projMat = getProjectionMatrix();
    camData.invViewMat = glm::inverse(camData.viewMat);
    camData.invProjMat = glm::inverse(camData.projMat);
    return camData;
}

VkImageView OfflineCamera::getImageView(ImageViewType type) {
    if (type == ImageViewType::COLOR) {
        return colorImageView;
    }
    else if (type == ImageViewType::DEPTH) {
        return depthImageView;
    }
    else {
        throw std::runtime_error("Incorrect ImageViewType");
    }
}