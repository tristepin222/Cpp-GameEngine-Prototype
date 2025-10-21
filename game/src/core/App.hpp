#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <glfw3.h>
#include "VulkanContext.hpp"
#include "SwapchainManager.hpp"
#include "../renderer/Renderer.hpp"
#include "FrameManager.hpp"
#include "Buffer.hpp"
#include "../include/ecs/components/Mesh.hpp"
#include <optional>
#include "Window.hpp"
#include "../include/ecs/uniforms/uniforms.hpp"
#include "../include/ecs/MeshManager.hpp"
#include "../include/ecs/components/Primitives.hpp"
#include "../include/ecs/Registry.hpp"
#include "../include/ecs/components/Camera.hpp"
#include "../include/ecs/systems/CameraSystem.hpp"
#include "../include/ecs/components/inputComponent.hpp"
#include "../Include/ecs/systems/InputSystem.hpp"

class App {
public:
    App() = default;
    ~App() {
        if (window) glfwDestroyWindow(window->getHandle());
        glfwTerminate();
    }

    void run() {

        window = std::make_unique<Window>(1280, 720, "DOD Renderer");
        window->setResizeCallback([this](int w, int h) { onWindowResized(w, h); });

        Entity CameraEntity = registry.create();

        registry.emplace<Camera>(CameraEntity, Camera{});
        registry.emplace<Transform>(CameraEntity, Transform{ glm::vec3(2, 2, 2), glm::vec3(0), glm::vec3(1) });
        registry.emplace<InputComponent>(CameraEntity, InputComponent{});

        initVulkan();

        Mesh& cubeMesh = meshManager.addMesh(
            Primitives::cubeVertices(),
            Primitives::cubeIndices()
        );


        meshManager.uploadToGPU(context.graphicsQueue, context.commandPool);

        setupCamera();
        mainLoop();
    }

    uint16_t maxInstances = 100000;

private:
    Registry registry;
    std::unique_ptr<Window> window;
    MeshManager meshManager; // central manager for all meshes
    CameraSystem cameraSystem{ registry };
    InputSystem inputSystem{ registry, window->getHandle() };

    VulkanContext context;
    SwapchainManager swapchain;
    std::optional<Renderer> renderer;
    std::optional<FrameManager> frameManager;
    std::optional<DescriptorPool> descriptorPool;

    std::vector<VulkanBuffer> ownedVertexBuffers;
    std::vector<VulkanBuffer> ownedIndexBuffers;


    void initVulkan() {
        context.init(window->getHandle());
        swapchain.create(context.device, context.physicalDevice, context.surface, { 1280, 720 });

        const uint32_t framesInFlight = 2;

        meshManager = MeshManager(context.device, context.physicalDevice);

        renderer.emplace(context.device, swapchain.extent, framesInFlight, meshManager);

        frameManager.emplace(context.device, context.graphicsQueue, context.presentQueue, swapchain.swapchain, framesInFlight);
        descriptorPool.emplace(context.device, framesInFlight);
        // Allocate per-frame instance buffers
        renderer->allocateBuffers(context.physicalDevice, 100000);

        // Initialize command buffers
        frameManager->init(context.commandPool);

    }
    void onWindowResized(int width, int height) {
        recreateSwapchain();
        std::cout << "[onWindowResized] Swapchain recreated: "
            << width << "x" << height << std::endl;
    }

    void setupCamera() {
        CameraUBO cameraUBO{};
        cameraUBO.view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        cameraUBO.proj = glm::perspective(glm::radians(45.0f),
            swapchain.extent.width / (float)swapchain.extent.height, 0.1f, 100.0f);
        cameraUBO.proj[1][1] *= -1; // Vulkan Y-flip
        renderer->updateCameraUBO(frameManager->currentFrame, cameraUBO, context);

        renderer->initDescriptorSets(*descriptorPool, maxInstances);
        renderer->setupPipeline(context.device, swapchain.renderPass);
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window->getHandle())) {
            glfwPollEvents();
            float dt = 1;
            inputSystem.update(dt);
            cameraSystem.update(dt);


            uint32_t imageIndex = 0;

            // --- Handle window resize or outdated swapchain ---

            VkResult result = frameManager->beginFrame(imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                onWindowResized(swapchain.extent.width, swapchain.extent.height);
                continue; // Skip this frame
            }
            else if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to acquire swapchain image!");
            }

            VkCommandBuffer cmd = frameManager->commandBuffers[frameManager->currentFrame];
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(cmd, &beginInfo);

            renderer->renderFrame(
                cmd,
                swapchain.framebuffers[imageIndex],
                swapchain.renderPass,
                swapchain.extent,
                frameManager->currentFrame
            );

            vkEndCommandBuffer(cmd);
            frameManager->endFrame(imageIndex);
        }

        vkDeviceWaitIdle(context.device);
    }
    void recreateSwapchain() {
        int width = 0, height = 0;
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window->getHandle(), &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(context.device);

        // 1) Let renderer free any swapchain-dependent resources it owns (pipelines, layouts).
        if (renderer) renderer->cleanupSwapchainResources();

        // 2) Destroy old swapchain resources (SwapchainManager takes care of image views / framebuffers / renderPass as needed)
        swapchain.cleanup(context.device);

        // 3) Recreate swapchain (images, image views, framebuffers, renderPass if implemented)
        VkExtent2D newExtent{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        swapchain.create(context.device, context.physicalDevice, context.surface, newExtent);

        // 4) Update frame manager swapchain handle (so acquire/present use new swapchain)
        if (frameManager) frameManager->setSwapchain(swapchain.swapchain);

        // 5) Recreate pipeline now that swapchain.renderPass is valid
        if (renderer) {
            renderer->setupPipeline(context.device, swapchain.renderPass);
        }

        // 6) Re-initialize descriptor sets AFTER pipelineLayout is valid.
        //    descriptorPool should remain live; do not recreate it here. If you do recreate pool, recreate it before this call.

        if (descriptorPool) {
            vkResetDescriptorPool(context.device, *descriptorPool, 0); // or destroy+recreate
        }

        if (renderer && descriptorPool) {
            renderer->initDescriptorSets(*descriptorPool, maxInstances);
        }

        // 7) Update camera/projection UBO for new aspect ratio
        CameraUBO ubo{};
        ubo.view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        ubo.proj = glm::perspective(glm::radians(45.0f), (float)newExtent.width / (float)newExtent.height, 0.1f, 100.0f);
        ubo.proj[1][1] *= -1.0f;
        if (renderer && frameManager) {
            renderer->updateCameraUBO(frameManager->currentFrame, ubo, context);
        }

        std::cout << "[recreateSwapchain] recreated to " << newExtent.width << "x" << newExtent.height << std::endl;
    }

};
