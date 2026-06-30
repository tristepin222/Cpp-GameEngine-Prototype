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
    src/core/FrameManager.cpp
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
    src/editor/EditorUI.cpp
    src/scenes/Scene.cpp
    src/scenes/JSONUtils.cpp
    src/scenes/SceneManager.cpp
    src/scenes/SceneSerializer.cpp
)

add_library(engine STATIC ${ENGINE_SOURCES})

# Public include directories (include/ and src/ are exported to linking targets)
target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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

# Target for builtin shaders compilation
add_custom_target(compile_builtin_shaders ALL DEPENDS ${BUILTIN_SHADERS_SPV})
add_dependencies(engine compile_builtin_shaders)

message(STATUS "Configured engine static library")
