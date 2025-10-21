cmake_minimum_required(VERSION 3.15)
project(ExternalMapping2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g")

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
    src/main.cpp
    src/application.cpp
    src/vulkanContext.cpp
    src/swapchain.cpp
    src/window.cpp)

add_executable(ExternalMapping2 ${SOURCES})

find_package(Vulkan REQUIRED)

target_link_libraries(ExternalMapping2
    glfw
    vulkan
    dl
    pthread
)

add_custom_command(
    TARGET ExternalMapping2 PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling shaders..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/shaders ./compile.sh
)