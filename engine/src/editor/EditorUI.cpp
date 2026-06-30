#include "editor/EditorUI.hpp"

#include <limits>
#include <stdexcept>
#include <string>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include "ecs/Registry.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Mesh.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Grid.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "scenes/Scene.hpp"
#include "scenes/SceneManager.hpp"
#include "ImGuizmo.h"

using namespace ImGui;
using namespace std;

EditorUI::EditorUI(Registry& registry, VulkanRenderer& renderer, SceneManager& sceneManager, EditorModeState& editorMode)
    : registry(registry), renderer(renderer), sceneManager(sceneManager), editorMode(editorMode) {
}

EditorUI::~EditorUI() {
    shutdown();
}

void EditorUI::initialize(GLFWwindow* window) {
    if (initialized) {
        return;
    }

    this->window = window;

    IMGUI_CHECKVERSION();
    CreateContext();
    StyleColorsDark();

    createDescriptorPool();

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        throw runtime_error("Failed to initialize ImGui GLFW backend");
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
        throw runtime_error("Failed to initialize ImGui Vulkan backend");
    }

    applyInputMode();
    initialized = true;
}

void EditorUI::shutdown() {
    if (!initialized) {
        return;
    }

    vkDeviceWaitIdle(renderer.device.getDevice());
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    DestroyContext();
    destroyDescriptorPool();
    initialized = false;
}

void EditorUI::beginFrame() {
    if (!initialized) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    NewFrame();
}

void EditorUI::drawPanels() {
    if (!initialized) {
        return;
    }


    drawHierarchyPanel();
    drawInspectorPanel();

    drawGizmo();
    handleViewportPicking();

    drawDebugPanel();
}

void EditorUI::drawGizmo()
{
    if (!hasSelection)
        return;

    Transform* transform = registry.get<Transform>(selectedEntity);
    if (!transform)
        return;

    ImGuiIO& io = ImGui::GetIO();

    ImGuizmo::BeginFrame();
    ImGuizmo::Enable(true);
    ImGuizmo::SetOrthographic(false);

    // draw directly to foreground drawlist
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

    ImGuizmo::SetRect(
        0.0f,
        0.0f,
        io.DisplaySize.x,
        io.DisplaySize.y
    );

    glm::mat4 view = renderer.getActiveCameraView();
    glm::mat4 proj = renderer.getActiveCameraProjection();

    proj[1][1] *= -1.0f; // Vulkan

    glm::mat4 model = transform->matrix();

    ImGuizmo::Manipulate(
        &view[0][0],
        &proj[0][0],
        ImGuizmo::TRANSLATE,
        ImGuizmo::LOCAL,
        &model[0][0]
    );

    if (ImGuizmo::IsUsing())
        decomposeMatrixToTransform(model, *transform);
}

void EditorUI::decomposeMatrixToTransform(const glm::mat4& mat, Transform& t)
{
    float matrixTranslation[3];
    float matrixRotation[3];
    float matrixScale[3];

    ImGuizmo::DecomposeMatrixToComponents(
        &mat[0][0],
        matrixTranslation,
        matrixRotation,
        matrixScale
    );

    t.position = glm::vec3(
        matrixTranslation[0],
        matrixTranslation[1],
        matrixTranslation[2]
    );

    t.rotation = glm::vec3(
        matrixRotation[0],
        matrixRotation[1],
        matrixRotation[2]
    );

    t.scale = glm::vec3(
        matrixScale[0],
        matrixScale[1],
        matrixScale[2]
    );
}


