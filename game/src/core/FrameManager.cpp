#include "FrameManager.hpp"
#include <stdexcept>

FrameManager::FrameManager(VkDevice dev, VkQueue gQ, VkQueue pQ, VkSwapchainKHR sc, uint32_t framesInFlight)
    : device(dev), graphicsQueue(gQ), presentQueue(pQ), swapchain(sc), maxFramesInFlight(framesInFlight) {
}

FrameManager::~FrameManager() {
    // Ensure device is still valid
    if (!device) return;

    vkDeviceWaitIdle(device); // make sure GPU is done with all operations

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        if (imageAvailableSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);

        if (renderFinishedSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);

        if (inFlightFences[i] != VK_NULL_HANDLE)
            vkDestroyFence(device, inFlightFences[i], nullptr);
    }
}

void FrameManager::init(VkCommandPool commandPool) {

    uint32_t swapImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, nullptr);
    maxFramesInFlight = std::min(maxFramesInFlight, swapImageCount);

    imageAvailableSemaphores.resize(maxFramesInFlight);
    renderFinishedSemaphores.resize(maxFramesInFlight);
    inFlightFences.resize(maxFramesInFlight);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        if (vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }

    // Allocate command buffers
    commandBuffers.resize(maxFramesInFlight);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = maxFramesInFlight;

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

VkResult FrameManager::beginFrame(uint32_t& imageIndex) {
    // Wait for this frame's fence to ensure GPU is done
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // Acquire next image
    VkResult result = vkAcquireNextImageKHR(
        device,
        swapchain,
        UINT64_MAX, // consider using a finite timeout if needed
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return result; // swapchain needs recreation
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    // Reset fence for this frame before submitting commands
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // Reset command buffer for recording
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    return result;
}

void FrameManager::endFrame(uint32_t imageIndex) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        // caller should handle swapchain recreation
    }

    currentFrame = (currentFrame + 1) % maxFramesInFlight;
}
