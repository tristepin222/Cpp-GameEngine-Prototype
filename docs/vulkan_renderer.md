# Vulkan Rendering Engine

This document details the graphics architecture of the Vulkan Renderer. The renderer is designed to abstract raw Vulkan API complexity into clean, RAII C++ wrapper classes while preserving performance, supporting instanced drawing, and utilizing modern techniques like Push Constants.

```plantuml
@startuml VulkanLayer
package "Vulkan Core Wrapper" {
    class VulkanDevice {
        - instance : VkInstance
        - physicalDevice : VkPhysicalDevice
        - device : VkDevice
        - graphicsQueue : VkQueue
        - graphicsQueueFamily : uint32_t
    }

    class VulkanSwapchain {
        - swapChain : VkSwapChainKHR
        - swapChainImages : vector<VkImage>
        - swapChainImageViews : vector<VkImageView>
        - swapChainFramebuffers : vector<VkFramebuffer>
        - renderPass : VkRenderPass
        - swapChainExtent : VkExtent2D
    }

    class VulkanPipeline {
        - pipeline : VkPipeline
        - layout : VkPipelineLayout
    }

    class VulkanBuffer {
        - buffer : VkBuffer
        - memory : VkDeviceMemory
        - size : VkDeviceSize
        + uploadData(void*, size_t)
        + map() : void*
        + unmap()
    }

    class VulkanDescriptors {
        - descriptorPool : VkDescriptorPool
        - descriptorSetLayout : VkDescriptorSetLayout
        - descriptorSets : vector<VkDescriptorSet>
    }

    class VulkanCommandManager {
        - commandPool : VkCommandPool
        - commandBuffers : vector<VkCommandBuffer>
    }

    class VulkanFrameSync {
        - imageAvailableSemaphores : vector<VkSemaphore>
        - renderFinishedSemaphores : vector<VkSemaphore>
        - inFlightFences : vector<VkFence>
    }
}

class VulkanRenderer {
    + device : VulkanDevice
    + swapchain : VulkanSwapchain
    + pipeline : VulkanPipeline
    + cmdManager : VulkanCommandManager
    + frameSync : VulkanFrameSync
    + instanceBuffer : VulkanBuffer
    + meshSoA : MeshSoA
    + instanceDataCPU : InstanceDataSoA
    + beginFrame()
    + endFrame()
    + recreateSwapchain()
}

class MeshSoA {
    + vertexBuffers : vector<VulkanBuffer>
    + indexBuffers : vector<VulkanBuffer>
}

VulkanRenderer *-- VulkanDevice
VulkanRenderer *-- VulkanSwapchain
VulkanRenderer *-- VulkanPipeline
VulkanRenderer *-- VulkanCommandManager
VulkanRenderer *-- VulkanFrameSync
VulkanRenderer *-- VulkanBuffer : instanceBuffer
VulkanRenderer *-- MeshSoA
MeshSoA *-- VulkanBuffer : VBO/IBO
@enduml
```

---

## Vulkan Abstraction Layer

Vulkan requires explicit declaration of resources, layouts, synchronization, and hardware access. The engine implements a set of object-oriented C++ classes under `game/src/core/` to safely encapsulate Vulkan handles:

*   **[VulkanContext](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/VulkanContext.hpp)**: Stores instance-wide structures, validation layer callbacks, and physical device enumerations.
*   **[VulkanDevice](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/VulkanDevice.hpp)**: Handles hardware physical devices selection (prioritizing discrete GPUs) and logical device creation. Configures queue families for graphics, presentation, and transfers.
*   **[VulkanSwapchain](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/VulkanSwapChain.hpp)**: Manages screen resolution changes, double/triple buffering image chains, render pass configurations, swapchain image views, and framebuffers.
*   **[VulkanPipeline](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/VulkanPipeline.hpp)**: Coordinates shader layout state bindings, viewport/scissor setup, depth/stencil tests, color blending, rasterization, and multi-sampling configurations.
*   **[VulkanBuffer](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/VulkanBuffer.hpp)**: Encapsulates `VkBuffer` allocation and `VkDeviceMemory` binding. Supports staging allocations, GPU-local transfer operations, and persistent host mapping.
*   **[VulkanDescriptors](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/VulkanDescriptors.hpp)**: Standardizes descriptor layouts, bindings, pools, and set allocations for camera matrices and global uniforms.
*   **[VulkanCommandManager](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/VulkanCommandManager.hpp)**: Creates command pools and records frame buffer command sequences. Supports one-time command buffer execution for staging buffers uploads.
*   **[VulkanFrameSync](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/VulkanFrameSync.hpp)**: Holds CPU-GPU synchronization elements, preventing race conditions via fences and swapchain image-acquisition semaphores.

