cmake_minimum_required(VERSION 3.20)
project(SandboxScripts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the engine SDK using the EngineConfig.cmake helper
# The SDK is located at ../sdk/ relative to sandbox_game/
find_package(Engine REQUIRED PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/cmake" NO_DEFAULT_PATH)

set(SCRIPT_SOURCES
    src/PhysgunScript.cpp
)

add_library(physgun_script SHARED ${SCRIPT_SOURCES})

# Include cinemachine plugin headers so we can access CinemachineVirtualCamera properties
target_include_directories(physgun_script PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/cinemachine/include"
)

# Link against the engine SDK imported targets
target_link_libraries(physgun_script PRIVATE
    Engine::engine
)

# Set the output directory of the compiled DLL to be sandbox_game/scripts/ directly
set_target_properties(physgun_script PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}"
)
