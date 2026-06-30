cmake_minimum_required(VERSION 3.20)
project(CppGameEnginePrototype LANGUAGES CXX)

# Require C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Redirect runtime binary output folder directly to the game/ directory.
# This aligns the executable path with the assets/ and build/ folders.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/game")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/game")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/game")

# --- Dependencies Configuration ---

# 1. Vulkan SDK
find_package(Vulkan REQUIRED)
message(STATUS "Found Vulkan SDK: ${Vulkan_LIBRARIES}")

# 2. FetchContent for GLFW (v3.4) and GLM (v1.0.1)
include(FetchContent)

# Fetch GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
# Disable building extra stuff in GLFW dependency to save compile time
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(glfw)

# Fetch GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

# --- Third-Party Static Libraries ---

# ImGui Static Library
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_vulkan.cpp
)

# ImGui Includes
target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)

# Link ImGui to GLFW and Vulkan
target_link_libraries(imgui PRIVATE glfw Vulkan::Vulkan)

# ImGuizmo Static Library
add_library(imguizmo STATIC
    third_party/imguizmo/GraphEditor.cpp
    third_party/imguizmo/ImCurveEdit.cpp
    third_party/imguizmo/ImGradient.cpp
    third_party/imguizmo/ImGuizmo.cpp
    third_party/imguizmo/ImSequencer.cpp
)

# ImGuizmo Includes & Linkage
target_include_directories(imguizmo PUBLIC ${CMAKE_SOURCE_DIR}/third_party/imguizmo)
target_link_libraries(imguizmo PRIVATE imgui glm::glm)

# --- Game Engine Executable ---

# Explicit list of game engine source files
set(GAME_SOURCES
    game/src/main.cpp
    game/src/VulkanRenderer.cpp
    
    # Core Vulkan Wrappers
    game/src/core/FrameManager.cpp
    game/src/core/PipelineBuilder.cpp
    game/src/core/SwapchainManager.cpp
    game/src/core/VulkanBuffer.cpp
    game/src/core/VulkanCommandManager.cpp
    game/src/core/VulkanContext.cpp
    game/src/core/VulkanDescriptors.cpp
    game/src/core/VulkanDevice.cpp
    game/src/core/VulkanFrameSync.cpp
    game/src/core/VulkanPipeline.cpp
    game/src/core/VulkanSwapChain.cpp
    
    # Editor & Scenes & Renderer Systems
    game/src/editor/EditorUI.cpp
    game/src/scenes/SceneManager.cpp
    game/src/scenes/TestScene.cpp
)

# Define executable target
add_executable(game ${GAME_SOURCES})

# Target Include Directories
target_include_directories(game PRIVATE
    ${CMAKE_SOURCE_DIR}/game/include
    ${CMAKE_SOURCE_DIR}/game/src
)

# Target Linkage
target_link_libraries(game PRIVATE
    glfw
    glm::glm
    Vulkan::Vulkan
    imgui
    imguizmo
)

# Print confirmation
message(STATUS "Configured project target: game")
