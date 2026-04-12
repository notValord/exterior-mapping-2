cmake_minimum_required(VERSION 3.15)
project(ExteriorMapping2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE "Debug")       # turn off for release

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

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/headers)

set(SHADER_STUMP ".compiled")
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
file(GLOB SHADER_SRC "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp")
file(GLOB SOURCES "${SRC_DIR}/*.cpp")

add_executable(ExteriorMapping2 ${SOURCES})

find_package(Vulkan REQUIRED)

target_link_libraries(ExteriorMapping2
    glfw
    vulkan
    dl
    pthread
    imgui
)


# Custom command to compile shaders
add_custom_command(
    OUTPUT "${SHADER_DIR}/${SHADER_STUMP}"
    COMMAND  bash ./compile.sh
    COMMAND ${CMAKE_COMMAND} -E touch ${SHADER_STUMP}
    WORKING_DIRECTORY "${SHADER_DIR}"
    DEPENDS ${SHADER_SRC}
    COMMENT "Compiling shaders..."
)

add_custom_target(Shaders DEPENDS ${SHADER_DIR}/${SHADER_STUMP})
add_dependencies(ExteriorMapping2 Shaders)

# Create debug and setups directories if they don't exist
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/debug)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/setups)