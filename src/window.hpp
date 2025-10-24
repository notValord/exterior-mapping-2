#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

// remake the screen widht and height as constructor argumetns

class Window {
public:
    GLFWwindow* window;
    bool framebufferResized = false;

    Window(unsigned int width, unsigned int height);
    ~Window();

private:
    uint32_t screenWidth;
    uint32_t screenHeight;

    static void frambufferResizeCallback(GLFWwindow* window, int width, int height);
};