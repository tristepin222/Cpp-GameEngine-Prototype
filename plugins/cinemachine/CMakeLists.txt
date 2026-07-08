cmake_minimum_required(VERSION 3.20)
project(CinemachinePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_SOURCES
    src/CinemachinePlugin.cpp
    src/CinemachineSystem.cpp
)

add_library(cinemachine_plugin SHARED ${PLUGIN_SOURCES})

# Public includes
target_include_directories(cinemachine_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against the engine shared library and third party dependencies
target_link_libraries(cinemachine_plugin PRIVATE
    engine
    imgui
    glm::glm
    glfw
)

# Post-Build Step: Copy DLL directly to sandbox_game's executable plugins directory
add_custom_command(TARGET cinemachine_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sandbox_game>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cinemachine_plugin> "$<TARGET_FILE_DIR:sandbox_game>/plugins/"
    COMMENT "Copying Cinemachine plugin DLL to sandbox plugins directory..."
)
