#include "window.hpp"

void Window::frambufferResizeCallback(GLFWwindow* window, int width, int height) {
    Window* currentWindow = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    currentWindow->framebufferResized = true;
}

Window::Window(unsigned int width, unsigned int height)
    : screenWidth(width), screenHeight(height) {
    if (!glfwInit()){       // Initiate the library
        throw std::runtime_error("GLFW init failed!");
    }

    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);       // Do not create an OpenGL context
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);       // Disable resize for now

    window = glfwCreateWindow(screenWidth, screenHeight, "Exterior mapping 2", nullptr, nullptr);
    if (window == nullptr){
        throw std::runtime_error("GLFW windows wasn't created!");
    }
    
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, frambufferResizeCallback);
}

Window::~Window() {
    glfwDestroyWindow(window);
    glfwTerminate();
}