void EditorUI::drawSceneControls() {
    if (Button(editorMode.flyMode ? "Switch To Edit Mode" : "Switch To Fly Mode")) {
        editorMode.flyMode = !editorMode.flyMode;
        applyInputMode();
        statusMessage = editorMode.flyMode ? "Fly mode enabled." : "Edit mode enabled.";
    }
    SameLine();
    TextUnformatted(editorMode.flyMode ? "Camera controls active" : "Editor controls active");

    char pathBuffer[260]{};
    scenePath.copy(pathBuffer, scenePath.size(), 0);
    pathBuffer[scenePath.size()] = '\0';
    if (InputText("Scene Path", pathBuffer, sizeof(pathBuffer))) {
        scenePath = pathBuffer;
    }

    Scene* currentScene = sceneManager.getCurrentScene();
    if (Button("Save Scene")) {
        if (currentScene && currentScene->saveToFile(scenePath)) {
            statusMessage = "Scene saved to " + scenePath;
        } else {
            statusMessage = "Failed to save scene.";
        }
    }
    SameLine();
    if (Button("Load Scene")) {
        if (currentScene && currentScene->loadFromFile(scenePath)) {
            statusMessage = "Scene loaded from " + scenePath;
            hasSelection = false;
            selectedEntity = Entity();
            renameBuffer.clear();
        } else {
            statusMessage = "Failed to load scene.";
        }
    }
    TextWrapped("%s", statusMessage.c_str());
}

