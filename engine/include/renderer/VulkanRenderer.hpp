#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <stdexcept>
#include <glm/glm.hpp>

#include "core/VulkanDevice.hpp"
#include "core/VulkanSwapchain.hpp"
#include "core/VulkanPipeline.hpp"
#include "core/VulkanBuffer.hpp"
#include "core/VulkanDescriptors.hpp"
#include "core/VulkanCommandManager.hpp"
#include "core/VulkanFrameSync.hpp"

#include "../ecs/components/Mesh.hpp"
#include "../ecs/components/Material.hpp"
#include "../ecs/components/Transform.hpp"
#include "../ecs/components/Camera.hpp"
#include "../soa/MeshSoA.hpp"

/**
 * @struct PipelineHandle
 * @brief Holds a compiled graphics pipeline and its layout.
 */
struct PipelineHandle {
    /** @brief Vulkan pipeline object. */
    VkPipeline pipeline = VK_NULL_HANDLE;
    /** @brief Vulkan pipeline layout. */
    VkPipelineLayout layout = VK_NULL_HANDLE;
};

/**
 * @struct CameraUBO
 * @brief Representation of camera data inside Vulkan uniform buffers.
 */
struct CameraUBO {
    /** @brief View-projection matrix. */
    glm::mat4 viewProj;
};

/**
 * @struct InstanceData
 * @brief Structure containing transformation and ID parameters for a single renderable instance.
 */
struct InstanceData {
    /** @brief Transformation model matrix. */
    glm::mat4 model;
    /** @brief Identifier of the associated material. */
    uint32_t materialID;
    /** @brief Identifier of the associated mesh. */
    uint32_t meshID;
    /** @brief Instance color tint. */
    glm::vec4 color;
};

/**
 * @struct InstanceDataSoA
 * @brief CPU-side representation of batch-rendering instances organized as a Structure of Arrays.
 */
struct InstanceDataSoA {
    /** @brief Model matrices of all instances. */
    std::vector<glm::mat4> models;
    /** @brief Material IDs of all instances. */
    std::vector<uint32_t> materialIDs;
    /** @brief Mesh IDs of all instances. */
    std::vector<uint32_t> meshIDs;
    /** @brief Colors of all instances. */
    std::vector<glm::vec4> colors;

    /**
     * @brief Clears all array allocations.
     */
    void clear() {
        models.clear();
        materialIDs.clear();
        meshIDs.clear();
        colors.clear();
    }

    /**
     * @brief Appends a new instance.
     * @param inst Instance parameters.
     * @return Index of the new instance.
     */
    size_t push(const InstanceData& inst) {
        models.push_back(inst.model);
        meshIDs.push_back(inst.meshID);
        materialIDs.push_back(inst.materialID);
        colors.push_back(inst.color);
        return models.size() - 1;
    }

    /**
     * @brief Retrieves total active instance count.
     * @return Instance count.
     */
    size_t size() const { return models.size(); }

    /**
     * @brief Retrieves CPU instancing details at index formatted for GPU.
     * @param i Array index.
     * @return GPU format instance data.
     */
    InstanceDataGPU get(size_t i) const {
        return {
            models[i],
            colors[i],
            meshIDs[i],
            materialIDs[i]
        };
    }
};


class ResourceManager;

/**
 * @class VulkanRenderer
 * @brief Manages core Vulkan pipeline states, device interaction, frames, buffers, and command submissions.
 */
class VulkanRenderer {
public:
    VulkanRenderer(GLFWwindow* win, const std::string& exeDir = "", bool enableValidationLayers = true);
    /**
     * @brief Destroy the Vulkan Renderer object and free resources.
     */
    ~VulkanRenderer();

    /**
     * @brief Resolves a shader path relative to the executable or CWD to support packaged/SDK builds.
     * @param originalPath The hardcoded shader path to resolve.
     * @return The resolved absolute or relative path to the shader file.
     */
    std::string resolveShaderPath(const std::string& originalPath) const;

    /**
     * @brief Gets the directory where the active executable resides.
     */
    const std::string& getExeDir() const { return exeDir; }

    /**
     * @brief Starts render pass recording for the current frame.
     */
    void beginFrame();
    /**
     * @brief Submits command buffers and presents the rendered swapchain image.
     */
    void endFrame();

    /**
     * @brief Submits draw commands for a single mesh.
     * @param mesh The target mesh.
     * @param mat The applied material.
     * @param transform The transformation components.
     */
    void drawMesh(const Mesh& mesh, const Material& mat, const Transform& transform);
    /**
     * @brief Performs drawing of instanced mesh batches.
     */
    void drawInstances();

    /**
     * @brief Creates the GPU buffers for batch instancing.
     * @param maxInstances Allocated size bound.
     */
    void createInstanceBuffer(size_t maxInstances);
    /**
     * @brief Transfers CPU instances down to the GPU instance buffer.
     */
    void updateInstanceBuffer();

