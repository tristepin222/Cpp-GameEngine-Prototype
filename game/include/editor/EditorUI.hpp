#pragma once

#include <string>
#include <vulkan/vulkan.h>

struct GLFWwindow;
class Registry;
class SceneManager;
class VulkanRenderer;

class EditorUI {
public:
    EditorUI(Registry& registry, VulkanRenderer& renderer, SceneManager& sceneManager);
    ~EditorUI();

    void initialize(GLFWwindow* window);
    void shutdown();
    void beginFrame();
    void drawPanels();
    void render(VkCommandBuffer commandBuffer);

private:
    void createDescriptorPool();
    void destroyDescriptorPool();

    Registry& registry;
    VulkanRenderer& renderer;
    SceneManager& sceneManager;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    bool initialized = false;
    std::string scenePath = "assets/scenes/test_scene.json";
    std::string statusMessage = "Scene not saved yet.";
};
