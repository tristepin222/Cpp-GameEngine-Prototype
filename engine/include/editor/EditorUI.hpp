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

/**
 * @class EditorUI
 * @brief Handles rendering of the editor GUI using ImGui, scene hierarchy, inspector, gizmos, and raycast picking.
 */
class EditorUI {
public:
    /**
     * @brief Construct a new Editor UI object.
     * @param registry Reference to the ECS Registry.
     * @param renderer Reference to the Vulkan Renderer.
     * @param sceneManager Reference to the Scene Manager.
     * @param editorMode Reference to the Editor Mode State.
     */
    EditorUI(Registry& registry, VulkanRenderer& renderer, SceneManager& sceneManager, EditorModeState& editorMode);
    /**
     * @brief Destroy the Editor UI object.
     */
    ~EditorUI();

    /**
     * @brief Initializes ImGui bindings and structures.
     * @param window Pointer to the GLFW window.
     */
    void initialize(GLFWwindow* window);
    /**
     * @brief Shuts down ImGui and releases descriptor pools.
     */
    void shutdown();
    /**
     * @brief Starts a new ImGui frame.
     */
    void beginFrame();
    /**
     * @brief Processes and populates ImGui panel layout commands.
     */
    void drawPanels();
    /**
     * @brief Submits ImGui draw data to the current Vulkan command buffer.
     * @param commandBuffer Command buffer to write commands to.
     */
    void render(VkCommandBuffer commandBuffer);
    /**
     * @brief Toggles between first-person fly camera controls and editor UI control modes.
     */
    void toggleFlyMode();

private:
    /**
     * @brief Processes viewport clicking rays for entity selection.
     */
    void handleViewportPicking();
    /**
     * @brief Creates Vulkan descriptor pool specifically for ImGui.
     */
    void createDescriptorPool();
    /**
     * @brief Releases ImGui Vulkan descriptor pool resources.
     */
    void destroyDescriptorPool();
    /**
     * @brief Renders panel controls for scene operations (New/Load/Save/Spawn).
     */
    void drawSceneControls();
    /**
     * @brief Renders hierarchical entity list panel.
     */
    void drawHierarchyPanel();
    /**
     * @brief Renders detail inspector panel for the selected entity.
     */
    void drawInspectorPanel();
    /**
     * @brief Renders inline controls for Transform component fields.
     */
    void drawTransformEditor();
    /**
     * @brief Renders inline controls for Material component fields.
     */
    void drawMaterialEditor();
    /**
     * @brief Renders the asset browser panel.
     */
    void drawAssetBrowser();
    /**
     * @brief Renders the floating asset import settings window.
     */
    void drawImportSettingsWindow();
    /**
     * @brief Renders inline controls for Mesh component fields.
     */
    void drawMeshEditor();
    /**
     * @brief Renders inline controls for Grid component fields.
     */
    void drawGridEditor();
    /**
     * @brief Renders inline controls for Camera component fields.
     */
    void drawCameraEditor();
    /**
     * @brief Renders inline controls for Skeleton component fields.
     */
    void drawSkeletonEditor();
    /**
     * @brief Renders inline controls for Animator component fields.
     */
    void drawAnimatorEditor();
    /**
     * @brief Renders inline controls for Hierarchy component fields.
     */
    void drawHierarchyEditor();
    /**
     * @brief Renders inline controls for IK Solver component fields.
     */
    void drawIKSolverEditor();
    /**
     * @brief Renders inline controls for Animation Controller component fields.
     */
    void drawAnimationControllerEditor();
    /**
     * @brief Renders inline controls for RigidBody component fields.
     */
    void drawRigidBodyEditor();
    /**
     * @brief Renders inline controls for Collider component fields.
     */
    void drawColliderEditor();
    /**
     * @brief Draws a styled banner title inside component panels.
     * @param title Header title text.
     */
    void drawSectionHeader(const std::string& title);
    /**
     * @brief Renders custom controls for editing glm::vec3 variables.
     * @param label The control's label.
     * @param values Pointer to the float array of 3 values.
     * @param speed Interaction speed/step.
     * @return True if any values changed.
     */
    bool drawVec3Control(const char* label, float* values, float speed = 0.05f);
    /**
     * @brief Applies mouse visibility cursor mode according to active editor mode.
     */
    void applyInputMode();
    /**
     * @brief Renders the debug metrics and raycast state panel.
     */
    void drawDebugPanel();
    /**
     * @brief Processes and displays transformation gizmo overlay on the selected entity.
     */
    void drawGizmo();
    /**
     * @brief Helper function to extract Euler transform fields from a 4x4 matrix.
     * @param mat Input matrix.
     * @param t Output transform parameters.
     */
    void decomposeMatrixToTransform(const glm::mat4& mat, Transform& t);
    /**
     * @brief Recursively traverses parent references to build the entity's absolute world transform.
     */
    glm::mat4 getEntityWorldMatrix(Entity entity, int depth = 0);
    /**
     * @brief Draws wireframe overlays for all active colliders when enabled.
     */
    void drawColliderDebugOverlay();

    /** @brief Reference to registry. */
    Registry& registry;
    /** @brief Reference to Vulkan renderer. */
    VulkanRenderer& renderer;
    /** @brief Reference to scene manager. */
    SceneManager& sceneManager;
    /** @brief Reference to editor mode state tracker. */
    EditorModeState& editorMode;
    /** @brief Pointer to GLFW window. */
    GLFWwindow* window = nullptr;
    /** @brief ImGui dedicated descriptor pool. */
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    /** @brief State flag for ImGui initialization status. */
    bool initialized = false;
    /** @brief Target json file path for scene serialization. */
    std::string scenePath = "assets/scenes/test_scene.json";
    /** @brief Status info message printed on UI panel. */
    std::string statusMessage = "Scene not saved yet.";
    /** @brief Cache buffer string for renaming entities. */
    std::string renameBuffer;
    /** @brief The entity currently selected. */
    Entity selectedEntity{};
    /** @brief Selection flag. */
    bool hasSelection = false;
    /** @brief Mouse click state tracking. */
    bool previousLeftMouseDown = false;
    /** @brief Raycast origin caching. */
    glm::vec3 lastPickRayOrigin{ 0.0f };
    /** @brief Raycast direction caching. */
    glm::vec3 lastPickRayDirection{ 0.0f, 0.0f, -1.0f };
    /** @brief Name of the picked entity. */
    std::string lastPickNearestEntityName = "None";
    /** @brief Distance to the picked entity. */
    float lastPickNearestDistance = -1.0f;
    /** @brief Human-readable pick results. */
    std::string lastPickResult = "No pick attempted yet.";
    /** @brief Editor/Fly toggle key state cache. */
    bool previousToggleKeyDown = false;
    /** @brief Flag to toggle drawing collider wireframes in the viewport. */
    bool showColliders = false;
};