    /**
     * @brief Instantiates uniform buffers for camera view and projection matrices.
     */
    void createCameraUBO();
    /**
     * @brief Refreshes active camera matrix attributes in UBO.
     */
    void updateCameraUBO();

    /**
     * @brief Recreates the swapchain when window resizing occurs.
     */
    void recreateSwapchain();
    /**
     * @brief Deallocates all Vulkan rendering state and resource bindings.
     */
    void cleanup();

    /**
     * @brief Creates a graphic pipeline custom-configured with specified shader source files.
     * @param vertPath File path to compiled vertex shader.
     * @param fragPath File path to compiled fragment shader.
     * @return PipelineHandle wrapping VkPipeline and layout.
     */
    PipelineHandle createPipelineForShaders(const std::string& vertPath, const std::string& fragPath);
    /**
     * @brief Allocates and uploads GPU buffers for a specific mesh loaded on the CPU.
     * @param id Mesh ID.
     */
    void uploadMesh(size_t id);
    /**
     * @brief Returns window close request status.
     * @return True if close requested.
     */
    bool shouldClose() const;
    /**
     * @brief Calculates delta time since the previous frame.
     * @return Delta time in seconds.
     */
    float getDeltaTime();

    /**
     * @brief Gets GLFW window pointer.
     * @return Pointer to GLFW window.
     */
    GLFWwindow* getWindow() const { return window; }
    /**
     * @brief Checks if a key is currently pressed (executes within engine.dll context).
     * @param key GLFW key code.
     * @return True if pressed.
     */
    bool getKey(int key) const;
    /**
     * @brief Gets Vulkan RenderPass.
     * @return RenderPass handle.
     */
    VkRenderPass getRenderPass() const { return swapchain.getRenderPass(); }
    /**
     * @brief Gets Vulkan Pipeline.
     * @return Pipeline handle.
     */
    VkPipeline getPipeline() const { return pipeline.get(); }
    /**
     * @brief Gets Vulkan Pipeline Layout.
     * @return PipelineLayout handle.
     */
    VkPipelineLayout getPipelineLayout() const { return pipeline.getLayout(); }
    /**
     * @brief Gets Swapchain Extent.
     * @return Extent representation.
     */
    VkExtent2D getSwapchainExtent() const { return swapchain.getExtent(); }

    /**
     * @brief Retrieves active Vulkan command buffer for current frame.
     * @return VkCommandBuffer handle.
     */
    VkCommandBuffer getCurrentCommandBuffer() {
        return cmdManager.getCurrentCommandBuffer();
    }
    /**
     * @brief Begins command recording on a temporary command buffer.
     * @return VkCommandBuffer handle.
     */
    VkCommandBuffer beginSingleUseCommands() {
        return cmdManager.beginOneTimeCommand();
    }
    /**
     * @brief Finalizes and submits a temporary command buffer.
     * @param commandBuffer The temporary command buffer.
     */
    void endSingleUseCommands(VkCommandBuffer commandBuffer) {
        cmdManager.endOneTimeCommand(commandBuffer, device.getGraphicsQueue());
    }
    /**
     * @brief Gets Camera Descriptor Set.
     * @return VkDescriptorSet handle.
     */
    VkDescriptorSet getCameraDescriptorSet() const {
        return cameraDescriptorSet;
    }
    /**
     * @brief Gets Descriptor Set Layout for Textures.
     * @return VkDescriptorSetLayout handle.
     */
    VkDescriptorSetLayout getTextureDescriptorSetLayout() const {
        return descriptors.getTextureDescriptorSetLayout();
    }
    /**
     * @brief Gets Default White Texture Descriptor Set fallback.
     * @return VkDescriptorSet handle.
     */
    VkDescriptorSet getDefaultTextureSet() const;

    /**
     * @brief Configures active camera matrices.
     * @param viewProj View-Projection matrix.
     * @param position 3D position vector.
     */
    void setActiveCamera(const glm::mat4& viewProj, const glm::vec3& position) {
        activeCameraViewProj = viewProj;
        activeCameraPosition = position;
        hasActiveCameraData = true;
    }
    /**
     * @brief Configures active camera view, projection, and position.
     * @param projection Projection matrix.
     * @param position 3D position vector.
     * @param view View matrix.
     */
    void setActiveCamera(const glm::mat4& projection, const glm::vec3& position, const glm::mat4& view)
    {
        activeCameraView = view;
        activeCameraProjection = projection;
        activeCameraViewProj = projection * view;
        activeCameraPosition = position;
        hasActiveCameraData = true;
    }