void EditorUI::drawHierarchyPanel() {
    Begin("Hierarchy");
    TextUnformatted("Scene Entities");
    Separator();

    Scene* currentScene = sceneManager.getCurrentScene();
    if (Button("Add Cube")) {
        if (currentScene) {
            Entity created = currentScene->createPrimitiveEntity("Cube");
            if (created.getId() != Entity::INVALID_ENTITY) {
                selectedEntity = created;
                hasSelection = true;
                if (Name* createdName = registry.get<Name>(created)) {
                    renameBuffer = createdName->value;
                }
                statusMessage = "Created cube entity.";
            } else {
                statusMessage = "Failed to create cube entity.";
            }
        }
    }
    SameLine();
    if (Button("Add Triangle")) {
        if (currentScene) {
            Entity created = currentScene->createPrimitiveEntity("Triangle");
            if (created.getId() != Entity::INVALID_ENTITY) {
                selectedEntity = created;
                hasSelection = true;
                if (Name* createdName = registry.get<Name>(created)) {
                    renameBuffer = createdName->value;
                }
                statusMessage = "Created triangle entity.";
            } else {
                statusMessage = "Failed to create triangle entity.";
            }
        }
    }
    SameLine();
    if (Button("Add Camera")) {
        if (currentScene) {
            Entity created = currentScene->createEntityOfType("Camera");
            if (created.getId() != Entity::INVALID_ENTITY) {
                selectedEntity = created;
                hasSelection = true;
                if (Name* createdName = registry.get<Name>(created)) {
                    renameBuffer = createdName->value;
                }
                statusMessage = "Created camera entity.";
            } else {
                statusMessage = "Failed to create camera entity.";
            }
        }
    }
    SameLine();
    if (Button("Add Grid")) {
        if (currentScene) {
            Entity created = currentScene->createEntityOfType("Grid");
            if (created.getId() != Entity::INVALID_ENTITY) {
                selectedEntity = created;
                hasSelection = true;
                if (Name* createdName = registry.get<Name>(created)) {
                    renameBuffer = createdName->value;
                }
                statusMessage = "Created grid entity.";
            } else {
                statusMessage = "Failed to create grid entity.";
            }
        }
    }
    SameLine();
    bool canDuplicate = hasSelection && currentScene != nullptr;
    BeginDisabled(!canDuplicate);
    if (Button("Duplicate Selected")) {
        Entity duplicated = currentScene->duplicateEntity(selectedEntity);
        if (duplicated.getId() != Entity::INVALID_ENTITY) {
            selectedEntity = duplicated;
            hasSelection = true;
            if (Name* duplicatedName = registry.get<Name>(duplicated)) {
                renameBuffer = duplicatedName->value;
            }
            statusMessage = "Duplicated selected entity.";
        } else {
            statusMessage = "Failed to duplicate selected entity.";
        }
    }
    EndDisabled();
    SameLine();
    bool canDelete = hasSelection && currentScene != nullptr;
    BeginDisabled(!canDelete);
    if (Button("Delete Selected")) {
        if (currentScene->deleteEntity(selectedEntity)) {
            statusMessage = "Deleted selected entity.";
            hasSelection = false;
            selectedEntity = Entity();
            renameBuffer.clear();
        } else {
            statusMessage = "Failed to delete selected entity.";
        }
    }
    EndDisabled();

    Separator();

    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

    for (auto [entity, name] : registry.view<Name>()) {
        bool selected = (hasSelection && entity == selectedEntity);

        if (Selectable(name.value.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            selectedEntity = entity;
            hasSelection = true;
            renameBuffer = name.value;
        }
    }

    PopStyleVar();

    End();
}

void EditorUI::drawInspectorPanel() {
    Begin("Inspector");
    TextUnformatted("Runtime ECS Editor");
    drawSceneControls();

    if (!hasSelection) {
        Separator();
        TextUnformatted("Select an entity in the hierarchy.");
        End();
        return;
    }

    Name* name = registry.get<Name>(selectedEntity);
    if (!name) {
        hasSelection = false;
        Separator();
        TextUnformatted("Selection is no longer valid.");
        End();
        return;
    }

    Separator();
    Text("Selected: %s", name->value.c_str());

    char renameBufferChars[128]{};
    renameBuffer.copy(renameBufferChars, std::min(renameBuffer.size(), sizeof(renameBufferChars) - 1), 0);
    if (InputText("Name", renameBufferChars, sizeof(renameBufferChars))) {
        renameBuffer = renameBufferChars;
    }
    SameLine();
    if (Button("Rename Selected")) {
        if (!renameBuffer.empty()) {
            name->value = renameBuffer;
            statusMessage = "Renamed selected entity.";
        } else {
            statusMessage = "Name cannot be empty.";
        }
    }

    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

    drawSectionHeader(name->value.c_str());

    drawTransformEditor();
    drawMaterialEditor();
    drawGridEditor();
    drawCameraEditor();



    PopStyleVar(2);

    End();
}

void EditorUI::drawDebugPanel() {
    Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    TextUnformatted("Picking Debug");
    Separator();

    Text("Result: %s", lastPickResult.c_str());
    Text("Nearest Candidate: %s", lastPickNearestEntityName.c_str());

    if (lastPickNearestDistance >= 0.0f) {
        Text("Nearest Distance: %.3f", lastPickNearestDistance);
    }
    else {
        TextUnformatted("Nearest Distance: none");
    }

    Spacing();

    TextUnformatted("Ray Origin:");
    Text("(%.2f, %.2f, %.2f)",
        lastPickRayOrigin.x,
        lastPickRayOrigin.y,
        lastPickRayOrigin.z);

    TextUnformatted("Ray Direction:");
    Text("(%.2f, %.2f, %.2f)",
        lastPickRayDirection.x,
        lastPickRayDirection.y,
        lastPickRayDirection.z);

    Spacing();
    Separator();
    TextUnformatted("Clip Depth Mode: OpenGL-style (-1..1)");

    End();
}

void EditorUI::drawTransformEditor() {
    Transform* transform = registry.get<Transform>(selectedEntity);
    if (!transform || !CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    float position[3] =
    {
        transform->position.x,
        transform->position.y,
        transform->position.z
    };

    if (drawVec3Control("Position", position))
    {
        transform->position =
        {
            position[0],
            position[1],
            position[2]
        };
    }

    float rotation[3] =
    {
        transform->rotation.x,
        transform->rotation.y,
        transform->rotation.z
    };

    if (drawVec3Control("Rotation", rotation, 0.5f))
    {
        transform->rotation =
        {
            rotation[0],
            rotation[1],
            rotation[2]
        };
    }

    float scale[3] =
    {
        transform->scale.x,
        transform->scale.y,
        transform->scale.z
    };

    if (drawVec3Control("Scale", scale, 0.05f))
    {
        transform->scale =
        {
            scale[0],
            scale[1],
            scale[2]
        };
    }

    if (Camera* camera = registry.get<Camera>(selectedEntity)) {
        renderer.setActiveCamera(camera->projection(), transform->position, camera->view(*transform));
    }
}

void EditorUI::drawMaterialEditor() {
    Material* material = registry.get<Material>(selectedEntity);
    if (!material || !CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    float color[4] = { material->color.r, material->color.g, material->color.b, material->color.a };
    if (ColorEdit4("Color", color)) {
        material->color = { color[0], color[1], color[2], color[3] };
    }
}

void EditorUI::drawGridEditor() {
    Grid* grid = registry.get<Grid>(selectedEntity);
    if (!grid || !CollapsingHeader("Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    DragFloat("Spacing", &grid->spacing, 0.05f, 0.1f, 100.0f);
    DragFloat("Size", &grid->size, 1.0f, 1.0f, 1000.0f);
}

void EditorUI::drawCameraEditor() {
    Camera* camera = registry.get<Camera>(selectedEntity);
    Transform* transform = registry.get<Transform>(selectedEntity);
    if (!camera || !CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    bool changed = false;
    changed |= DragFloat("FOV", &camera->fov, 0.1f, 1.0f, 120.0f);
    changed |= DragFloat("Near Plane", &camera->nearPlane, 0.01f, 0.01f, 10.0f);
    changed |= DragFloat("Far Plane", &camera->farPlane, 1.0f, 1.0f, 5000.0f);
    changed |= DragFloat("Move Speed", &camera->moveSpeed, 0.1f, 0.1f, 100.0f);
    changed |= DragFloat("Mouse Sensitivity", &camera->mouseSensitivity, 0.01f, 0.01f, 5.0f);

    if (transform) {
        Text("Aspect: %.3f", camera->aspect);
        if (changed) {
            renderer.setActiveCamera(camera->projection(), transform->position, camera->view(*transform));
        }
    }
}

void EditorUI::handleViewportPicking() {

    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())
        return;


    if (!window || editorMode.flyMode) {
        previousLeftMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        return;
    }

    const bool leftMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool clickStarted = leftMouseDown && !previousLeftMouseDown;
    previousLeftMouseDown = leftMouseDown;

    if (!clickStarted)
        return;

    if (GetIO().WantCaptureMouse)
        return;


    if (!renderer.hasActiveCamera()) {
        statusMessage = "No active camera available for picking.";
        lastPickResult = statusMessage;
        lastPickNearestEntityName = "None";
        lastPickNearestDistance = -1.0f;
        return;
    }

    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);
    if (width <= 0 || height <= 0) {
        lastPickResult = "Viewport size is invalid for picking.";
        lastPickNearestEntityName = "None";
        lastPickNearestDistance = -1.0f;
        return;
    }

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    const float normalizedX = static_cast<float>((2.0 * mouseX) / static_cast<double>(width) - 1.0);
    const float normalizedY = static_cast<float>(1.0 - (2.0 * mouseY) / static_cast<double>(height));

    const glm::mat4 inverseViewProjection = glm::inverse(renderer.getActiveCameraViewProj());
    const glm::vec4 nearClip = inverseViewProjection * glm::vec4(normalizedX, normalizedY, -1.0f, 1.0f);
    const glm::vec4 farClip = inverseViewProjection * glm::vec4(normalizedX, normalizedY, 1.0f, 1.0f);
    if (glm::abs(nearClip.w) < 0.0001f || glm::abs(farClip.w) < 0.0001f) {
        statusMessage = "Viewport picking could not unproject the mouse ray.";
        lastPickResult = statusMessage;
        lastPickNearestEntityName = "None";
        lastPickNearestDistance = -1.0f;
        return;
    }

    const glm::vec3 nearPoint = glm::vec3(nearClip) / nearClip.w;
    const glm::vec3 farPoint = glm::vec3(farClip) / farClip.w;
    const glm::vec3 rayDirection = glm::normalize(farPoint - nearPoint);
    const glm::vec3 rayOrigin = nearPoint;
    lastPickRayOrigin = rayOrigin;
    lastPickRayDirection = rayDirection;

    Entity hitEntity{};
    Name* hitName = nullptr;
    float nearestHitDistance = std::numeric_limits<float>::max();
    lastPickNearestEntityName = "None";
    lastPickNearestDistance = -1.0f;

    for (auto [entity, name, transform, mesh] : registry.view<Name, Transform, Mesh>()) {
        if (mesh.vertices.empty() || registry.has<Grid>(entity)) {
            continue;
        }

        glm::vec3 worldMin(std::numeric_limits<float>::max());
        glm::vec3 worldMax(std::numeric_limits<float>::lowest());
        const glm::mat4 model = transform.matrix();

        for (const Vertex& vertex : mesh.vertices) {
            const glm::vec3 worldPosition = glm::vec3(model * glm::vec4(vertex.position, 1.0f));
            worldMin = glm::min(worldMin, worldPosition);
            worldMax = glm::max(worldMax, worldPosition);
        }

        // --- Bounding sphere from AABB ---
        glm::vec3 center = (worldMin + worldMax) * 0.5f;
        float radius = glm::length(worldMax - center);
        float distanceToCamera = glm::length(center - rayOrigin);
        radius += distanceToCamera * 0.06f;
        radius *= 1.6f;

        // --- Ray-sphere intersection ---
        glm::vec3 oc = rayOrigin - center;

        float a = glm::dot(rayDirection, rayDirection);
        float b = 2.0f * glm::dot(oc, rayDirection);
        float c = glm::dot(oc, oc) - radius * radius;

        float discriminant = b * b - 4.0f * a * c;

        if (discriminant < 0.0f) {
            continue;
        }

        // nearest intersection
        float sqrtD = sqrt(discriminant);
        float t0 = (-b - sqrtD) / (2.0f * a);
        float t1 = (-b + sqrtD) / (2.0f * a);

        // pick closest valid hit
        float hitDistance = (t0 > 0.0f) ? t0 : t1;

        if (hitDistance > 0.0f && hitDistance < nearestHitDistance) {
            nearestHitDistance = hitDistance;
            hitEntity = entity;
            hitName = &name;
            lastPickNearestEntityName = name.value;
            lastPickNearestDistance = hitDistance;
        }
    }

    if (hitName) {
        selectedEntity = hitEntity;
        hasSelection = true;
        renameBuffer = hitName->value;
        statusMessage = "Selected " + hitName->value + " from viewport.";
        lastPickResult = statusMessage;
        return;
    }

    hasSelection = false;
    selectedEntity = Entity();
    renameBuffer.clear();
    statusMessage = "Viewport selection cleared.";
    lastPickResult = statusMessage;
}


void EditorUI::render(VkCommandBuffer commandBuffer) {
    if (!initialized) {
        return;
    }

    Render();
    ImGui_ImplVulkan_RenderDrawData(GetDrawData(), commandBuffer);
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
    poolInfo.maxSets = 1000 * static_cast<uint32_t>(size(poolSizes));
    poolInfo.poolSizeCount = static_cast<uint32_t>(size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(renderer.device.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw runtime_error("Failed to create ImGui descriptor pool");
    }
}

void EditorUI::destroyDescriptorPool() {
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(renderer.device.getDevice(), descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}

void EditorUI::drawSectionHeader(const std::string& title) {
    Spacing();
    TextUnformatted(title.c_str());
    Separator();
}

bool EditorUI::drawVec3Control(const char* label, float* values, float speed) {

    bool changed = false;

    PushID(label);

    Columns(2, nullptr, false);
    SetColumnWidth(0, 80.0f);

    TextUnformatted(label);
    NextColumn();

    float width = CalcItemWidth();
    float itemWidth = width / 3.0f - 4.0f;

    PushItemWidth(itemWidth);

    for (int i = 0; i < 3; i++)
    {
        PushID(i);

        changed |= DragFloat("##v", &values[i], speed);

        PopID();

        if (i < 2)
            SameLine();
    }

    PopItemWidth();
    Columns(1);
    PopID();

    return changed;
}

void EditorUI::applyInputMode() {
    if (!window) {
        return;
    }

    if (editorMode.flyMode) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}
