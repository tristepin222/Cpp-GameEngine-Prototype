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
#include "ecs/components/PrimitiveType.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/components/Skeleton.hpp"
#include "ecs/components/Animator.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "ecs/components/AnimationController.hpp"
#include "ecs/components/IKSolver.hpp"
#include <functional>
#include "renderer/VulkanRenderer.hpp"
#include "scenes/Scene.hpp"
#include "scenes/SceneManager.hpp"
#include "renderer/ResourceManager.hpp"
#include <filesystem>
#include "ImGuizmo.h"

using namespace ImGui;
using namespace std;

static bool entityHasSkin(Registry& registry, Entity entity) {
    if (registry.has<SkeletonComponent>(entity)) return true;
    if (auto* hierarchy = registry.get<HierarchyComponent>(entity)) {
        if (hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent)) {
            return registry.has<SkeletonComponent>(hierarchy->parent);
        }
    }
    return false;
}

/**
 * @brief Construct a new Editor UI:: Editor UI object.
 * @param registry Reference to ECS registry.
 * @param renderer Reference to Vulkan renderer.
 * @param sceneManager Reference to scene manager.
 * @param editorMode Reference to editor mode state.
 */
EditorUI::EditorUI(Registry& registry, VulkanRenderer& renderer, SceneManager& sceneManager, EditorModeState& editorMode)
    : registry(registry), renderer(renderer), sceneManager(sceneManager), editorMode(editorMode) {
}

/**
 * @brief Destroy the Editor UI:: Editor UI object.
 */
EditorUI::~EditorUI() {
    shutdown();
}

/**
 * @brief Configures ImGui context and backends.
 * @param window Target GLFW window context.
 */
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

/**
 * @brief Disposes descriptor pool and cleans up ImGui contexts.
 */
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

/**
 * @brief Prepares a new frame for drawing GUI windows.
 */
void EditorUI::beginFrame() {
    if (!initialized) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    NewFrame();
}

/**
 * @brief Directs layout drawing of active editor panels and overlays.
 */
void EditorUI::drawPanels() {
    if (!initialized) {
        return;
    }


    drawHierarchyPanel();
    drawInspectorPanel();
    drawAssetBrowser();

    drawGizmo();
    handleViewportPicking();

    drawDebugPanel();
}

/**
 * @brief Draws Gizmo handle overlay to translate/rotate selected entities.
 */
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

/**
 * @brief Decomposes raw matrix transform parameters to target transform object.
 * @param mat Target source matrix.
 * @param t Target transform destination reference.
 */
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


/**
 * @brief Renders main level controls panel.
 */
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

/**
 * @brief Renders hierarchy list of all names active in ECS.
 */
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

    std::function<void(Entity, int)> drawEntityNode = [&](Entity entity, int depth) {
        if (depth > 10) return;
        Name* nameComp = registry.get<Name>(entity);
        if (!nameComp) return;

        bool selected = (hasSelection && entity == selectedEntity);
        
        if (depth > 0) {
            ImGui::Indent(depth * 15.0f);
        }

        std::string label = nameComp->value + "##" + std::to_string(entity.getId());
        if (Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            selectedEntity = entity;
            hasSelection = true;
            renameBuffer = nameComp->value;
        }
        
        if (depth > 0) {
            ImGui::Unindent(depth * 15.0f);
        }

        // Draw children recursively
        for (auto [childEntity, hierarchy] : registry.view<HierarchyComponent>()) {
            if (hierarchy.parent == entity) {
                drawEntityNode(childEntity, depth + 1);
            }
        }
    };

    // Draw all root entities (no HierarchyComponent, or parent is invalid)
    for (auto [entity, name] : registry.view<Name>()) {
        bool hasParent = false;
        if (auto* hierarchy = registry.get<HierarchyComponent>(entity)) {
            if (hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent)) {
                hasParent = true;
            }
        }
        
        if (!hasParent) {
            drawEntityNode(entity, 0);
        }
    }

    PopStyleVar();

    End();
}