    /**
     * @brief Checks if a camera is currently active.
     * @return True if active camera data is loaded, false otherwise.
     */
    bool hasActiveCamera() const { return hasActiveCameraData; }
    /**
     * @brief Gets active camera view projection matrix.
     * @return Mat4.
     */
    const glm::mat4& getActiveCameraViewProj() const { return activeCameraViewProj; }
    /**
     * @brief Gets active camera position.
     * @return Vec3.
     */
    const glm::vec3& getActiveCameraPosition() const { return activeCameraPosition; }
    /**
     * @brief Gets active camera view matrix.
     * @return Mat4.
     */
    const glm::mat4& getActiveCameraView() const { return activeCameraView; }
    /**
     * @brief Gets active camera projection matrix.
     * @return Mat4.
     */
    const glm::mat4& getActiveCameraProjection() const { return activeCameraProjection; }
    
    /** @brief Storage vector of created custom Vulkan pipelines. */
    std::vector<std::unique_ptr<VulkanPipeline>> pipelines;
    /** @brief CPU-side instancing data in Structure-of-Arrays format. */
    InstanceDataSoA instanceDataCPU;

    /** @brief Mesh database components in SoA layout. */
    MeshSoA meshSoA;
    /** @brief Vulkan GPU buffer for instance matrices. */
    VulkanBuffer instanceBuffer;

    /** @brief Core logical and physical Vulkan device interface. */
    VulkanDevice device{ true };
    /** @brief Swapchain manager module. */
    VulkanSwapchain swapchain;
    /** @brief Global Vulkan descriptor set management module. */
    VulkanDescriptors descriptors;
    /** @brief Global Resource Manager instance. */
    std::unique_ptr<ResourceManager> resourceManager;

private:
    /** @brief Pointer to GLFW window. */
    GLFWwindow* window = nullptr;
    /** @brief Directory where the executable is located. */
    std::string exeDir;


    /** @brief Debug messenger handle for validation layer callbacks. */
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
#ifdef DEBUG
    /** @brief Flag denoting validation checks state. */
    bool enableValidationLayers = true; // turn off for release
#else
    /** @brief Flag denoting validation checks state. */
    bool enableValidationLayers = false;
#endif
    /** @brief Default graphics pipeline state manager. */
    VulkanPipeline pipeline;
    /** @brief Command pool and command buffer allocator. */
    VulkanCommandManager cmdManager;
    /** @brief Synchronization fences and semaphores management. */
    VulkanFrameSync frameSync;
    /** @brief Presentation surface binding window and Vulkan instance. */
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    /** @brief Swapchain index of next image to render. */
    uint32_t currentImageIndex = 0;

    /** @brief GPU buffer for the camera UBO. */
    VulkanBuffer cameraBuffer;

    /** @brief descriptor set representing active camera. */
    VkDescriptorSet cameraDescriptorSet = VK_NULL_HANDLE;

    /** @brief Flag checking if window resized. */
    bool framebufferResized = false;
    /** @brief Previous frame time stamp. */
    double lastTime = 0.0;
    /** @brief Active camera view proj matrix cache. */
    glm::mat4 activeCameraViewProj{ 1.0f };
    /** @brief Active camera world space position cache. */
    glm::vec3 activeCameraPosition{ 0.0f };
    /** @brief Active camera view matrix cache. */
    glm::mat4 activeCameraView{};
    /** @brief Active camera projection matrix cache. */
    glm::mat4 activeCameraProjection{};
    /** @brief Flag checking if active camera is initialized. */
    bool hasActiveCameraData = false;

private:
    /** @brief Initializes main Vulkan instance, debuggers, devices, surfaces, and pipelines. */
    void initVulkan();
    /** @brief Prepares and compiles graphics pipeline configurations. */
    void createPipeline();
    /** @brief Couples window handle with Vulkan instance context. */
    void createWindowSurface();
    /** @brief Populates the Vulkan debug messenger structure parameters. */
    VkDebugUtilsMessengerCreateInfoEXT populateDebugMessengerCreateInfo();
    /** @brief Attaches validation layer logger callbacks. */
    void setupDebugMessenger();
    /** @brief Cleans up messenger validation logs. */
    void destroyDebugMessenger();
    /** @brief Instantiates the root Vulkan API instance. */
    void createInstanceAndDebug();
    /** @brief Creates physical/logical devices and queue interfaces. */
    void createDeviceAndQueues();
    /** @brief Initializes descriptor pools and configurations. */
    void setupDescriptors();
    /** @brief Configures swapchain render targets. */
    void createSwapchain();
    /** @brief Instantiates buffers and pipelines. */
    void createBuffersAndPipelines();
    /** @brief Sets up command buffers, pools, and sync fences. */
    void createCommandsAndSync();
    /** @brief Submits command buffers to graphic queue and schedules screen presentation. */
    void submitAndPresent();
};
