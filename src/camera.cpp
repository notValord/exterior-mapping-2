#include "camera.hpp"

#include "memManager.hpp"
#include "util.hpp"
#include "swapchain.hpp"

Camera::Camera(float extentRatio)
    : aspectRatio(extentRatio) {

}

Camera::~Camera() {

}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(pos, pos + front, up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    glm::mat4 projection =  glm::perspective(fov, aspectRatio, nearPlane, farPlane);
    projection[1][1] *= -1;       // compensation for the Y coordinate being inverted

    return projection;
}

void Camera::updateRatio(float newRatio){
    aspectRatio = newRatio;
}

void Camera::updateYawPitch(float yaw_delta, float pitch_delta) {
    yaw   += yaw_delta;
    pitch += pitch_delta;

    // Clamp pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 new_front;
    new_front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    new_front.y = -sin(glm::radians(pitch));
    new_front.z = -sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    front = glm::normalize(new_front);

    // Recalculate right and up to keep an orthogonal basis
    glm::vec3 right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
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

// change to exponential
void Camera::speedUp(float deltaTime) {
    speed = glm::min(speed + speedStep * deltaTime, CAM_MAX_SPEED);
    std::cout << "Curretn speed: " << speed << " " << deltaTime << std::endl;
}

void Camera::speedDown(float deltaTime) {
    speed = glm::max(speed - speedStep * deltaTime, CAM_MIN_SPEED);
    std::cout << "Curretn speed: " << speed << std::endl;
}

OfflineCamera::OfflineCamera(float extentRatio, VkDevice device, MemoryManager& memMan, VkExtent2D& swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass)
    : Camera(extentRatio), deviceHandle(device), memManager(memMan), swapChainExtentHandle(swapChainExtent), attachementsFormats(formats) {
    createOfflineResources(renderpass);
}

OfflineCamera::~OfflineCamera() {
    vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);

    vkDestroyImageView(deviceHandle, colorImageView, nullptr);
    vkDestroyImageView(deviceHandle, depthImageView, nullptr);

    memManager.destroyImage(colorImage, colorImageMemory);
    memManager.destroyImage(depthImage, depthImageMemory);
}

void OfflineCamera::createOfflineResources(const VkRenderPass renderPass) {
    memManager.createImage(swapChainExtentHandle.width, swapChainExtentHandle.height, attachementsFormats.colorImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, colorImage,
        VMA_MEMORY_USAGE_GPU_ONLY, colorImageMemory);
    memManager.createImage(swapChainExtentHandle.width, swapChainExtentHandle.height, attachementsFormats.depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, depthImage,
        VMA_MEMORY_USAGE_GPU_ONLY, depthImageMemory);

    // transition image layouts from Undefined

    colorImageView = createImageView(colorImage, attachementsFormats.colorImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, deviceHandle);
    depthImageView = createImageView(depthImage, attachementsFormats.depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, deviceHandle);

    std::array<VkImageView, 2> attachements = {
        colorImageView,
        depthImageView
    };

    VkFramebufferCreateInfo framebufferCI{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = static_cast<uint32_t>(attachements.size()),
        .pAttachments = attachements.data(),
        .width = swapChainExtentHandle.width,
        .height = swapChainExtentHandle.height,
        .layers = 1
    };

    if (vkCreateFramebuffer(deviceHandle, &framebufferCI, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create frambutters!");
    }
}