/**
 * @brief Inspector panel routing control fields based on components.
 */
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
    drawMeshEditor();
    drawMaterialEditor();
    drawSkeletonEditor();
    drawAnimatorEditor();
    drawGridEditor();
    drawCameraEditor();



    PopStyleVar(2);

    End();
}

/**
 * @brief Renders details panel of raycasts, metrics, and picking.
 */
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

/**
 * @brief Renders inline controls for editing transform coordinates.
 */
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

/**
 * @brief Color and texture picking editor for Materials.
 */
void EditorUI::drawMaterialEditor() {
    Material* material = registry.get<Material>(selectedEntity);
    if (!material || !CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    float color[4] = { material->color.r, material->color.g, material->color.b, material->color.a };
    if (ColorEdit4("Color", color)) {
        material->color = { color[0], color[1], color[2], color[3] };
    }

    char textureBuf[256]{};
    snprintf(textureBuf, sizeof(textureBuf), "%s", material->texturePath.c_str());
    if (InputText("Texture Path", textureBuf, sizeof(textureBuf))) {
        material->texturePath = textureBuf;
        if (!material->texturePath.empty()) {
            if (auto* tex = renderer.resourceManager->loadTexture(material->texturePath, renderer)) {
                material->descriptorSet = tex->descriptorSet;
            }
        } else {
            material->descriptorSet = VK_NULL_HANDLE;
        }
    }
}

/**
 * @brief Renders inline controls for editing Mesh geometry and glTF paths.
 */
void EditorUI::drawMeshEditor() {
    Mesh* mesh = registry.get<Mesh>(selectedEntity);
    if (!mesh || registry.has<Grid>(selectedEntity) || !CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    char gltfBuf[256]{};
    snprintf(gltfBuf, sizeof(gltfBuf), "%s", mesh->gltfPath.c_str());
    if (InputText("glTF Path", gltfBuf, sizeof(gltfBuf))) {
        mesh->gltfPath = gltfBuf;
    }
    SameLine();
    if (Button("Load glTF")) {
        if (!mesh->gltfPath.empty()) {
            try {
                Mesh loaded = renderer.resourceManager->loadMesh(mesh->gltfPath, renderer);
                mesh->vertices = loaded.vertices;
                mesh->indices = loaded.indices;
                mesh->vertexBuffer = loaded.vertexBuffer;
                mesh->indexBuffer = loaded.indexBuffer;
                mesh->id = loaded.id;

                registry.remove<SkeletonComponent>(selectedEntity);
                registry.remove<AnimatorComponent>(selectedEntity);
                SkeletonComponent skeleton{};
                AnimatorComponent animator{};
                if (renderer.resourceManager->loadSkeletonAndAnimations(mesh->gltfPath, skeleton, animator)) {
                    registry.emplace<SkeletonComponent>(selectedEntity, std::move(skeleton));
                    registry.emplace<AnimatorComponent>(selectedEntity, std::move(animator));
                }

                if (auto* material = registry.get<Material>(selectedEntity)) {
                    bool hasSkin = entityHasSkin(registry, selectedEntity);
                    PipelineHandle pipeline = renderer.createPipelineForShaders(
                        hasSkin ? "build/shaders/skinned.vert.spv" : "build/shaders/unlit.vert.spv",
                        "build/shaders/unlit.frag.spv"
                    );
                    material->pipeline = pipeline.pipeline;
                    material->pipelineLayout = pipeline.layout;
                }

                statusMessage = "Loaded glTF mesh successfully.";
            } catch (const std::exception& e) {
                statusMessage = std::string("Failed to load glTF: ") + e.what();
            }
        }
    }

    Text("Vertices: %d, Indices: %d", (int)mesh->vertices.size(), (int)mesh->indices.size());
}

/**
 * @brief Renders the asset browser panel.
 */
void EditorUI::drawAssetBrowser() {
    Begin("Asset Browser");

    if (!std::filesystem::exists("assets")) {
        std::filesystem::create_directories("assets");
        std::filesystem::create_directories("assets/models");
        std::filesystem::create_directories("assets/textures");
    }

    if (TreeNodeEx("Models", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator("assets")) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".gltf" || ext == ".glb") {
                    std::string pathStr = entry.path().generic_string();
                    BulletText("%s", entry.path().filename().string().c_str());
                    if (hasSelection) {
                        SameLine();
                        PushID(pathStr.c_str());
                        if (Button("Use Model")) {
                            if (auto* mesh = registry.get<Mesh>(selectedEntity)) {
                                int primCount = renderer.resourceManager->getMeshPrimitiveCount(pathStr);
                                if (primCount > 1) {
                                    // Multi-primitive model: split into separate entities with a hierarchy
                                    std::string baseName = "Model";
                                    if (auto* nameComp = registry.get<Name>(selectedEntity)) {
                                        baseName = nameComp->value;
                                    }
                                    
                                    // 1. Assign first primitive to the selected entity (becomes root part)
                                    mesh->gltfPath = pathStr;
                                    mesh->primitiveIndex = 0;
                                    try {
                                        Mesh loaded = renderer.resourceManager->loadMesh(pathStr, renderer, 0);
                                        mesh->vertices = loaded.vertices;
                                        mesh->indices = loaded.indices;
                                        mesh->vertexBuffer = loaded.vertexBuffer;
                                        mesh->indexBuffer = loaded.indexBuffer;
                                        mesh->id = loaded.id;
                                        
                                        if (auto* nameComp = registry.get<Name>(selectedEntity)) {
                                            nameComp->value = loaded.nodeName.empty() ? (baseName + "_part0") : loaded.nodeName;
                                            renameBuffer = nameComp->value;
                                        }

                                        registry.remove<SkeletonComponent>(selectedEntity);
                                        registry.remove<AnimatorComponent>(selectedEntity);
                                        SkeletonComponent skeleton{};
                                        AnimatorComponent animator{};
                                        if (renderer.resourceManager->loadSkeletonAndAnimations(pathStr, skeleton, animator)) {
                                            registry.emplace<SkeletonComponent>(selectedEntity, std::move(skeleton));
                                            registry.emplace<AnimatorComponent>(selectedEntity, std::move(animator));
                                        }

                                        if (auto* material = registry.get<Material>(selectedEntity)) {
                                            bool hasSkin = entityHasSkin(registry, selectedEntity);
                                            PipelineHandle pipeline = renderer.createPipelineForShaders(
                                                hasSkin ? "build/shaders/skinned.vert.spv" : "build/shaders/unlit.vert.spv",
                                                "build/shaders/unlit.frag.spv"
                                            );
                                            material->pipeline = pipeline.pipeline;
                                            material->pipelineLayout = pipeline.layout;
                                        }
                                    } catch (const std::exception& e) {
                                        statusMessage = std::string("Error part 0: ") + e.what();
                                    }
                                    
                                    // 2. Spawn remaining primitives as child entities (identity local transform)
                                    Scene* currentScene = sceneManager.getCurrentScene();
                                    for (int i = 1; i < primCount; ++i) {
                                        Entity child = registry.create();
                                        if (child.getId() != Entity::INVALID_ENTITY) {
                                            // Child transforms are identity local to the parent root part
                                            registry.emplace<Transform>(child, Transform{});
                                            registry.emplace<PrimitiveType>(child, PrimitiveType{ PrimitiveKind::Cube });
                                            registry.emplace<HierarchyComponent>(child, HierarchyComponent{ selectedEntity });
                                            
                                            Mesh childMesh{};
                                            childMesh.gltfPath = pathStr;
                                            childMesh.primitiveIndex = i;
                                            try {
                                                Mesh loaded = renderer.resourceManager->loadMesh(pathStr, renderer, i);
                                                childMesh.vertices = loaded.vertices;
                                                childMesh.indices = loaded.indices;
                                                childMesh.vertexBuffer = loaded.vertexBuffer;
                                                childMesh.indexBuffer = loaded.indexBuffer;
                                                childMesh.id = loaded.id;
                                                childMesh.nodeName = loaded.nodeName;

                                                std::string childName = loaded.nodeName.empty() ? (baseName + "_part" + std::to_string(i)) : loaded.nodeName;
                                                registry.emplace<Name>(child, Name{ childName });
                                                
                                                registry.emplace<Mesh>(child, std::move(childMesh));
                                                
                                                glm::vec4 color(1.0f);
                                                Material material{ color };
                                                bool hasSkin = entityHasSkin(registry, child);
                                                PipelineHandle pipeline = renderer.createPipelineForShaders(
                                                    hasSkin ? "build/shaders/skinned.vert.spv" : "build/shaders/unlit.vert.spv",
                                                    "build/shaders/unlit.frag.spv"
                                                );
                                                material.pipeline = pipeline.pipeline;
                                                material.pipelineLayout = pipeline.layout;
                                                registry.emplace<Material>(child, std::move(material));
                                                
                                                if (currentScene) {
                                                    currentScene->trackEntity(child);
                                                }
                                            } catch (const std::exception& e) {
                                                registry.destroy(child);
                                                std::cerr << "Failed to load part " << i << ": " << e.what() << std::endl;
                                            }
                                        }
                                    }
                                    statusMessage = "Assigned glTF model: " + pathStr + " (split into " + std::to_string(primCount) + " parented parts)";
                                } else {
                                    // Single-primitive model: standard behavior
                                    mesh->gltfPath = pathStr;
                                    mesh->primitiveIndex = -1;
                                    try {
                                        Mesh loaded = renderer.resourceManager->loadMesh(pathStr, renderer);
                                        mesh->vertices = loaded.vertices;
                                        mesh->indices = loaded.indices;
                                        mesh->vertexBuffer = loaded.vertexBuffer;
                                        mesh->indexBuffer = loaded.indexBuffer;
                                        mesh->id = loaded.id;

                                        registry.remove<SkeletonComponent>(selectedEntity);
                                        registry.remove<AnimatorComponent>(selectedEntity);
                                        SkeletonComponent skeleton{};
                                        AnimatorComponent animator{};
                                        if (renderer.resourceManager->loadSkeletonAndAnimations(pathStr, skeleton, animator)) {
                                            registry.emplace<SkeletonComponent>(selectedEntity, std::move(skeleton));
                                            registry.emplace<AnimatorComponent>(selectedEntity, std::move(animator));
                                        }

                                        if (auto* material = registry.get<Material>(selectedEntity)) {
                                            bool hasSkin = entityHasSkin(registry, selectedEntity);
                                            PipelineHandle pipeline = renderer.createPipelineForShaders(
                                                hasSkin ? "build/shaders/skinned.vert.spv" : "build/shaders/unlit.vert.spv",
                                                "build/shaders/unlit.frag.spv"
                                            );
                                            material->pipeline = pipeline.pipeline;
                                            material->pipelineLayout = pipeline.layout;
                                        }

                                        statusMessage = "Assigned glTF model: " + pathStr;
                                    } catch (const std::exception& e) {
                                        statusMessage = std::string("Error: ") + e.what();
                                    }
                                }
                            }
                        }
                        PopID();
                    }
                }
            }
        }
        TreePop();
    }

    Separator();

    if (TreeNodeEx("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator("assets")) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
                    std::string pathStr = entry.path().generic_string();
                    BulletText("%s", entry.path().filename().string().c_str());
                    if (hasSelection) {
                        SameLine();
                        PushID(pathStr.c_str());
                        if (Button("Use Texture")) {
                            if (auto* material = registry.get<Material>(selectedEntity)) {
                                material->texturePath = pathStr;
                                if (auto* tex = renderer.resourceManager->loadTexture(pathStr, renderer)) {
                                    material->descriptorSet = tex->descriptorSet;
                                    statusMessage = "Assigned texture: " + pathStr;
                                }
                            }
                        }
                        PopID();
                    }
                }
            }
        }
        TreePop();
    }

    End();
}

