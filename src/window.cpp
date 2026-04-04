#include <window.hpp>

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    Window* currentWindow = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    currentWindow->framebufferResized = true;
}

Window::Window(unsigned int width, unsigned int height)
    : screenWidth(width), screenHeight(height) {
    if (!glfwInit()){       // Initiate the library
        throw std::runtime_error("GLFW init failed!");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);       // Do not create an OpenGL context
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);       // Disable resize for now

    window = glfwCreateWindow(screenWidth, screenHeight, appName.c_str(), nullptr, nullptr);
    if (window == nullptr){
        throw std::runtime_error("GLFW windows wasn't created!");
    }
    
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

Window::~Window() {
    glfwDestroyWindow(window);
    glfwTerminate();
}