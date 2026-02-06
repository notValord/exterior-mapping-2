cmake_minimum_required(VERSION 3.15)
project(ExternalMapping2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g")

# include imgui
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/include/imgui)

add_library(imgui
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/headers)

# set(SHADER_SRC
#     shaders/camCube.vert
#     shaders/camCube.frag
#     shaders/frustumShader.vert
#     shaders/frustumShader.frag
#     shaders/lineShader.vert
#     shaders/lineShader.frag
#     shaders/offlineShader.vert
#     shaders/offlineShader.frag
#     shaders/rayIntersect.comp
#     shaders/shader.vert
#     shaders/shader.frag
# )

set(SHADER_STUMP ".compiled")
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
file(GLOB SHADER_SRC "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp")

set(SOURCES
    src/main.cpp
    src/application.cpp
    src/vulkanContext.cpp
    src/swapchain.cpp
    src/window.cpp
    src/commandManager.cpp
    src/descriptorManager.cpp
    src/mesh.cpp
    src/pipeline.cpp
    src/syncManager.cpp
    src/textures.cpp
    src/uniforms.cpp
    src/util.cpp
    src/vertex.cpp
    src/memManager.cpp
    src/camera.cpp
    src/imguiProxy.cpp
    src/inputManager.cpp
    src/camManager.cpp
    src/debug.cpp
)

add_executable(ExternalMapping2 ${SOURCES})

find_package(Vulkan REQUIRED)

target_link_libraries(ExternalMapping2
    glfw
    vulkan
    dl
    pthread
    imgui
)

add_custom_command(
    OUTPUT "${SHADER_DIR}/${SHADER_STUMP}"
    COMMAND  bash ./compile.sh
    COMMAND ${CMAKE_COMMAND} -E touch ${SHADER_STUMP}
    WORKING_DIRECTORY "${SHADER_DIR}"
    DEPENDS ${SHADER_SRC}
    COMMENT "Compiling shaders..."
)

add_custom_target(Shaders DEPENDS ${SHADER_DIR}/${SHADER_STUMP})
add_dependencies(ExternalMapping2 Shaders)