/**
 * @brief Drag inputs for grid dimensions.
 */
void EditorUI::drawGridEditor() {
    Grid* grid = registry.get<Grid>(selectedEntity);
    if (!grid || !CollapsingHeader("Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    DragFloat("Spacing", &grid->spacing, 0.05f, 0.1f, 100.0f);
    DragFloat("Size", &grid->size, 1.0f, 1.0f, 1000.0f);
}

/**
 * @brief FOV and clipping range editor fields for camera.
 */
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

/**
 * @brief Performs picking ray-sphere checking against active entity geometries.
 */
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


/**
 * @brief Submits generated draw list command structures to Vulkan.
 * @param commandBuffer Destination command buffer.
 */
void EditorUI::render(VkCommandBuffer commandBuffer) {
    if (!initialized) {
        return;
    }

    Render();
    ImGui_ImplVulkan_RenderDrawData(GetDrawData(), commandBuffer);
}

/**
 * @brief Instantiates the ImGui dedicated descriptor pool.
 */
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

/**
 * @brief Destroys the dedicated descriptor pool.
 */
void EditorUI::destroyDescriptorPool() {
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(renderer.device.getDevice(), descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}

/**
 * @brief Spacing headers for component subsections.
 * @param title Header title.
 */
void EditorUI::drawSectionHeader(const std::string& title) {
    Spacing();
    TextUnformatted(title.c_str());
    Separator();
}

/**
 * @brief Multi-column float drag fields helper.
 * @param label Control label.
 * @param values Control value array pointer.
 * @param speed Adjustment sensitivity.
 * @return True if changed, false otherwise.
 */
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

/**
 * @brief Controls mouse cursor lock according to flying states.
 */
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

void EditorUI::drawSkeletonEditor() {
    SkeletonComponent* skeleton = registry.get<SkeletonComponent>(selectedEntity);
    if (!skeleton || !CollapsingHeader("Skeleton", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    Text("Bones count: %d", (int)skeleton->joints.size());
    if (TreeNode("Bones List")) {
        for (size_t i = 0; i < skeleton->joints.size(); ++i) {
            const auto& joint = skeleton->joints[i];
            BulletText("[%d] %s (Parent: %d)", (int)i, joint.name.c_str(), joint.parentIndex);
        }
        TreePop();
    }
}

void EditorUI::drawAnimatorEditor() {
    if (auto* hierarchy = registry.get<HierarchyComponent>(selectedEntity)) {
        if (hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent) && registry.has<AnimatorComponent>(hierarchy->parent)) {
            if (CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen)) {
                TextUnformatted("Animation is driven by parent entity.");
            }
            return;
        }
    }

    AnimatorComponent* animator = registry.get<AnimatorComponent>(selectedEntity);
    if (!animator || !CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    if (animator->animations.empty()) {
        TextUnformatted("No animation clips loaded.");
        return;
    }
    
    std::vector<const char*> clipNames;
    for (const auto& anim : animator->animations) {
        clipNames.push_back(anim.name.c_str());
    }
    
    int currentClipIdx = animator->activeAnimationIndex;
    if (Combo("Active Animation", &currentClipIdx, clipNames.data(), static_cast<int>(clipNames.size()))) {
        animator->activeAnimationIndex = currentClipIdx;
        animator->currentTime = 0.0f;
    }
    
    SliderFloat("Playback Speed", &animator->playbackSpeed, 0.0f, 5.0f, "%.2fx");
    Checkbox("Looping", &animator->loop);
    
    if (currentClipIdx >= 0 && currentClipIdx < static_cast<int>(animator->animations.size())) {
        const auto& activeClip = animator->animations[currentClipIdx];
        float progress = activeClip.duration > 0.0f ? (animator->currentTime / activeClip.duration) : 0.0f;
        ProgressBar(progress, ImVec2(-1, 0), (std::to_string(animator->currentTime) + "s / " + std::to_string(activeClip.duration) + "s").c_str());
        
        if (Button("Play")) {
            animator->playbackSpeed = 1.0f;
        }
        SameLine();
        if (Button("Pause")) {
            animator->playbackSpeed = 0.0f;
        }
        SameLine();
        if (Button("Reset")) {
            animator->currentTime = 0.0f;
        }
    }

    // --- Animation Controller Section ---
    Separator();
    if (auto* controller = registry.get<AnimationControllerComponent>(selectedEntity)) {
        if (CollapsingHeader("Animation Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
            Text("Current State: %s", controller->currentState.empty() ? "None" : controller->currentState.c_str());
            if (controller->isCrossfading) {
                Text("Crossfading to %s (%.2f / %.2fs)", controller->targetState.c_str(), controller->crossfadeProgress, controller->crossfadeDuration);
                ProgressBar(controller->crossfadeProgress / controller->crossfadeDuration);
            }

            // Parameters management
            if (TreeNode("Parameters")) {
                for (auto& [paramName, paramVal] : controller->parameters) {
                    SliderFloat(paramName.c_str(), &paramVal, 0.0f, 1.0f);
                }
                TreePop();
            }

            // Quick State Setup Demo buttons
            if (Button("Setup Idle/Walk State Machine")) {
                controller->states.clear();
                
                AnimationState idleState;
                idleState.name = "Idle";
                idleState.clipName = "idle";
                idleState.isBlendTree = false;
                if (!animator->animations.empty()) idleState.clipName = animator->animations[0].name;
                
                AnimationState moveState;
                moveState.name = "Movement";
                moveState.isBlendTree = true;
                moveState.blendTree.parameterName = "speed";
                
                if (animator->animations.size() >= 2) {
                    BlendNode nodeWalk{ animator->animations[0].name, 0.0f };
                    BlendNode nodeRun{ animator->animations[1].name, 1.0f };
                    moveState.blendTree.nodes = { nodeWalk, nodeRun };
                } else if (!animator->animations.empty()) {
                    BlendNode nodeWalk{ animator->animations[0].name, 0.0f };
                    moveState.blendTree.nodes = { nodeWalk };
                }
                
                controller->states = { idleState, moveState };

                controller->transitions.clear();
                
                AnimationTransition toMove;
                toMove.fromState = "Idle";
                toMove.toState = "Movement";
                toMove.crossfadeDuration = 0.3f;
                TransitionCondition condMove{ "speed", ">", 0.1f };
                toMove.conditions = { condMove };
                
                AnimationTransition toIdle;
                toIdle.fromState = "Movement";
                toIdle.toState = "Idle";
                toIdle.crossfadeDuration = 0.3f;
                TransitionCondition condIdle2{ "speed", "<", 0.1f };
                toIdle.conditions = { condIdle2 };
                
                controller->transitions = { toMove, toIdle };
                controller->parameters["speed"] = 0.0f;
                controller->currentState = "Idle";
                controller->currentStateTime = 0.0f;
                controller->isCrossfading = false;
            }
        }
    } else {
        if (Button("Create Animation Controller")) {
            registry.emplace<AnimationControllerComponent>(selectedEntity, AnimationControllerComponent{});
        }
    }

    // --- IK Solver Section ---
    Separator();
    if (auto* ik = registry.get<IKSolverComponent>(selectedEntity)) {
        if (CollapsingHeader("IK Solver", ImGuiTreeNodeFlags_DefaultOpen)) {
            Checkbox("Enable IK Solver", &ik->enabled);
            
            const char* solverTypes[] = { "2-Bone (Analytical)", "FABRIK (Iterative)" };
            int currentType = static_cast<int>(ik->solverType);
            if (Combo("Solver Type", &currentType, solverTypes, 2)) {
                ik->solverType = static_cast<IKSolverType>(currentType);
            }

            if (ik->solverType == IKSolverType::TwoBone) {
                char startJointBuf[64]{};
                char midJointBuf[64]{};
                char endJointBuf[64]{};
                strncpy_s(startJointBuf, ik->startJointName.c_str(), sizeof(startJointBuf) - 1);
                strncpy_s(midJointBuf, ik->middleJointName.c_str(), sizeof(midJointBuf) - 1);
                strncpy_s(endJointBuf, ik->endJointName.c_str(), sizeof(endJointBuf) - 1);
                
                if (InputText("Start Joint (e.g. thigh)", startJointBuf, sizeof(startJointBuf))) {
                    ik->startJointName = startJointBuf;
                }
                if (InputText("Middle Joint (e.g. shin)", midJointBuf, sizeof(midJointBuf))) {
                    ik->middleJointName = midJointBuf;
                }
                if (InputText("End Joint (e.g. foot)", endJointBuf, sizeof(endJointBuf))) {
                    ik->endJointName = endJointBuf;
                }
                
                if (Button("Auto Setup Left Leg Joints")) {
                    if (auto* skeleton = registry.get<SkeletonComponent>(selectedEntity)) {
                        for (const auto& joint : skeleton->joints) {
                            if (joint.name.find("thigh.L") != std::string::npos || joint.name.find("Thigh.L") != std::string::npos || joint.name.find("UpperLeg_L") != std::string::npos) {
                                ik->startJointName = joint.name;
                            }
                            if (joint.name.find("shin.L") != std::string::npos || joint.name.find("Shin.L") != std::string::npos || joint.name.find("LowerLeg_L") != std::string::npos) {
                                ik->middleJointName = joint.name;
                            }
                            if (joint.name.find("foot.L") != std::string::npos || joint.name.find("Foot.L") != std::string::npos || joint.name.find("Foot_L") != std::string::npos) {
                                ik->endJointName = joint.name;
                            }
                        }
                        ik->polePosition = glm::vec3(0.0f, 0.0f, 1.0f);
                    }
                }
            } else {
                SliderInt("Max Iterations", &ik->maxIterations, 1, 50);
                SliderFloat("Tolerance", &ik->tolerance, 0.0001f, 0.01f, "%.4f");
                
                TextUnformatted("Bone Chain Joints (Base to Tip):");
                for (size_t i = 0; i < ik->jointChainNames.size(); ++i) {
                    char jointBuf[64]{};
                    strncpy_s(jointBuf, ik->jointChainNames[i].c_str(), sizeof(jointBuf) - 1);
                    PushID(static_cast<int>(i));
                    if (InputText("##joint", jointBuf, sizeof(jointBuf))) {
                        ik->jointChainNames[i] = jointBuf;
                    }
                    SameLine();
                    if (Button("Remove")) {
                        ik->jointChainNames.erase(ik->jointChainNames.begin() + i);
                        PopID();
                        break;
                    }
                    PopID();
                }
                if (Button("Add Bone to Chain")) {
                    ik->jointChainNames.push_back("");
                }
                SameLine();
                if (Button("Auto Setup Left Arm Chain")) {
                    if (auto* skeleton = registry.get<SkeletonComponent>(selectedEntity)) {
                        ik->jointChainNames.clear();
                        for (const auto& joint : skeleton->joints) {
                            if (joint.name.find("shoulder.L") != std::string::npos || joint.name.find("Shoulder.L") != std::string::npos || joint.name.find("Clavicle_L") != std::string::npos) {
                                ik->jointChainNames.push_back(joint.name);
                            }
                        }
                        for (const auto& joint : skeleton->joints) {
                            if (joint.name.find("upper_arm.L") != std::string::npos || joint.name.find("UpperArm.L") != std::string::npos || joint.name.find("UpperArm_L") != std::string::npos) {
                                ik->jointChainNames.push_back(joint.name);
                            }
                        }
                        for (const auto& joint : skeleton->joints) {
                            if (joint.name.find("forearm.L") != std::string::npos || joint.name.find("Forearm.L") != std::string::npos || joint.name.find("Forearm_L") != std::string::npos) {
                                ik->jointChainNames.push_back(joint.name);
                            }
                        }
                        for (const auto& joint : skeleton->joints) {
                            if (joint.name.find("hand.L") != std::string::npos || joint.name.find("Hand.L") != std::string::npos || joint.name.find("Hand_L") != std::string::npos) {
                                ik->jointChainNames.push_back(joint.name);
                            }
                        }
                    }
                }
            }

            DragFloat3("IK Target Position", &ik->targetPosition.x, 0.05f);
            DragFloat3("IK Pole Position", &ik->polePosition.x, 0.05f);
            SliderFloat("IK Target Weight", &ik->targetWeight, 0.0f, 1.0f);
        }
    } else {
        if (Button("Add IK Solver Component")) {
            registry.emplace<IKSolverComponent>(selectedEntity, IKSolverComponent{});
        }
    }
}
