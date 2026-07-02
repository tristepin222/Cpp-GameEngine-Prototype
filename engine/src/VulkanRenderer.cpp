#include "../include/renderer/VulkanRenderer.hpp"
#include "renderer/ResourceManager.hpp"
#include <iostream>

/**
 * @brief Construct a new Vulkan Renderer:: Vulkan Renderer object.
 * @param win Reference window.
 * @param enableValidation True to configure validation layers, false otherwise.
 */
VulkanRenderer::VulkanRenderer(GLFWwindow* win, bool enableValidation)
    : window(win), enableValidationLayers(enableValidation)
{
    resourceManager = std::make_unique<ResourceManager>();

    // Set up window resize callback
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
        auto renderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
        if (renderer) {
            renderer->framebufferResized = true;
        }
    });
    glfwSetWindowUserPointer(window, this);

    initVulkan();
    lastTime = glfwGetTime();
}

/**
 * @brief Destroy the Vulkan Renderer:: Vulkan Renderer object.
 */
VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

//
// ─── Initialization ─────────────────────────────────────────────────────────────
//
// initVulkan() — orchestration (call this from ctor)
/**
 * @brief Initializes the Vulkan API contexts.
 */
void VulkanRenderer::initVulkan() {
    createInstanceAndDebug();       // create instance & debug messenger
    createWindowSurface();          // must create surface before picking physical device
    createDeviceAndQueues();        // pick physical device using surface, create logical device
    createSwapchain();              // create swapchain (depends on device + surface)
    setupDescriptors();
    createCommandsAndSync();
    createBuffersAndPipelines();
}

// -----------------------------
// Instance & Debug
// -----------------------------
/**
 * @brief Creates Vulkan instance and hooks validation callback logs.
 */
void VulkanRenderer::createInstanceAndDebug() {
    // This should create the VkInstance inside device.initialize()
    device.initialize();           // creates VkInstance
    setupDebugMessenger();         // debug messenger requires a valid instance
}

// -----------------------------
// Surface, Device & Swapchain
// -----------------------------
/**
 * @brief Creates the presentation surface coupling Vulkan and window context.
 */
