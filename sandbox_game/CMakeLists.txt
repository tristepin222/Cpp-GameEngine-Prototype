cmake_minimum_required(VERSION 3.20)
project(SandboxGame LANGUAGES CXX)

# Executable target sources
set(SANDBOX_SOURCES
    src/main.cpp
)

add_executable(sandbox_game ${SANDBOX_SOURCES})

# Target Include Directories (add game-specific src directory for local resolution)
target_include_directories(sandbox_game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link against the engine library
# (This propagates Vulkan, GLFW, GLM, ImGui, ImGuizmo, and engine include directories automatically!)
target_link_libraries(sandbox_game PRIVATE engine)

# Post-Build Steps:
# 1. Copy compiled shaders (engine builtin shaders) to target output folder
# 2. Copy game-specific assets folder to target output folder

add_custom_command(TARGET sandbox_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_BINARY_DIR}/shaders"
    "$<TARGET_FILE_DIR:sandbox_game>/build/shaders"
    COMMENT "Copying compiled shaders to sandbox run folder..."
)

add_custom_command(TARGET sandbox_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:sandbox_game>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:sandbox_game>/assets"
    COMMENT "Copying game assets to sandbox run folder..."
)

add_custom_command(TARGET sandbox_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/project.settings"
    "$<TARGET_FILE_DIR:sandbox_game>/project.settings"
    COMMENT "Copying project settings to sandbox run folder..."
)

add_custom_command(TARGET sandbox_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "$<TARGET_FILE:engine>"
    "$<TARGET_FILE_DIR:sandbox_game>/"
    COMMENT "Copying engine DLL next to game executable..."
)

# Rename binary output target name to 'game' or keep 'sandbox_game'
set_target_properties(sandbox_game PROPERTIES OUTPUT_NAME "game")

message(STATUS "Configured sandbox game executable target")
