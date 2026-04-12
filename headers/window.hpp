#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

/**
 * @class Window
 * @brief GLFW window wrapper for Vulkan rendering.
 * 
 * Manages the creation and lifecycle of a GLFW window configured for Vulkan rendering.
 * Handles window resize callbacks and maintains the window state.
 */
class Window {
public:
    GLFWwindow* window;
    bool framebufferResized = false;


    /**
     * @brief Creates a Vulkan-compatible window.
     * Initializes GLFW, creates a window, and sets up resize callbacks.
     * @param width The width of the window in pixels.
     * @param height The height of the window in pixels.
     * @throws std::runtime_error if GLFW initialization or window creation fails.
     */
    Window(unsigned int width, unsigned int height);
    ~Window();

    uint32_t getWidth() const;
    uint32_t getHeight() const;

private:
    uint32_t screenWidth;
    uint32_t screenHeight;

    std::string appName = "Exterior mapping 2";

    /**
     * @brief Static callback function for framebuffer resize events.
     * Sets the framebufferResized flag for the associated Window instance.
     * @param window The GLFW window that was resized.
     * @param width The new width of the framebuffer in pixels.
     * @param height The new height of the framebuffer in pixels.
     */
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};