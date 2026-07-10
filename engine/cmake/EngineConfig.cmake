# EngineConfig.cmake
# Consumed by game projects via: find_package(Engine REQUIRED PATHS <sdk>/cmake)
# Sets up Engine::engine as a fully self-contained imported SHARED target.

cmake_minimum_required(VERSION 3.20)

# Resolve absolute path to the SDK root (one level above this cmake/ directory)
get_filename_component(ENGINE_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# ------------------------------------------------------------------
# Vulkan must be installed on the user's machine (like a GPU driver)
# ------------------------------------------------------------------
find_package(Vulkan REQUIRED)

# ------------------------------------------------------------------
# GLFW pre-built static lib
# ------------------------------------------------------------------
add_library(Engine::glfw STATIC IMPORTED GLOBAL)
set_target_properties(Engine::glfw PROPERTIES
    IMPORTED_LOCATION             "${ENGINE_SDK_ROOT}/lib/glfw3.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ENGINE_SDK_ROOT}/include/GLFW"
)

# ------------------------------------------------------------------
# GLM (header-only)
# ------------------------------------------------------------------
add_library(Engine::glm INTERFACE IMPORTED GLOBAL)
set_target_properties(Engine::glm PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ENGINE_SDK_ROOT}/include"
)

# ------------------------------------------------------------------
# ImGui pre-built static lib
# ------------------------------------------------------------------
add_library(Engine::imgui STATIC IMPORTED GLOBAL)
set_target_properties(Engine::imgui PROPERTIES
    IMPORTED_LOCATION             "${ENGINE_SDK_ROOT}/lib/imgui.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ENGINE_SDK_ROOT}/third_party/imgui;${ENGINE_SDK_ROOT}/third_party/imgui/backends"
)

# ------------------------------------------------------------------
# ImGuizmo pre-built static lib
# ------------------------------------------------------------------
add_library(Engine::imguizmo STATIC IMPORTED GLOBAL)
set_target_properties(Engine::imguizmo PROPERTIES
    IMPORTED_LOCATION             "${ENGINE_SDK_ROOT}/lib/imguizmo.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ENGINE_SDK_ROOT}/third_party/imguizmo"
)

# ------------------------------------------------------------------
# Engine shared library (the main DLL)
# ------------------------------------------------------------------
add_library(Engine::engine SHARED IMPORTED GLOBAL)
set_target_properties(Engine::engine PROPERTIES
    IMPORTED_LOCATION "${ENGINE_SDK_ROOT}/bin/engine.dll"
    IMPORTED_IMPLIB   "${ENGINE_SDK_ROOT}/lib/engine.lib"
    INTERFACE_INCLUDE_DIRECTORIES
        # Public engine headers (include/ merged with src/ for Vulkan internals)
        "${ENGINE_SDK_ROOT}/include;${ENGINE_SDK_ROOT}/src;${ENGINE_SDK_ROOT}/third_party/imgui;${ENGINE_SDK_ROOT}/third_party/imgui/backends;${ENGINE_SDK_ROOT}/third_party/imguizmo"
    INTERFACE_LINK_LIBRARIES
        "Vulkan::Vulkan;Engine::glfw;Engine::glm;Engine::imgui;Engine::imguizmo"
)

message(STATUS "[Engine SDK] Loaded from: ${ENGINE_SDK_ROOT}")
