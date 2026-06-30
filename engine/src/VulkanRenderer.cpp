#include "../include/renderer/VulkanRenderer.hpp"
#include <iostream>

VulkanRenderer::VulkanRenderer(GLFWwindow* win, bool enableValidation)
    : window(win), enableValidationLayers(enableValidation)
{
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

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

//
// ─── Initialization ─────────────────────────────────────────────────────────────
//
// initVulkan() — orchestration (call this from ctor)
void VulkanRenderer::initVulkan() {
    createInstanceAndDebug();       // create instance & debug messenger
    createWindowSurface();          // must create surface before picking physical device
    createDeviceAndQueues();        // pick physical device using surface, create logical device
    createSwapchain();              // create swapchain (depends on device + surface)
    setupDescriptors();
    createBuffersAndPipelines();
    createCommandsAndSync();
}

// -----------------------------
// Instance & Debug
// -----------------------------
void VulkanRenderer::createInstanceAndDebug() {
    // This should create the VkInstance inside device.initialize()
    device.initialize();           // creates VkInstance
    setupDebugMessenger();         // debug messenger requires a valid instance
}

// -----------------------------
// Surface, Device & Swapchain
// -----------------------------
void VulkanRenderer::createWindowSurface() {
    if (surface != VK_NULL_HANDLE) return; // already created

    if (glfwCreateWindowSurface(device.getInstance(), window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
}

void VulkanRenderer::createDeviceAndQueues() {
    // surface must be valid here
    if (surface == VK_NULL_HANDLE) {
        throw std::runtime_error("createDeviceAndQueues called with null surface");
    }

    device.pickPhysicalDevice(surface);      // pick GPU that supports presentation to surface
    device.createLogicalDevice();            // create device + queues
}

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
void VulkanRenderer::setupDescriptors() {
    descriptors.create(device.getDevice());
    descriptors.createCameraDescriptorSetLayout();
}

void VulkanRenderer::createBuffersAndPipelines() {
    createCameraUBO();
    descriptors.allocateCameraDescriptorSets(cameraBuffer.get(), cameraBuffer.getSize());
    cameraDescriptorSet = descriptors.getCameraDescriptorSet();

    createInstanceBuffer(10000);
    createPipeline();
}

void VulkanRenderer::createCommandsAndSync() {
    cmdManager.create(device.getDevice(), device.getGraphicsQueueFamily(), 2);
    frameSync.create(device.getDevice(), 2);
}


void VulkanRenderer::createPipeline() {
    const std::string vert = "build/shaders/grid.vert.spv";
    const std::string frag = "build/shaders/grid.frag.spv";
    std::vector<VkDescriptorSetLayout> layouts = { descriptors.getCameraDescriptorSetLayout() };

    pipeline.create(device.getDevice(), swapchain.getExtent(), swapchain.getRenderPass(), vert, frag, layouts);
}

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

    VkClearValue clearColor = { {{0.1f, 0.1f, 0.1f, 1.0f}} };
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = swapchain.getRenderPass();
    rpInfo.framebuffer = swapchain.getFramebuffers().at(currentImageIndex);
    rpInfo.renderArea.offset = { 0, 0 };
    rpInfo.renderArea.extent = swapchain.getExtent();
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearColor;

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

void VulkanRenderer::endFrame() {
    VkCommandBuffer cmd = cmdManager.getCurrentCommandBuffer();
    vkCmdEndRenderPass(cmd);
    cmdManager.endFrame();

    submitAndPresent();
}

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

void VulkanRenderer::createInstanceBuffer(size_t maxInstances) {
    VkDeviceSize size = maxInstances * sizeof(glm::mat4);
    instanceBuffer.create(device.getDevice(), device.getPhysicalDevice(), size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

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

void VulkanRenderer::createCameraUBO() {
    cameraBuffer.create(device.getDevice(), device.getPhysicalDevice(), sizeof(CameraUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void VulkanRenderer::updateCameraUBO() {
    if (!hasActiveCameraData) return;
    cameraBuffer.uploadData(&activeCameraViewProj, sizeof(activeCameraViewProj));
}

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

void VulkanRenderer::cleanup() {
    vkDeviceWaitIdle(device.getDevice());

    destroyDebugMessenger();

    pipeline.destroy();
    descriptors.destroy();
    instanceBuffer.destroy();
    cameraBuffer.destroy();
    frameSync.destroy();
    cmdManager.destroy();
    swapchain.cleanup();
    device.cleanup();
}

PipelineHandle VulkanRenderer::createPipelineForShaders(const std::string& vert, const std::string& frag) {
    auto pipe = std::make_unique<VulkanPipeline>();
    pipe->create(device.getDevice(), swapchain.getExtent(), swapchain.getRenderPass(),
        vert, frag, { descriptors.getCameraDescriptorSetLayout() });

    VkPipeline p = pipe->get();
    VkPipelineLayout l = pipe->getLayout();
    pipelines.push_back(std::move(pipe)); // keep alive

    return { p, l };
}

float VulkanRenderer::getDeltaTime() {
    double currentTime = glfwGetTime();
    float dt = static_cast<float>(currentTime - lastTime);
    lastTime = currentTime;
    return dt;
}

bool VulkanRenderer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

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
void VulkanRenderer::destroyDebugMessenger() {
    if (!enableValidationLayers || debugMessenger == VK_NULL_HANDLE) return;

    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        device.getInstance(), "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(device.getInstance(), debugMessenger, nullptr);
    }
}

