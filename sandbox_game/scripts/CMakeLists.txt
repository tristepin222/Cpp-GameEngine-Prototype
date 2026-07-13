cmake_minimum_required(VERSION 3.20)
project(SandboxScripts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the engine SDK using the EngineConfig.cmake helper
# The SDK is located at ../sdk/ relative to sandbox_game/
find_package(Engine REQUIRED PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/cmake" NO_DEFAULT_PATH)

# Path to the cross-platform reflection generator host tool inside SDK
if(WIN32)
    set(REFLECTION_GENERATOR_EXE "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/bin/reflection_generator.exe")
else()
    set(REFLECTION_GENERATOR_EXE "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/bin/reflection_generator")
endif()

set(SCRIPT_SOURCES
    src/PhysgunScript.cpp
)

# Set up custom command to compile/generate script reflection boilerplate code automatically
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/src/generated_reflection.cpp"
    COMMAND "${REFLECTION_GENERATOR_EXE}" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src/generated_reflection.cpp"
    DEPENDS ${SCRIPT_SOURCES}
    COMMENT "[Reflection Generator] Parsing annotations and generating registration code..."
)

# Include the generated source file in compilation target
list(APPEND SCRIPT_SOURCES src/generated_reflection.cpp)

add_library(physgun_script SHARED ${SCRIPT_SOURCES})

# Include cinemachine plugin headers so we can access CinemachineVirtualCamera properties
target_include_directories(physgun_script PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/cinemachine/include"
)

# Link against the engine SDK imported targets
target_link_libraries(physgun_script PRIVATE
    Engine::engine
)

# Set the output directory of the compiled DLL to sandbox_game/bin/ (automatically ignored by git)
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bin")
set_target_properties(physgun_script PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
)
