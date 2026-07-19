cmake_minimum_required(VERSION 3.20)
project(EngineLibrary LANGUAGES CXX)

# --- Third-Party Static Libraries ---

# ImGui Static Library
add_library(imgui STATIC
    ../third_party/imgui/imgui.cpp
    ../third_party/imgui/imgui_draw.cpp
    ../third_party/imgui/imgui_tables.cpp
    ../third_party/imgui/imgui_widgets.cpp
    ../third_party/imgui/backends/imgui_impl_glfw.cpp
    ../third_party/imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)

target_link_libraries(imgui PRIVATE glfw Vulkan::Vulkan)

# ImGuizmo Static Library
add_library(imguizmo STATIC
    ../third_party/imguizmo/GraphEditor.cpp
    ../third_party/imguizmo/ImCurveEdit.cpp
    ../third_party/imguizmo/ImGradient.cpp
    ../third_party/imguizmo/ImGuizmo.cpp
    ../third_party/imguizmo/ImSequencer.cpp
)

target_include_directories(imguizmo PUBLIC ${CMAKE_SOURCE_DIR}/third_party/imguizmo)
target_link_libraries(imguizmo PRIVATE imgui glm::glm)

# --- Engine Library Target ---

set(ENGINE_SOURCES
    src/VulkanRenderer.cpp
    src/core/Application.cpp
    src/core/PluginManager.cpp
    src/core/FrameManager.cpp
    src/core/JobSystem.cpp
    src/core/PipelineBuilder.cpp
    src/core/SwapchainManager.cpp
    src/core/VulkanBuffer.cpp
    src/core/VulkanCommandManager.cpp
    src/core/VulkanContext.cpp
    src/core/VulkanDescriptors.cpp
    src/core/VulkanDevice.cpp
    src/core/VulkanFrameSync.cpp
    src/core/VulkanPipeline.cpp
    src/core/VulkanSwapChain.cpp
    src/ecs/EntityFactory.cpp
    src/ecs/EntityCloner.cpp
    src/ecs/Registry.cpp
    src/ecs/systems/PlayerControllerSystem.cpp
    src/editor/EditorUI.cpp
    src/scenes/Scene.cpp
    src/scenes/JSONUtils.cpp
    src/scenes/ComponentSerializerRegistry.cpp
    src/scenes/SceneManager.cpp
    src/scenes/SceneManagement.cpp
    src/scenes/SceneSerializer.cpp
    src/scenes/TilesetAsset.cpp
    src/renderer/ResourceManager.cpp
    src/meta/ComponentReflection.cpp
    src/ufbx.cpp
    src/miniaudio.cpp
    src/ecs/systems/AudioSystem.cpp
    src/ecs/systems/TilemapSystem.cpp
    src/ecs/systems/UISystem.cpp
)

add_library(engine SHARED ${ENGINE_SOURCES})
set_target_properties(engine PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)
if(MSVC)
    target_compile_options(engine PRIVATE /Zi)
    target_link_options(engine PRIVATE /DEBUG)
endif()

# Public include directories (include/ and src/ are exported to linking targets)
target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../sandbox_game/scripts/public
    ${cgltf_SOURCE_DIR}
    ${stb_SOURCE_DIR}
    ${miniaudio_SOURCE_DIR}
)

target_link_libraries(engine PUBLIC
    glfw
    glm::glm
    Vulkan::Vulkan
    imgui
    imguizmo
)

# --- Builtin Shaders Compilation ---
# Compile grid.vert, grid.frag, unlit.vert, unlit.frag into SPIR-V.
# Output compiled shaders to $<TARGET_FILE_DIR:engine>/build/shaders/

find_program(Vulkan_GLSLC NAMES glslc)

function(compile_builtin_shader SHADER_PATH)
    get_filename_component(SHADER_NAME ${SHADER_PATH} NAME)
    set(SPV_PATH "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    
    add_custom_command(
        OUTPUT ${SPV_PATH}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND ${Vulkan_GLSLC} ${SHADER_PATH} -o ${SPV_PATH}
        DEPENDS ${SHADER_PATH}
        COMMENT "Compiling engine builtin shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
    )
    list(APPEND BUILTIN_SHADERS_SPV ${SPV_PATH})
    set(BUILTIN_SHADERS_SPV ${BUILTIN_SHADERS_SPV} PARENT_SCOPE)
endfunction()

set(BUILTIN_SHADERS_SPV "")
compile_builtin_shader("${CMAKE_CURRENT_SOURCE_DIR}/builtin_shaders/grid.vert")
compile_builtin_shader("${CMAKE_CURRENT_SOURCE_DIR}/builtin_shaders/grid.frag")
compile_builtin_shader("${CMAKE_CURRENT_SOURCE_DIR}/builtin_shaders/unlit.vert")
compile_builtin_shader("${CMAKE_CURRENT_SOURCE_DIR}/builtin_shaders/unlit.frag")
compile_builtin_shader("${CMAKE_CURRENT_SOURCE_DIR}/builtin_shaders/skinned.vert")
compile_builtin_shader("${CMAKE_CURRENT_SOURCE_DIR}/builtin_shaders/lit.vert")
compile_builtin_shader("${CMAKE_CURRENT_SOURCE_DIR}/builtin_shaders/lit.frag")
compile_builtin_shader("${CMAKE_CURRENT_SOURCE_DIR}/builtin_shaders/skinned_lit.vert")

# Target for builtin shaders compilation
add_custom_target(compile_builtin_shaders ALL DEPENDS ${BUILTIN_SHADERS_SPV})
add_dependencies(engine compile_builtin_shaders)

message(STATUS "Configured engine static library")

# --- Editor Executable ---
add_executable(editor editor/main.cpp)
target_link_libraries(editor PRIVATE engine)
set_target_properties(editor PROPERTIES OUTPUT_NAME "editor")
target_include_directories(editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(editor PRIVATE /Zi)
    target_link_options(editor PRIVATE /DEBUG)
endif()

# --- Headless Game Runtime Executable (no editor UI) ---
add_executable(game_runtime runtime/main.cpp)
target_link_libraries(game_runtime PRIVATE engine)
set_target_properties(game_runtime PROPERTIES OUTPUT_NAME "game_runtime")
target_include_directories(game_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Reflection Generator Host Tool ---
add_executable(reflection_generator src/tools/reflection_generator.cpp)
set_target_properties(reflection_generator PROPERTIES OUTPUT_NAME "reflection_generator")

message(STATUS "Configured editor, game_runtime, and reflection_generator executables")