void VulkanRenderer::createWindowSurface() {
    if (surface != VK_NULL_HANDLE) return; // already created

    if (glfwCreateWindowSurface(device.getInstance(), window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
}

/**
 * @brief Selects physical GPU and configures device queues.
 */
void VulkanRenderer::createDeviceAndQueues() {
    // surface must be valid here
    if (surface == VK_NULL_HANDLE) {
        throw std::runtime_error("createDeviceAndQueues called with null surface");
    }

    device.pickPhysicalDevice(surface);      // pick GPU that supports presentation to surface
    device.createLogicalDevice();            // create device + queues
}

/**
 * @brief Initializes swapchain presentation structures.
 */
void VulkanRenderer::createSwapchain() {
    swapchain.initialize(
        device.getDevice(),
        device.getPhysicalDevice(),
        surface,
        device.getGraphicsQueueFamily()
    );
}

// -----------------------------
// Remaining setup
// -----------------------------
/**
 * @brief Setup descriptor layouts and allocations.
 */
void VulkanRenderer::setupDescriptors() {
    descriptors.create(device.getDevice());
    descriptors.createCameraDescriptorSetLayout();
    descriptors.createTextureDescriptorSetLayout();
}

/**
 * @brief Creates uniform buffers and compiles base pipeline.
 */
void VulkanRenderer::createBuffersAndPipelines() {
    createCameraUBO();
    descriptors.allocateCameraDescriptorSets(cameraBuffer.get(), cameraBuffer.getSize());
    cameraDescriptorSet = descriptors.getCameraDescriptorSet();

    // Create the default white texture fallback
    resourceManager->createDefaultWhiteTexture(*this);

    createInstanceBuffer(10000);
    createPipeline();
}

/**
 * @brief Allocates command pools and sync structures.
 */
void VulkanRenderer::createCommandsAndSync() {
    cmdManager.create(device.getDevice(), device.getGraphicsQueueFamily(), 2);
    frameSync.create(device.getDevice(), 2);
}


/**
 * @brief Builds the default graphics pipeline configuration.
 */
void VulkanRenderer::createPipeline() {
    const std::string vert = "build/shaders/grid.vert.spv";
    const std::string frag = "build/shaders/grid.frag.spv";
    std::vector<VkDescriptorSetLayout> layouts = { descriptors.getCameraDescriptorSetLayout() };

    pipeline.create(device.getDevice(), swapchain.getExtent(), swapchain.getRenderPass(), vert, frag, layouts);
}

/**
 * @brief Prepares Vulkan contexts for command recording.
 */
void VulkanRenderer::beginFrame() {
    frameSync.waitForCurrentFrame();

    VkResult res = vkAcquireNextImageKHR(
        device.getDevice(),
        swapchain.getSwapchain(),
        UINT64_MAX,
        frameSync.getImageAvailableSemaphore(),
        VK_NULL_HANDLE,
        &currentImageIndex
    );

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    cmdManager.beginFrame();
    VkCommandBuffer cmd = cmdManager.getCurrentCommandBuffer();

    VkClearValue clearValues[2]{};
    clearValues[0].color = { {0.1f, 0.1f, 0.1f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = swapchain.getRenderPass();
    rpInfo.framebuffer = swapchain.getFramebuffers().at(currentImageIndex);
    rpInfo.renderArea.offset = { 0, 0 };
    rpInfo.renderArea.extent = swapchain.getExtent();
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain.getExtent().width);
    viewport.height = static_cast<float>(swapchain.getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain.getExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getLayout(), 0, 1, &cameraDescriptorSet, 0, nullptr);
}

/**
 * @brief Stops active render pass recording.
 */
void VulkanRenderer::endFrame() {
    VkCommandBuffer cmd = cmdManager.getCurrentCommandBuffer();
    vkCmdEndRenderPass(cmd);
    cmdManager.endFrame();

    submitAndPresent();
}

/**
 * @brief Submits command buffers to graphics queue.
 */
void VulkanRenderer::submitAndPresent() {
    VkCommandBuffer cmdBuf = cmdManager.getCurrentCommandBuffer();

    VkSemaphore waitSemaphores[] = { frameSync.getImageAvailableSemaphore() };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { frameSync.getRenderFinishedSemaphore() };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, frameSync.getInFlightFence()) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit draw command buffer");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR sc = swapchain.getSwapchain();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &sc;
    presentInfo.pImageIndices = &currentImageIndex;

    VkResult res = vkQueuePresentKHR(device.getGraphicsQueue(), &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
    }
    else if (res != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    frameSync.nextFrame();
}

/**
 * @brief Allocates memory for model transformation instance buffer.
 * @param maxInstances Number of maximum instances.
 */
void VulkanRenderer::createInstanceBuffer(size_t maxInstances) {
    VkDeviceSize size = maxInstances * sizeof(glm::mat4);
    instanceBuffer.create(device.getDevice(), device.getPhysicalDevice(), size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

/**
 * @brief Uploads instanced structures to GPU buffers.
 */
void VulkanRenderer::updateInstanceBuffer() {
    if (instanceDataCPU.size() == 0) return;

    std::vector<InstanceDataGPU> gpuData(instanceDataCPU.size());
    for (size_t i = 0; i < instanceDataCPU.size(); i++) {
        gpuData[i].model = instanceDataCPU.models[i];
        gpuData[i].color = instanceDataCPU.colors[i];
        // optionally include meshID/materialID if your shader needs it
    }

    instanceBuffer.uploadData(gpuData.data(), gpuData.size() * sizeof(InstanceDataGPU));
}

/**
 * @brief Allocates memory for Camera Uniform Buffer.
 */
void VulkanRenderer::createCameraUBO() {
    cameraBuffer.create(device.getDevice(), device.getPhysicalDevice(), sizeof(CameraUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

/**
 * @brief Maps and uploads camera projection matrices to GPU.
 */
void VulkanRenderer::updateCameraUBO() {
    if (!hasActiveCameraData) return;
    cameraBuffer.uploadData(&activeCameraViewProj, sizeof(activeCameraViewProj));
}

/**
 * @brief Deallocates current swapchain and rebuilds it matching window dimensions.
 */
void VulkanRenderer::recreateSwapchain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device.getDevice());

    // Clean up old swapchain-dependent objects
    pipeline.destroy();
    swapchain.cleanup();

    // Recreate swapchain
    swapchain.initialize(
        device.getDevice(),
        device.getPhysicalDevice(),
        surface,
        device.getGraphicsQueueFamily()
    );

    // Recreate pipeline using new render pass & extent
    createPipeline();
}

/**
 * @brief releases allocated Vulkan resources.
 */
void VulkanRenderer::cleanup() {
    vkDeviceWaitIdle(device.getDevice());

    destroyDebugMessenger();

    if (resourceManager) {
        resourceManager->cleanup(device.getDevice());
    }

    pipeline.destroy();
    descriptors.destroy();
    instanceBuffer.destroy();
    cameraBuffer.destroy();
    frameSync.destroy();
    cmdManager.destroy();
    swapchain.cleanup();
    device.cleanup();
}

/**
 * @brief Helper utility creating a separate graphics pipeline using target shaders.
 * @param vert path to vertex bytecode.
 * @param frag path to fragment bytecode.
 * @return Pipeline handle.
 */
PipelineHandle VulkanRenderer::createPipelineForShaders(const std::string& vert, const std::string& frag) {
    auto pipe = std::make_unique<VulkanPipeline>();
    std::vector<VkDescriptorSetLayout> layouts = {
        descriptors.getCameraDescriptorSetLayout(),
        descriptors.getTextureDescriptorSetLayout()
    };
    pipe->create(device.getDevice(), swapchain.getExtent(), swapchain.getRenderPass(),
        vert, frag, layouts);

    VkPipeline p = pipe->get();
    VkPipelineLayout l = pipe->getLayout();
    pipelines.push_back(std::move(pipe)); // keep alive

    return { p, l };
}

/**
 * @brief Computes frame delta time since last invocation.
 * @return Delta time value.
 */
float VulkanRenderer::getDeltaTime() {
    double currentTime = glfwGetTime();
    float dt = static_cast<float>(currentTime - lastTime);
    lastTime = currentTime;
    return dt;
}

/**
 * @brief Verification if window close signals are active.
 * @return True if close requested.
 */
bool VulkanRenderer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

/**
 * @brief Uploads mesh data to vertex and index buffers.
 * @param meshID ID of target mesh.
 */
void VulkanRenderer::uploadMesh(size_t meshID) {
    auto& vertices = meshSoA.vertices[meshID];
    auto& indices = meshSoA.indices[meshID];

    // Vertex buffer
    if (!vertices.empty()) {
        VkDeviceSize size = vertices.size() * sizeof(Vertex);
        meshSoA.vertexBuffers[meshID].create(
            device.getDevice(),
            device.getPhysicalDevice(),
            size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        meshSoA.vertexBuffers[meshID].uploadData(vertices.data(), size);
    }

    // Index buffer
    if (!indices.empty()) {
        VkDeviceSize size = indices.size() * sizeof(uint32_t);
        meshSoA.indexBuffers[meshID].create(
            device.getDevice(),
            device.getPhysicalDevice(),
            size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        meshSoA.indexBuffers[meshID].uploadData(indices.data(), size);
    }
}
/**
 * @brief Populates validation debug create info parameters.
 * @return debug create info.
 */
VkDebugUtilsMessengerCreateInfoEXT VulkanRenderer::populateDebugMessengerCreateInfo() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* userData) -> VkBool32 {
            std::cerr << "[Vulkan Validation] " << pCallbackData->pMessage << std::endl;
            return VK_FALSE;
        };
    createInfo.pUserData = nullptr;
    return createInfo;
}
/**
 * @brief Attaches validation logger callbacks.
 */
void VulkanRenderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    auto createInfo = populateDebugMessengerCreateInfo();

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        device.getInstance(), "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        if (func(device.getInstance(), &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create debug messenger!");
        }
    }
}
/**
 * @brief Deallocates debug messenger log handle.
 */
void VulkanRenderer::destroyDebugMessenger() {
    if (!enableValidationLayers || debugMessenger == VK_NULL_HANDLE) return;

    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        device.getInstance(), "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(device.getInstance(), debugMessenger, nullptr);
    }
}

/**
 * @brief Retrieves the default white texture descriptor set fallback.
 * @return VkDescriptorSet handle.
 */
VkDescriptorSet VulkanRenderer::getDefaultTextureSet() const {
    if (resourceManager) {
        return resourceManager->getDefaultWhiteTexture()->descriptorSet;
    }
    return VK_NULL_HANDLE;
}

