cmake_minimum_required(VERSION 3.15)
project(ExteriorMapping2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

# Prefer system packages, but fall back to FetchContent if not found
include(FetchContent)

# GLFW (prefer system package)
find_package(glfw3 3.3 QUIET)
if (NOT glfw3_FOUND)
    message(STATUS "GLFW not found via find_package — fetching with FetchContent")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.3.8
    )
    FetchContent_MakeAvailable(glfw)
endif()

# GLM (header-only)
find_package(glm QUIET)
if (NOT glm_FOUND)
    message(STATUS "GLM not found via find_package — fetching with FetchContent")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        0.9.9.8
    )
    FetchContent_MakeAvailable(glm)
endif()

# Resolve target names (system or FetchContent may provide different target names)
if (TARGET glfw::glfw)
    set(GLFW_TARGET glfw::glfw)
elseif (TARGET glfw)
    set(GLFW_TARGET glfw)
else()
    message(FATAL_ERROR "GLFW target not found after find_package or FetchContent")
endif()

if (TARGET glm::glm)
    set(GLM_TARGET glm::glm)
elseif (TARGET glm)
    set(GLM_TARGET glm)
else()
    message(FATAL_ERROR "GLM target not found after find_package or FetchContent")
endif()

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

# imgui backends need GLFW/Vulkan headers at compile time
target_link_libraries(imgui PUBLIC ${GLFW_TARGET} Vulkan::Vulkan)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/headers)


set(SHADER_STUMP ".compiled")
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
file(GLOB SHADER_SRC "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp")
file(GLOB SOURCES "${SRC_DIR}/*.cpp")

add_executable(ExteriorMapping2 ${SOURCES})

target_link_libraries(ExteriorMapping2
        ${GLFW_TARGET}
        ${GLM_TARGET}
        Vulkan::Vulkan
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