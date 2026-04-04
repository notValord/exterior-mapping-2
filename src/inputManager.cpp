#include <inputManager.hpp>

#include <camManager.hpp>

static bool showUIFlag = true;
static bool nextCamTrigger = false;
static bool novelViewTrigger = false;
static bool observerTrigger = false;

void FPSCounter::frame() {
    frameCount++;

    auto now = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::chrono::seconds::period>(now - lastTime).count();

    // Update every second
    if (elapsed >= 1.0) {
        fps = frameCount / elapsed;
        frameCount = 0;
        lastTime = now;
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;  // Let ImGui handle it, ignore game hotkeys
    
    // if (action == GLFW_PRESS) {
    //     std::cout << "Key pressed: " << key << std::endl;
    // }
    // else if (action == GLFW_RELEASE) {
    //     std::cout << "Key released: " << key << std::endl;
    // }

    if (action == GLFW_PRESS && key == GLFW_KEY_U) {
        showUIFlag = showUIFlag ? false:true;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_N) {
        nextCamTrigger = true;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_M) {
        novelViewTrigger = true;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_B) {
        observerTrigger = true;
    }
}

InputManager::InputManager(GLFWwindow* window, CamerasManager& camManager, const AttachementsFormats& imageFormats, const std::vector<VkImageView>& swapChainImageViews,
     const PhysicalDeviceInstance& physicalDeviceInstance, const VkQueue graphicsQueue, const QueueFamilyIndices& familyIndices, VkExtent2D& swapChainExtent, MemoryManager& memMan,
     Mesh& mesh)
    : fpsCnt(),
      imguiProxy(imageFormats, swapChainImageViews, physicalDeviceInstance, window, graphicsQueue, familyIndices, swapChainExtent, memMan),
     windowHandle(window),
     camManagerHandle(camManager),
     meshRef(mesh){
    presentType = ImageViewType::COLOR;
    novelDebug = DebugCompute::NO_DEBUG;
    novelHeuristic = NovelHeuristic::COLOR_HEURISTIC;

    distance = DistanceType::POINT;
}

InputManager::~InputManager() {

}

void InputManager::setCallbacks() {
    glfwSetKeyCallback(windowHandle, key_callback);
}

void InputManager::processInput() {
    float currentFrameTime = glfwGetTime();
    float deltaTime = currentFrameTime - lastFrameTime;
    lastFrameTime = currentFrameTime;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;  // Let ImGui handle it, ignore game hotkeys

    // Camera movement
    if (glfwGetKey(windowHandle, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(windowHandle, GLFW_KEY_UP) == GLFW_PRESS)
        camManagerHandle.activeCam->moveForward(deltaTime);
    if (glfwGetKey(windowHandle, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(windowHandle, GLFW_KEY_DOWN) == GLFW_PRESS)
        camManagerHandle.activeCam->moveBackward(deltaTime);
    if (glfwGetKey(windowHandle, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(windowHandle, GLFW_KEY_LEFT) == GLFW_PRESS)
        camManagerHandle.activeCam->moveLeft(deltaTime);
    if (glfwGetKey(windowHandle, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(windowHandle, GLFW_KEY_RIGHT) == GLFW_PRESS)
        camManagerHandle.activeCam->moveRight(deltaTime);
    if (glfwGetKey(windowHandle, GLFW_KEY_SPACE) == GLFW_PRESS)
        camManagerHandle.activeCam->moveUp(deltaTime);
    if (glfwGetKey(windowHandle, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camManagerHandle.activeCam->moveDown(deltaTime);

    if (glfwGetKey(windowHandle, GLFW_KEY_E) == GLFW_PRESS)
        camManagerHandle.activeCam->speedUp(deltaTime);
    if (glfwGetKey(windowHandle, GLFW_KEY_Q) == GLFW_PRESS)
        camManagerHandle.activeCam->speedDown(deltaTime);


    if (glfwGetMouseButton(windowHandle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(windowHandle, &xpos, &ypos);

        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float sensitivity = 0.08f;
        float xoffset = static_cast<float>(xpos - lastX);
        float yoffset = static_cast<float>(lastY - ypos); // reversed Y axis
        lastX = xpos;
        lastY = ypos;

        xoffset *= sensitivity;
        yoffset *= sensitivity;

        camManagerHandle.activeCam->updateYawPitch(xoffset, yoffset);
    } 
    else {
        firstMouse = true; // reset when mouse releasedf
    }

    if (nextCamTrigger) {
        camManagerHandle.nextCam();
        nextCamTrigger = false;
    }
    if (novelViewTrigger) {
        camManagerHandle.toggleNovel();
        novelViewTrigger = false;
    }
    if (observerTrigger) {
        camManagerHandle.toggleObserver();
        observerTrigger = false;
    }
}

void InputManager::frame() {
    processInput();
    fpsCnt.frame();     // Get the approximate fps

    if (showUIFlag) {
        imguiProxy.rebuildUI(fpsCnt.fps, camManagerHandle, this, meshRef);
    }
}

VkCommandBuffer InputManager::recordUI(uint32_t currentFrame, uint32_t imageIndex) {
    if (!showUIFlag) {
        return VK_NULL_HANDLE;
    }
    
    return imguiProxy.recordCommandBuffer(currentFrame, imageIndex);
}

void InputManager::imguiResize(const std::vector<VkImageView>& swapChainImageViews, const VkExtent2D& swapChainExtent) {
    imguiProxy.recreateFramebuffers(swapChainImageViews, swapChainExtent);
}