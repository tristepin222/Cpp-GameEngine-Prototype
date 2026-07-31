cmake_minimum_required(VERSION 3.20)
project(AStarPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Custom command to generate reflection boilerplate
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/src/generated_reflection.cpp"
    COMMAND $<TARGET_FILE:reflection_generator> "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/generated_reflection.cpp"
    DEPENDS reflection_generator "${CMAKE_CURRENT_SOURCE_DIR}/include/AStarSystem.hpp"
    COMMENT "[Reflection Generator] Parsing annotations and generating registration code for AStar..."
)

set(PLUGIN_SOURCES
    src/generated_reflection.cpp
    src/AStarSystem.cpp
)

add_library(astar_plugin SHARED ${PLUGIN_SOURCES})

# Public includes
target_include_directories(astar_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against the engine shared library and third party dependencies
target_link_libraries(astar_plugin PRIVATE
    engine
    imgui
    glm::glm
    glfw
)


# Post-Build Step: Copy DLL to sdk/plugins/ so build_game.bat can pick it up
set(PLUGIN_SDK_OUTPUT "${CMAKE_SOURCE_DIR}/sdk/plugins")
add_custom_command(TARGET astar_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_SDK_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:astar_plugin> "${PLUGIN_SDK_OUTPUT}/"
    COMMENT "Copying AStar plugin DLL to sdk/plugins/..."
)

