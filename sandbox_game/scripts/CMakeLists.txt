cmake_minimum_required(VERSION 3.20)
project(SandboxScripts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the engine SDK using the EngineConfig.cmake helper
# The SDK is located at ../sdk/ relative to sandbox_game/
find_package(Engine REQUIRED PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/cmake" NO_DEFAULT_PATH)

# Automatically add, configure, generate reflection for, and link user scripts
crimson_add_user_scripts(physgun_script)
