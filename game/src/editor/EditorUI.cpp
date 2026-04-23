#include "editor/EditorUI.hpp"

#include <stdexcept>
#include <string>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "ecs/Registry.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Mesh.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/Transform.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "scenes/Scene.hpp"
#include "scenes/SceneManager.hpp"

EditorUI::EditorUI(Registry& registry, VulkanRenderer& renderer, SceneManager& sceneManager)
    : registry(registry), renderer(renderer), sceneManager(sceneManager) {
}

EditorUI::~EditorUI() {
    shutdown();
}

void EditorUI::initialize(GLFWwindow* window) {
    if (initialized) {
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    createDescriptorPool();

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        throw std::runtime_error("Failed to initialize ImGui GLFW backend");
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_0;
    initInfo.Instance = renderer.device.getInstance();
    initInfo.PhysicalDevice = renderer.device.getPhysicalDevice();
    initInfo.Device = renderer.device.getDevice();
    initInfo.QueueFamily = renderer.device.getGraphicsQueueFamily();
    initInfo.Queue = renderer.device.getGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = descriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.PipelineInfoMain.RenderPass = renderer.getRenderPass();
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        throw std::runtime_error("Failed to initialize ImGui Vulkan backend");
    }

    initialized = true;
}

void EditorUI::shutdown() {
    if (!initialized) {
        return;
    }

    vkDeviceWaitIdle(renderer.device.getDevice());
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    destroyDescriptorPool();
    initialized = false;
}

void EditorUI::beginFrame() {
    if (!initialized) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::drawPanels() {
    if (!initialized) {
        return;
    }

    ImGui::Begin("Inspector");
    ImGui::TextUnformatted("Runtime ECS Editor");

    char pathBuffer[260]{};
    scenePath.copy(pathBuffer, scenePath.size(), 0);
    pathBuffer[scenePath.size()] = '\0';
    if (ImGui::InputText("Scene Path", pathBuffer, sizeof(pathBuffer))) {
        scenePath = pathBuffer;
    }

    Scene* currentScene = sceneManager.getCurrentScene();
    if (ImGui::Button("Save Scene")) {
        if (currentScene && currentScene->saveToFile(scenePath)) {
            statusMessage = "Scene saved to " + scenePath;
        } else {
            statusMessage = "Failed to save scene.";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Scene")) {
        if (currentScene && currentScene->loadFromFile(scenePath)) {
            statusMessage = "Scene loaded from " + scenePath;
        } else {
            statusMessage = "Failed to load scene.";
        }
    }
    ImGui::TextWrapped("%s", statusMessage.c_str());

    for (auto [entity, transform, mesh, material, name] : registry.view<Transform, Mesh, Material, Name>()) {
        if (name.value != "Cube") {
            continue;
        }

        ImGui::Separator();
        ImGui::Text("Selected: %s", name.value.c_str());

        float position[3] = { transform.position.x, transform.position.y, transform.position.z };
        if (ImGui::DragFloat3("Position", position, 0.05f)) {
            transform.position.x = position[0];
            transform.position.y = position[1];
            transform.position.z = position[2];
        }

        float rotation[3] = { transform.rotation.x, transform.rotation.y, transform.rotation.z };
        if (ImGui::DragFloat3("Rotation", rotation, 0.5f)) {
            transform.rotation.x = rotation[0];
            transform.rotation.y = rotation[1];
            transform.rotation.z = rotation[2];
        }

        float scale[3] = { transform.scale.x, transform.scale.y, transform.scale.z };
        if (ImGui::DragFloat3("Scale", scale, 0.05f, 0.1f, 10.0f)) {
            transform.scale.x = scale[0];
            transform.scale.y = scale[1];
            transform.scale.z = scale[2];
        }

        float color[4] = { material.color.r, material.color.g, material.color.b, material.color.a };
        if (ImGui::ColorEdit4("Color", color)) {
            material.color.r = color[0];
            material.color.g = color[1];
            material.color.b = color[2];
            material.color.a = color[3];
        }

        break;
    }

    ImGui::End();
}

void EditorUI::render(VkCommandBuffer commandBuffer) {
    if (!initialized) {
        return;
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void EditorUI::createDescriptorPool() {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(renderer.device.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool");
    }
}

void EditorUI::destroyDescriptorPool() {
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(renderer.device.getDevice(), descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}
