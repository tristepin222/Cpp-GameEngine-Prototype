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

# Helper macro to add user script libraries with reflection and SDK dependencies
function(crimson_add_user_scripts TARGET_NAME)
    set(SCRIPT_PUBLIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/public")
    set(SCRIPT_PRIVATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/private")

    # Glob all source files recursively
    file(GLOB_RECURSE SCRIPT_SOURCES 
        "${SCRIPT_PRIVATE_DIR}/*.cpp"
        "${SCRIPT_PRIVATE_DIR}/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    )
    # Exclude generated_reflection.cpp from dependencies to prevent circular dependency
    list(REMOVE_ITEM SCRIPT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/generated_reflection.cpp")

    # Glob all headers recursively
    file(GLOB_RECURSE SCRIPT_HEADERS 
        "${SCRIPT_PUBLIC_DIR}/*.hpp"
        "${SCRIPT_PUBLIC_DIR}/*.h"
    )

    # Make sure we have at least one source file to compile
    if(NOT SCRIPT_SOURCES)
        message(FATAL_ERROR "[Engine SDK] No source files (.cpp/.cc) found in private/ or src/ folders!")
    endif()

    # Find the reflection generator executable path inside SDK
    if(EXISTS "${ENGINE_SDK_ROOT}/bin/reflection_generator.exe")
        set(REFLECTION_GENERATOR_EXE "${ENGINE_SDK_ROOT}/bin/reflection_generator.exe")
    elseif(EXISTS "${ENGINE_SDK_ROOT}/reflection_generator.exe")
        set(REFLECTION_GENERATOR_EXE "${ENGINE_SDK_ROOT}/reflection_generator.exe")
    else()
        set(REFLECTION_GENERATOR_EXE "${ENGINE_SDK_ROOT}/bin/reflection_generator.exe")
    endif()

    # Define generated file path
    set(GENERATED_REF_CPP "${CMAKE_CURRENT_SOURCE_DIR}/src/generated_reflection.cpp")

    # Set up custom command to compile/generate script reflection boilerplate code automatically
    if(NOT CMAKE_GENERATOR_PLATFORM MATCHES "ARM" AND NOT CMAKE_CROSSCOMPILING)
        add_custom_command(
            OUTPUT "${GENERATED_REF_CPP}"
            COMMAND "${REFLECTION_GENERATOR_EXE}" "${SCRIPT_PUBLIC_DIR}" "${GENERATED_REF_CPP}"
            DEPENDS ${SCRIPT_SOURCES} ${SCRIPT_HEADERS}
            COMMENT "[Reflection Generator] Parsing annotations and generating registration code..."
        )
    else()
        add_custom_command(
            OUTPUT "${GENERATED_REF_CPP}"
            COMMAND ${CMAKE_COMMAND} -E echo "[Reflection Generator] Skipping generation for scripts (ARM/Cross-compilation, using pre-generated file)"
            DEPENDS ${SCRIPT_SOURCES} ${SCRIPT_HEADERS}
            COMMENT "[Reflection Generator] Skipping generation..."
        )
    endif()

    # Include the generated source file in compilation target
    list(APPEND SCRIPT_SOURCES "${GENERATED_REF_CPP}")

    # Declare shared library target
    add_library(${TARGET_NAME} SHARED ${SCRIPT_SOURCES})

    # Set include directories
    target_include_directories(${TARGET_NAME} PRIVATE
        "${SCRIPT_PUBLIC_DIR}"
        "${SCRIPT_PRIVATE_DIR}"
        "${ENGINE_SDK_ROOT}/include/plugins/cinemachine"
        "${ENGINE_SDK_ROOT}/include/plugins/AStar"
        "${ENGINE_SDK_ROOT}/plugins/cinemachine/include"
    )

    # Link against Engine SDK
    target_link_libraries(${TARGET_NAME} PRIVATE Engine::engine)

    # Direct compiled DLL output to the project bin/ folder
    set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bin")
    set_target_properties(${TARGET_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
    )
    
    message(STATUS "[Engine SDK] Configured script library target: ${TARGET_NAME}")
endfunction()
