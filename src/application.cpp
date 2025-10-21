#include "application.hpp"

static unsigned int SCREEN_WIDTH = 800;
static unsigned int SCREEN_HEIGHT = 600;

void App::run() {;
    initVulkan();
    mainLoop();
    cleanup();
}

App::App() 
    : appWindow(SCREEN_WIDTH, SCREEN_HEIGHT), 
       vulkanContext(appWindow.window),
       swapchain(vulkanContext.getDeviceSurfaceHandle(), vulkanContext.familyIndices, vulkanContext.device, appWindow.window) {
    // order of members in a class
}

App::~App() {
    // reverse order of constructors
}

void App::initVulkan() {

}

void App::mainLoop() {
    while (!glfwWindowShouldClose(appWindow.window)) {
        glfwPollEvents();
    }
}

void App::cleanup() {

}