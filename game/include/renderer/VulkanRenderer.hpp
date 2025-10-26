#pragma once

#include <vulkan/vulkan.h>
#include <glfw3.h>
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
#include "../soa/TransformSoA.hpp"
#include "../soa/CameraSoA.hpp"
#include "../soa/MeshSoA.hpp"

struct PipelineHandle {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
};
// === Data Structures ===
struct CameraUBO {
    glm::mat4 viewProj;
};

struct InstanceData {
    glm::mat4 model;
    uint32_t materialID;
    uint32_t meshID;
    glm::vec4 color;
};

struct InstanceDataSoA {
    std::vector<glm::mat4> models;
    std::vector<uint32_t> materialIDs;
    std::vector<uint32_t> meshIDs;
    std::vector<glm::vec4> colors;

    void clear() {
        models.clear();
        materialIDs.clear();
        meshIDs.clear();
        colors.clear();
    }

    size_t push(const InstanceData& inst) {
        models.push_back(inst.model);
        meshIDs.push_back(inst.meshID);
        materialIDs.push_back(inst.materialID);
        colors.push_back(inst.color);
        return models.size() - 1;
    }

    size_t size() const { return models.size(); }

    InstanceDataGPU get(size_t i) const {
        return {
            models[i],
            colors[i],
            meshIDs[i],
            materialIDs[i]
        };
    }
};


// === VulkanRenderer Class ===
class VulkanRenderer {
public:
    explicit VulkanRenderer(GLFWwindow* win, bool enableValidationLayers = true);
    ~VulkanRenderer();

    // Frame lifecycle
    void beginFrame();
    void endFrame();

    // Draw helpers
    void drawMesh(const Mesh& mesh, const Material& mat, const Transform& transform);
    void drawInstances();

    // Buffers
    void createInstanceBuffer(size_t maxInstances);
    void updateInstanceBuffer();

    // Camera
    void createCameraUBO();
    void updateCameraUBO();

    // Swapchain / cleanup
    void recreateSwapchain();
    void cleanup();

    PipelineHandle createPipelineForShaders(const std::string& vertPath, const std::string& fragPath);
    void uploadMesh(size_t id);
    bool shouldClose() const;
    float getDeltaTime();

    // Getters
    GLFWwindow* getWindow() const { return window; }
    VkRenderPass getRenderPass() const { return swapchain.getRenderPass(); }
    VkPipeline getPipeline() const { return pipeline.get(); }
    VkPipelineLayout getPipelineLayout() const { return pipeline.getLayout(); }
    VkExtent2D getSwapchainExtent() const { return swapchain.getExtent(); }

    VkCommandBuffer getCurrentCommandBuffer() {
        return cmdManager.getCurrentCommandBuffer();
    }
    VkDescriptorSet getCameraDescriptorSet() const {
        return cameraDescriptorSet;
    }
    std::vector<std::unique_ptr<VulkanPipeline>> pipelines;
    // ECS instance buffer
    InstanceDataSoA instanceDataCPU;

    CameraSoA cameraSoA;
    TransformSoA transformSoA; // or pass it from your ECS
    MeshSoA meshSoA;
    VulkanBuffer instanceBuffer;

    VulkanDevice device{ true };
    VulkanSwapchain swapchain;
    VulkanDescriptors descriptors;

private:
    GLFWwindow* window = nullptr;


    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    bool enableValidationLayers = true; // turn off for release

    // Core Vulkan modules
    VulkanPipeline pipeline;
    VulkanCommandManager cmdManager;
    VulkanFrameSync frameSync;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    uint32_t currentImageIndex = 0;

    // Buffers
    VulkanBuffer cameraBuffer;

    // Descriptor set
    VkDescriptorSet cameraDescriptorSet = VK_NULL_HANDLE;

    // State
    bool framebufferResized = false;
    double lastTime = 0.0;

private:
    void initVulkan();
    void createPipeline();
    void createWindowSurface();
    VkDebugUtilsMessengerCreateInfoEXT populateDebugMessengerCreateInfo();
    void setupDebugMessenger();
    void destroyDebugMessenger();
    void createInstanceAndDebug();
    void createDeviceAndQueues();
    void setupDescriptors();
    void createSwapchain();
    void createBuffersAndPipelines();
    void createCommandsAndSync();
    void submitAndPresent();
};
