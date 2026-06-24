#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vulkan/vulkan.h>

#include "../ecs/Entity.hpp"
#include "EditorModeState.hpp"
#include "../ecs/components/Transform.hpp"

struct GLFWwindow;
class Registry;
class SceneManager;
class VulkanRenderer;

class EditorUI {
public:
    EditorUI(Registry& registry, VulkanRenderer& renderer, SceneManager& sceneManager, EditorModeState& editorMode);
    ~EditorUI();

    void initialize(GLFWwindow* window);
    void shutdown();
    void beginFrame();
    void drawPanels();
    void render(VkCommandBuffer commandBuffer);
    void toggleFlyMode();

private:
    void handleViewportPicking();
    void createDescriptorPool();
    void destroyDescriptorPool();
    void drawSceneControls();
    void drawHierarchyPanel();
    void drawInspectorPanel();
    void drawTransformEditor();
    void drawMaterialEditor();
    void drawGridEditor();
    void drawCameraEditor();
    void drawSectionHeader(const std::string& title);
    bool drawVec3Control(const char* label, float* values, float speed = 0.05f);
    void applyInputMode();
    void drawDebugPanel();
    void drawGizmo();
    void decomposeMatrixToTransform(const glm::mat4& mat, Transform& t);

    Registry& registry;
    VulkanRenderer& renderer;
    SceneManager& sceneManager;
    EditorModeState& editorMode;
    GLFWwindow* window = nullptr;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    bool initialized = false;
    std::string scenePath = "assets/scenes/test_scene.json";
    std::string statusMessage = "Scene not saved yet.";
    std::string renameBuffer;
    Entity selectedEntity{};
    bool hasSelection = false;
    bool previousLeftMouseDown = false;
    glm::vec3 lastPickRayOrigin{ 0.0f };
    glm::vec3 lastPickRayDirection{ 0.0f, 0.0f, -1.0f };
    std::string lastPickNearestEntityName = "None";
    float lastPickNearestDistance = -1.0f;
    std::string lastPickResult = "No pick attempted yet.";
    bool previousToggleKeyDown = false;
};