---

## Double-Buffered Frame Synchronization

To prevent the CPU from submitting draw commands faster than the GPU can process them (which would lead to visual artifacts or memory exhaust), the engine coordinates double-buffering using [VulkanFrameSync.hpp](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/VulkanFrameSync.hpp):

```cpp
struct VulkanFrameSync {
    std::vector<VkSemaphore> imageAvailableSemaphores; // GPU-GPU: image is ready for rendering
    std::vector<VkSemaphore> renderFinishedSemaphores; // GPU-GPU: rendering completed, ready to present
    std::vector<VkFence>     inFlightFences;           // CPU-GPU: CPU waits for frame to finish on GPU
};
```

### The Render Loop Sync Protocol
1.  **Wait for Frame Fence**: Before starting a new frame, the CPU waits on the `inFlightFence` of the current frame index (`vkWaitForFences`). This guarantees the GPU is finished executing the command buffer we're about to record into.
2.  **Acquire Next Image**: Request the next image from the swapchain using `vkAcquireNextImageKHR`, passing the `imageAvailableSemaphore`.
3.  **Reset Fence**: Reset the fence (`vkResetFences`) to lock the current frame buffer slot.
4.  **Submit Command Buffer**: Submit the recorded commands to the graphics queue (`vkQueueSubmit`).
    *   **Wait Stage**: Configured to wait on the `imageAvailableSemaphore` at the `VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT` stage.
    *   **Signal Stage**: Signals the `renderFinishedSemaphore` when drawing is completed.
    *   **Fence Trigger**: Passes the `inFlightFence` to automatically trigger when the GPU has finished execution.
5.  **Present Image**: Request presentation (`vkQueuePresentKHR`), waiting on the `renderFinishedSemaphore` to ensure rendering has finished.

---

## Shader & Graphics Pipeline Compilation

Shaders are written in GLSL and compiled to binary SPIR-V bytecode. The build process uses the `glslc` compiler to build:
*   `unlit.vert` / `unlit.frag` -> `unlit.vert.spv` / `unlit.frag.spv` (handles standard mesh drawing)
*   `grid.vert` / `grid.frag` -> `grid.vert.spv` / `grid.frag.spv` (handles infinite grid rendering)

The [PipelineBuilder](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/PipelineBuilder.hpp) dynamically builds the graphics pipelines. It links vertex input bindings, rasterization settings (cull modes, depth test enabling), color blending, and shader modules into a single `VkPipeline` state.

---

## Instanced Drawing & Push Constants

For maximum performance, the renderer avoids calling `vkCmdDraw` for every individual entity. Instead, it groups draw calls by **Mesh + Material combination** inside [RenderSystem.hpp](file:///f:/GitHub/Cpp-GameEngine-Prototype/game/src/core/RenderSystem.hpp).

### Per-Instance Data via Push Constants

Rather than storing model matrices in costly dynamic uniform buffers, the engine utilizes **Push Constants**. Push Constants reside in high-speed registers on the GPU, allowing instant access with zero memory overhead.

The push constant structure is defined as:
```cpp
struct PushConstants {
    glm::mat4 model;  // Entity transformation matrix
    glm::vec4 color;  // Base tint color
};
```

### The Batch Drawing Loop
1.  **Group Entities**: The `RenderSystem` aggregates active entities into batches of identical `Mesh*` and `Material*` keys.
2.  **Bind Pipeline & Descriptors**: Binds the material's pipeline and descriptor sets (which contain camera projection-view uniform buffer data).
3.  **Bind Vertex/Index Buffers**: Binds the mesh vertex buffer (VBO) and index buffer (IBO) once.
4.  **Draw Instances**: Iterates through the batch. For each instance:
    *   Fills the `PushConstants` struct with the instance's model matrix and color.
    *   Pushes data using `vkCmdPushConstants`.
    *   Calls `vkCmdDrawIndexed` with an instance count of 1.

By using push constants and batching vertex/index buffer bindings, the engine reduces CPU driver overhead and minimizes state changes.

---

## Infinite Grid Rendering

The engine implements a beautiful infinite grid component. It is drawn dynamically inside `drawGrids()` without requiring a complex vertex buffer:
1.  Binds the grid pipeline.
2.  Passes grid parameters (camera position, grid color, spacing, fade size) via a specialized grid push constant block.
3.  Calls `vkCmdDraw(cmd, 6, 1, 0, 0)` with 6 vertices.
4.  The `grid.vert` shader generates a screen-aligned quad on the fly. The `grid.frag` shader calculates grid lines in world space and applies a fading effect based on distance from the camera position, creating a smooth grid backdrop.
