cmake_minimum_required(VERSION 3.20)
project(CinemachinePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Custom command to generate reflection boilerplate
if(NOT CMAKE_GENERATOR_PLATFORM MATCHES "ARM" AND NOT CMAKE_CROSSCOMPILING)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/src/generated_reflection.cpp"
        COMMAND $<TARGET_FILE:reflection_generator> "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/generated_reflection.cpp"
        DEPENDS reflection_generator "${CMAKE_CURRENT_SOURCE_DIR}/include/CinemachineComponent.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/include/CinemachineSystem.hpp"
        COMMENT "[Reflection Generator] Parsing annotations and generating registration code for Cinemachine..."
    )
else()
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/src/generated_reflection.cpp"
        COMMAND ${CMAKE_COMMAND} -E echo "[Reflection Generator] Skipping generation for Cinemachine (ARM/Cross-compilation, using pre-generated file)"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/CinemachineComponent.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/include/CinemachineSystem.hpp"
        COMMENT "[Reflection Generator] Skipping generation for Cinemachine..."
    )
endif()

set(PLUGIN_SOURCES
    src/generated_reflection.cpp
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


# Post-Build Step: Copy DLL to sdk/plugins/ so build_game.bat can pick it up
set(PLUGIN_SDK_OUTPUT "${CMAKE_SOURCE_DIR}/sdk/plugins")
add_custom_command(TARGET cinemachine_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_SDK_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cinemachine_plugin> "${PLUGIN_SDK_OUTPUT}/"
    COMMENT "Copying Cinemachine plugin DLL to sdk/plugins/..."
)

