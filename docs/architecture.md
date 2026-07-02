# Engine Architecture

This document describes the high-level architecture and system layout of the C++ Game Engine Prototype.

## System Design Overview

The engine is structured as a modular desktop application in C++20, utilizing GLFW for OS windowing/events and Vulkan for rendering. The application maintains a clear separation between core systems, resource managers, systems logic, editor GUI, and the application level.

![System Architecture](diagrams/out/architecture/Architecture.png)

### Architectural Layers

1.  **Application Layer (`Application.cpp`, `SceneManager`, `DefaultScene`)**: 
    The bootstrap/entry point. It reads runtime configurations, creates the window, instantiates the ECS Registry, binds standard systems, and dynamically loads the startup scene.
2.  **Entity-Component System (ECS) Layer (`Registry`, `EntityManager`, `ComponentStorage`)**:
    A lightweight, custom ECS backend that manages entities, component allocations, and subscriptions. Component data is stored contiguously in memory pools (`ComponentStorage`) to maintain maximum cache locality.
3.  **Systems Layer (`System`, `SystemManager`)**:
    Encapsulates logical behaviours (rendering, user input, camera movement) by querying active components via ECS "Views" and executing updates once per frame.
4.  **Editor Layer (`EditorUI`, `EditorModeState`)**:
    An interactive debug panel and WYSIWYG editor powered by ImGui and ImGuizmo. Enables real-time entity manipulation, scene hierarchy inspection, component modification, and scene load/save options. Can be disabled for standalone play.
5.  **Renderer Layer (`VulkanRenderer`, `core/` abstractions)**:
    Wraps Vulkan objects (devices, swapchains, buffers, descriptor pools, command managers, pipelines) in custom, safe RAII classes. Manages double-buffering, data upload to VRAM, depth buffers, and draw command recording.

---

## Frame Lifecycle & Execution Flow

The engine operates on a single-threaded game loop inside `Engine::Application::run()`. The lifecycle of a single frame comprises polling OS events, updating active scenes, updating ECS systems, drawing the UI panels (in editor mode), and submitting Vulkan command buffers for drawing.

### Core Loop Structure (`Application::run()`)

Below is the C++ structural skeleton of the game loop:

```cpp
void Application::run() {
    onStart();

    while (running && !renderer->shouldClose()) {
        // 1. Poll OS windowing and inputs events
        glfwPollEvents();
        float dt = renderer->getDeltaTime();

        // 2. Custom game override ticks
        onUpdate(dt);

        // 3. Update scene transitions and logic
        sceneManager.update(dt);
        
        // 4. Process ECS systems (InputSystem -> CameraSystem -> RenderSystem)
        systemManager.updateAll(dt);
        
        if (config.enableEditor && editorUI) {
            // 5a. Record editor panel overlays
            editorUI->beginFrame();
            editorUI->drawPanels();
            
            // 6a. Render frame with ImGui overlay
            renderSystem->drawFrame([this](VkCommandBuffer cmd) {
                editorUI->render(cmd);
            });
        } else {
            // 5b. Standalone mode: Render clean viewport fullscreen (no UI)
            renderSystem->drawFrame();
        }
    }

    onShutdown();
}
```

The sequence below illustrates the frame execution flow:

![Frame Lifecycle Sequence](diagrams/out/frame_lifecycle/FrameLifecycle.png)

### Detailed Loop Phases

*   **OS Event Polling**: Calls `glfwPollEvents()` to update mouse/keyboard and window resizing data.
*   **Scene Update**: Updates active scene logic via `SceneManager::update(dt)`.
*   **ECS Systems Update**: Walks through the registered list of systems in the `SystemManager` and calls `update(dt)`:
    *   **Input System**: Reads mouse delta and WSADQE keys. When `Fly Mode` is active, it hides the cursor and updates the `InputComponent`'s look/move vectors. When `Edit Mode` is active, it releases the mouse cursor back to ImGui.
    *   **Camera System**: Reads the `InputComponent` and updates the active camera's view-projection matrices. Computes movement vectors in 3D space, applying mouse sensitivity and speeds, then propagates updates to the `VulkanRenderer`.
    *   **Render System (Update)**: Compiles active entity model matrices, colors, materials, and mesh configurations into a CPU cache (`InstanceDataSoA`) to prepare for instanced draw calls.
*   **Editor Panel Draw**: Records ImGui layouts, inspects properties of the selected entity, captures viewport mouse clicks for Raycast picking, and applies 3D transformations via ImGuizmo.
*   **Render Draw Frame**: The `RenderSystem::drawFrame` coordinates Vulkan-specific recording:
    1.  Acquires an image from the swapchain.
    2.  Pushes camera uniform buffer data (View-Projection matrix).
    3.  Uploads instance data (model matrices, colors) to the GPU dynamic instance buffer.
    4.  Draws grid elements and model batches using push constants.
    5.  Executes the overlay callback, triggering ImGui Vulkan backend render commands.
    6.  Submits the recorded command buffer to the graphics queue and presents the rendered swapchain image.

---

## Data-Driven Scenes & Standalone Configuration

Instead of compiling hardcoded C++ scene classes (like `TestScene`), the engine parses level data dynamically at runtime:

1. **`DefaultScene`**: Reads a serializable JSON file on launch and invokes `SceneSerializer` to instantiate ECS entities and upload assets automatically.
2. **`project.settings`**: A settings configuration file adjacent to the executable that sets the startup window title, dimensions, starting scene path, and the `enableEditor` toggle flag.
3. **Standalone Mode**: Setting `"enableEditor": false` locks mouse cursor control by default, enables flight movement controls, and disables the ImGui overlay and windowing hooks entirely.

---

## Asset Pipeline & Resource Caching

The engine implements a lightweight asset manager (`ResourceManager`) to handle external model geometries and image file decoding:

* **glTF Parser (`cgltf`)**: Recursively reads nodes, meshes, triangles, and textures, creating standard ECS entities with vertex buffers uploaded directly to GPU memory.
* **Texture Loader (`stb_image`)**: Decodes image formats into pixels, staging memory copies onto device-local Vulkan images. Applies a vertical flip on load to match Vulkan's downward Y-coordinate space.
* **Resource Caching**: Maintains mapping tables of loaded meshes/textures. If multiple entities share the same asset path, the manager serves cached descriptor sets, preventing duplicate VRAM allocations.
* **Fallback white texture**: A default 1x1 white texture bounds to materials without designated textures to maintain visual compatibility inside shaders.

---

## Architectural Trade-Offs

### Single-Threaded Game Loop
* **Why**: The entire game loop operates on the main thread. This choice was made to simplify Vulkan command buffer recording and swapchain synchronization. Multi-threaded command submission in Vulkan requires complex layout barrier tracking and thread-local command pools, which can add significant overhead and synchronization bugs.
* **Trade-off**: While this reduces pipeline complexity, it limits CPU throughput. In a production engine, long-running systems (like asset loading or physics calculation) would be offloaded to worker threads using a task scheduler.

### Decoupled Scene / Engine Splitting
* **Why**: The core engine is built as a static library, keeping it completely separated from the sandbox game executable. Game code only defines custom components, linking to the library.
* **Trade-off**: This isolates systems nicely but requires compiling custom serialization hooks for custom game components (via `ComponentSerializerRegistry`) to bridge the engine-library boundary. However, using the templated `registerComponent<T>` helper encapsulates serialization directly inside the component struct.
