cmake_minimum_required(VERSION 3.20)
project(EngineWorkspace LANGUAGES CXX)

# Require C++20 standard globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Dependencies Configuration (Shared across projects) ---

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

# Fetch cgltf
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.14
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cgltf)

# Fetch stb (for stb_image)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

# Fetch miniaudio
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(miniaudio)

# --- Add Subdirectories ---
# sandbox_game is now a standalone project — build it with build_game.bat
add_subdirectory(engine)
add_subdirectory(plugins)

message(STATUS "Configured engine workspace subdirectories")

