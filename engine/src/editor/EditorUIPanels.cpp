#include "editor/EditorUI.hpp"
#include "editor/EditorUIInternal.hpp"
#include "editor/NodeGraphFramework.hpp"
#include "meta/ComponentReflection.hpp"
#include "scenes/JSONUtils.hpp"
#include "editor/AssetBrowserRegistry.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "scenes/Scene.hpp"
#include "scenes/SceneManager.hpp"
#include "scenes/SceneSerializer.hpp"
#include "renderer/ResourceManager.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Mesh.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/components/Skeleton.hpp"
#include "ecs/components/Animator.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "ecs/components/EditorCamera.hpp"
#include "ecs/components/AnimationController.hpp"
#include "ecs/components/IKSolver.hpp"
#include "ecs/components/Collider.hpp"
#include "ecs/components/Tilemap.hpp"
#include "ecs/components/UIComponents.hpp"
#include "ecs/components/PhysgunScript.hpp"
#include "ufbx.h"
#include "cgltf.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>

using namespace ImGui;
using namespace std;

static void drawRegisteredAssetBrowserMenu(const std::filesystem::path& folderPath) {
    for (const auto& opt : Engine::AssetBrowserRegistry::getOptions()) {
        size_t slashPos = opt.labelPath.find('/');
        if (slashPos != std::string::npos) {
            std::string menuName = opt.labelPath.substr(0, slashPos);
            std::string itemName = opt.labelPath.substr(slashPos + 1);
            if (ImGui::BeginMenu(menuName.c_str())) {
                if (ImGui::MenuItem(itemName.c_str())) {
                    opt.callback(folderPath);
                }
                ImGui::EndMenu();
            }
        } else {
            if (ImGui::MenuItem(opt.labelPath.c_str())) {
                opt.callback(folderPath);
            }
        }
    }
}

void EditorUI::drawPanels() {
    if (!initialized) {
        return;
    }

    // Safety check: clear selection if the selected entity was destroyed/invalidated
    if (hasSelection && (!registry.isValid(selectedEntity) || selectedEntity.getId() == Entity::INVALID_ENTITY)) {
        selectedEntity = Entity();
        hasSelection = false;
        renameBuffer.clear();
    }

    ImGuiIO& io = ImGui::GetIO();
    float width = io.DisplaySize.x;
    float height = io.DisplaySize.y;

    // 1. Top Menu Bar (Main Menu Bar)
    float topY = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        topY = ImGui::GetWindowSize().y; // dynamic height of the menu bar
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (Scene* currentScene = sceneManager.getCurrentScene()) {
                    currentScene->saveToFile("sandbox_game/assets/scenes/test_scene.json");
                    statusMessage = "Scene saved successfully.";
                }
            }
            if (ImGui::MenuItem("Load Scene", "Ctrl+L")) {
                if (Scene* currentScene = sceneManager.getCurrentScene()) {
                    SceneSerializer serializer(registry, renderer);
                    std::vector<Entity> loadedEntities;
                    if (serializer.deserialize("sandbox_game/assets/scenes/test_scene.json", loadedEntities)) {
                        statusMessage = "Scene loaded successfully.";
                    } else {
                        statusMessage = "Failed to load scene.";
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Settings", "Ctrl+Shift+B")) {
                showBuildSettings = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            if (ImGui::MenuItem("Tileset Editor")) {
                s_openTilesetEditorWindow = true;
            }
            if (ImGui::MenuItem("Animation Editor")) {
                s_openAnimationEditorWindow = true;
            }
            if (ImGui::MenuItem("Node Graph Demo")) {
                s_openNodeGraphDemoWindow = true;
            }
            ImGui::EndMenu();
        }
        // Center-aligned Play / Stop buttons in the Main Menu Bar
        float menuBarWidth = ImGui::GetWindowWidth();
        float buttonGroupWidth = 80.0f; // estimated width
        ImGui::SameLine(menuBarWidth * 0.5f - buttonGroupWidth * 0.5f);
        
        if (!editorMode.isPlaying) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.48f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.65f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.35f, 0.08f, 1.0f));
            if (ImGui::Button("PLAY", ImVec2(80, 0))) {
                editorMode.pendingPlay = true;
                statusMessage = "Entering Play Mode...";
            }
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.68f, 0.12f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.18f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.08f, 0.08f, 1.0f));
            if (ImGui::Button("STOP", ImVec2(80, 0))) {
                editorMode.pendingStop = true;
                statusMessage = "Stopping simulation...";
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::EndMainMenuBar();
    }

    // Fallback if MainMenuBar is not showing
    if (topY == 0.0f) {
        topY = 22.0f; 
    }

    float workHeight = height - topY;

    // Sidebar dimensions (snapped layout)
    float leftWidth = glm::clamp(width * 0.20f, 260.0f, 400.0f);
    float rightWidth = glm::clamp(width * 0.22f, 320.0f, 460.0f);
    float centerWidth = width - leftWidth - rightWidth;
    float bottomHeight = workHeight * 0.32f;
    float topPanelHeight = workHeight - bottomHeight;

    // 2. Hierarchy Panel (Left - Top)
    ImGui::SetNextWindowPos(ImVec2(0.0f, topY));
    ImGui::SetNextWindowSize(ImVec2(leftWidth, topPanelHeight));
    drawHierarchyPanel();

    // 3. Debug Panel (Left - Bottom)
    ImGui::SetNextWindowPos(ImVec2(0.0f, topY + topPanelHeight));
    ImGui::SetNextWindowSize(ImVec2(leftWidth, bottomHeight));
    drawDebugPanel();

    // 4. Asset Browser (Center - Bottom)
    ImGui::SetNextWindowPos(ImVec2(leftWidth, topY + topPanelHeight));
    ImGui::SetNextWindowSize(ImVec2(centerWidth, bottomHeight));
    drawAssetBrowser();

    // 5. Inspector Panel (Right)
    ImGui::SetNextWindowPos(ImVec2(width - rightWidth, topY));
    ImGui::SetNextWindowSize(ImVec2(rightWidth, workHeight));
    drawInspectorPanel();

    // 6. Draw Gizmo and Viewport overlay controls (drawn on top of clear center area)
    drawGizmo();
    drawColliderDebugOverlay();
    drawPhysgunDebugOverlay();
    drawTilemapGridOverlay();
    handleViewportPicking();
    
    // 7. Float Import Settings panel
    drawImportSettingsWindow();
    
    // 7b. Floating Tileset Editor window
    drawTilesetEditorWindow();

    // 7c. Floating Animation Editor window
    drawAnimationEditorWindow();

    // 7d. Floating Node Graph Demo window
    drawNodeGraphDemoWindow();

    // 8. Build Settings panel (floating modal)
    if (showBuildSettings) {
        drawBuildSettingsPanel();
    }
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
    Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    Scene* currentScene = sceneManager.getCurrentScene();

    // ---- Top Toolbar ----
    float panelWidth = ImGui::GetContentRegionAvail().x;

    // [+ Create] button as a popup
    if (ImGui::Button("+ Create", ImVec2(panelWidth * 0.48f, 0))) {
        ImGui::OpenPopup("CreateEntityPopup");
    }

    if (ImGui::BeginPopup("CreateEntityPopup")) {
        ImGui::TextDisabled("Primitives");
        ImGui::Separator();
        if (ImGui::MenuItem("Cube"))          { if (currentScene) { auto e = currentScene->createPrimitiveEntity("Cube");     selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
        if (ImGui::MenuItem("Triangle"))      { if (currentScene) { auto e = currentScene->createPrimitiveEntity("Triangle"); selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
        ImGui::Separator();
        ImGui::TextDisabled("Entities");
        ImGui::Separator();
        if (ImGui::MenuItem("Camera"))        { if (currentScene) { auto e = currentScene->createEntityOfType("Camera"); selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
        if (ImGui::MenuItem("Grid"))          { if (currentScene) { auto e = currentScene->createEntityOfType("Grid");   selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
        if (ImGui::MenuItem("Empty GameObject")) { if (currentScene) { auto e = currentScene->createEntityOfType("Empty");  selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
        ImGui::Separator();
        if (ImGui::BeginMenu("UI")) {
            if (ImGui::MenuItem("Canvas")) { if (currentScene) { auto e = currentScene->createEntityOfType("Canvas"); selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
            if (ImGui::MenuItem("Panel"))  { if (currentScene) { auto e = currentScene->createEntityOfType("UI Panel"); selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
            if (ImGui::MenuItem("Image"))  { if (currentScene) { auto e = currentScene->createEntityOfType("UI Image"); selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
            if (ImGui::MenuItem("Text"))   { if (currentScene) { auto e = currentScene->createEntityOfType("UI Text"); selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
            if (ImGui::MenuItem("Button")) { if (currentScene) { auto e = currentScene->createEntityOfType("UI Button"); selectedEntity = e; hasSelection = true; if (auto* n = registry.get<Name>(e)) renameBuffer = n->value; } }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // [Duplicate] button — only enabled when something is selected
    BeginDisabled(!hasSelection || !currentScene);
    if (ImGui::Button("Duplicate", ImVec2(panelWidth * 0.48f, 0))) {
        Entity duplicated = currentScene->duplicateEntity(selectedEntity);
        if (duplicated.getId() != Entity::INVALID_ENTITY) {
            selectedEntity = duplicated;
            hasSelection = true;
            if (auto* n = registry.get<Name>(duplicated)) renameBuffer = n->value;
            statusMessage = "Duplicated selected entity.";
        }
    }
    EndDisabled();

    Separator();

    // ---- Entity Tree ----
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 3));

    // Scrollable child window for the entity tree list
    // Height is negative to leave space for the bottom delete button footer (approx 42px)
    ImGui::BeginChild("HierarchyTreeChild", ImVec2(0, -42.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Entity to delete deferred (can't destroy during iteration)
    Entity pendingDelete;
    bool hasPendingDelete = false;

    std::function<void(Entity, int)> drawEntityNode = [&](Entity entity, int depth) {
        if (depth > 10) return;
        if (registry.has<EditorCamera>(entity)) return;
        Name* nameComp = registry.get<Name>(entity);
        if (!nameComp) return;

        bool selected = (hasSelection && entity == selectedEntity);

        if (depth > 0) ImGui::Indent(depth * 16.0f);

        // Highlight selected row with accent color
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.00f, 0.40f, 0.70f, 0.60f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.00f, 0.48f, 0.80f, 0.80f));
        }

        std::string label = nameComp->value + "##" + std::to_string(entity.getId());
        if (Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            selectedEntity = entity;
            hasSelection = true;
            renameBuffer = nameComp->value;
        }

        // Drag source for hierarchy moving/rearranging
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            std::uint32_t entId = entity.getId();
            ImGui::SetDragDropPayload("DND_PAYLOAD_HIERARCHY_ENTITY", &entId, sizeof(entId));
            ImGui::Text("Move: %s", nameComp->value.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target for parenting
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_PAYLOAD_HIERARCHY_ENTITY")) {
                std::uint32_t draggedId = *static_cast<const std::uint32_t*>(payload->Data);
                Entity draggedEntity(draggedId);
                
                // Avoid parenting an entity to itself, or to any of its descendants (cycles)
                bool isSelfOrDescendant = (draggedEntity == entity);
                Entity check = entity;
                while (check.getId() != Entity::INVALID_ENTITY && registry.isValid(check)) {
                    if (auto* checkHierarchy = registry.get<HierarchyComponent>(check)) {
                        if (checkHierarchy->parent == draggedEntity) {
                            isSelfOrDescendant = true;
                            break;
                        }
                        check = checkHierarchy->parent;
                    } else {
                        break;
                    }
                }
                
                if (!isSelfOrDescendant) {
                    if (auto* hc = registry.get<HierarchyComponent>(draggedEntity)) {
                        hc->parent = entity;
                    } else {
                        registry.emplace<HierarchyComponent>(draggedEntity, HierarchyComponent{ entity });
                    }
                    statusMessage = "Parented entity under " + nameComp->value;
                } else {
                    statusMessage = "Cannot parent an entity to itself or its descendants!";
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (selected) ImGui::PopStyleColor(2);

        // Right-click context menu on any entity node
        std::string ctxId = "EntityCtx##" + std::to_string(entity.getId());
        if (ImGui::BeginPopupContextItem(ctxId.c_str())) {
            selectedEntity = entity;
            hasSelection = true;
            renameBuffer = nameComp->value;

            ImGui::TextDisabled("%s", nameComp->value.c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Duplicate")) {
                if (currentScene) {
                    Entity dup = currentScene->duplicateEntity(entity);
                    if (dup.getId() != Entity::INVALID_ENTITY) {
                        selectedEntity = dup;
                        hasSelection = true;
                        if (auto* n = registry.get<Name>(dup)) renameBuffer = n->value;
                        statusMessage = "Duplicated entity.";
                    }
                }
            }

            if (ImGui::MenuItem("Create Empty Child")) {
                if (currentScene) {
                    Entity child = registry.create();
                    registry.emplace<Name>(child, Name{ currentScene->makeUniqueEntityName("Empty GameObject") });
                    registry.emplace<Transform>(child, Transform{ glm::vec3(0.0f) });
                    registry.emplace<HierarchyComponent>(child, HierarchyComponent{ entity });
                    currentScene->trackEntity(child);
                    selectedEntity = child;
                    hasSelection = true;
                    renameBuffer = "Empty GameObject";
                    statusMessage = "Created empty child object.";
                }
            }

            if (auto* hc = registry.get<HierarchyComponent>(entity)) {
                if (hc->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hc->parent)) {
                    if (ImGui::MenuItem("Unparent (Make Root)")) {
                        hc->parent = Entity();
                        statusMessage = "Unparented entity to root.";
                    }
                }
            }

            if (hasSelection && selectedEntity != entity) {
                bool isDescendant = false;
                Entity check = entity;
                while (check.getId() != Entity::INVALID_ENTITY && registry.isValid(check)) {
                    if (auto* checkHierarchy = registry.get<HierarchyComponent>(check)) {
                        if (checkHierarchy->parent == selectedEntity) {
                            isDescendant = true;
                            break;
                        }
                        check = checkHierarchy->parent;
                    } else {
                        break;
                    }
                }
                if (!isDescendant) {
                    if (ImGui::MenuItem("Parent Selected to This")) {
                        if (auto* hc = registry.get<HierarchyComponent>(selectedEntity)) {
                            hc->parent = entity;
                        } else {
                            registry.emplace<HierarchyComponent>(selectedEntity, HierarchyComponent{ entity });
                        }
                        statusMessage = "Parented selected entity.";
                    }
                }
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
            if (ImGui::MenuItem("Delete")) {
                pendingDelete = entity;
                hasPendingDelete = true;
            }
            ImGui::PopStyleColor();

            ImGui::EndPopup();
        }

        if (depth > 0) ImGui::Unindent(depth * 16.0f);

        // Draw children recursively
        for (auto [childEntity, hierarchy] : registry.view<HierarchyComponent>()) {
            if (hierarchy.parent == entity) {
                drawEntityNode(childEntity, depth + 1);
            }
        }
    };

    // Draw all root entities (those with no valid parent)
    for (auto [entity, name] : registry.view<Name>()) {
        if (registry.has<EditorCamera>(entity)) continue;
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

    // Drop target on empty space of child window to unparent to root
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_PAYLOAD_HIERARCHY_ENTITY")) {
            std::uint32_t draggedId = *static_cast<const std::uint32_t*>(payload->Data);
            Entity draggedEntity(draggedId);
            if (auto* hc = registry.get<HierarchyComponent>(draggedEntity)) {
                hc->parent = Entity();
                statusMessage = "Unparented entity to root.";
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild(); // End of HierarchyTreeChild scrolling area

    PopStyleVar();

    // ---- Delete button (bottom of panel, red, always visible) ----
    Separator();

    bool canDelete = hasSelection && currentScene != nullptr;
    BeginDisabled(!canDelete);
    ImGui::PushStyleColor(ImGuiCol_Button,        canDelete ? ImVec4(0.65f, 0.10f, 0.10f, 1.0f) : ImVec4(0.30f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.50f, 0.08f, 0.08f, 1.0f));
    if (ImGui::Button("Delete Selected", ImVec2(-1, 0))) {
        if (currentScene->deleteEntity(selectedEntity)) {
            statusMessage = "Deleted selected entity.";
            hasSelection = false;
            selectedEntity = Entity();
            renameBuffer.clear();
        }
    }
    ImGui::PopStyleColor(3);
    EndDisabled();

    // Process deferred deletion from context menu
    if (hasPendingDelete && currentScene) {
        if (currentScene->deleteEntity(pendingDelete)) {
            statusMessage = "Deleted entity.";
            if (selectedEntity == pendingDelete) {
                hasSelection = false;
                selectedEntity = Entity();
                renameBuffer.clear();
            }
        }
    }

    End();
}

void EditorUI::drawInspectorPanel() {
    Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
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
    SameLine();
    if (Button("Save as Prefab")) {
        std::string prefabDir = "assets/prefabs";
        if (!std::filesystem::exists(prefabDir)) {
            std::filesystem::create_directories(prefabDir);
        }
        std::string prefabPath = prefabDir + "/" + name->value + ".prefab";
        SceneSerializer serializer(registry, renderer);
        if (serializer.serializePrefab(prefabPath, selectedEntity)) {
            statusMessage = "Saved prefab to " + prefabPath;
        } else {
            statusMessage = "Failed to save prefab.";
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
    drawHierarchyEditor();
    drawIKSolverEditor();
    drawAnimationControllerEditor();
    drawReflectedComponentsEditor();
    drawColliderEditor();
    drawTilemapInspector();
    drawUIComponentsEditor();
    drawGridEditor();
    drawCameraEditor();

    // Render dynamic plugin component editors
    for (auto& [compName, callback] : getDynamicInspectors()) {
        callback(registry, selectedEntity);
    }

    ImGui::Separator();
    if (ImGui::Button("+ Add Component", ImVec2(-1, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!registry.has<Material>(selectedEntity) && ImGui::MenuItem("Material")) {
            glm::vec4 color(1.0f);
            registry.emplace<Material>(selectedEntity, Material{ color });
            if (auto* material = registry.get<Material>(selectedEntity)) {
                bool hasSkin = entityHasSkin(registry, selectedEntity);
                std::string vertShader = hasSkin ? "skinned.vert.spv" : "unlit.vert.spv";
                std::string fragShader = "unlit.frag.spv";
                PipelineHandle pipeline = renderer.createPipelineForShaders(
                    renderer.resolveShaderPath("build/shaders/" + vertShader),
                    renderer.resolveShaderPath("build/shaders/" + fragShader)
                );
                material->pipeline = pipeline.pipeline;
                material->pipelineLayout = pipeline.layout;
                renderer.resourceManager->updateMaterialDescriptorSet(*material, renderer);
            }
            statusMessage = "Added Material component.";
        }
        if (!registry.has<Camera>(selectedEntity) && ImGui::MenuItem("Camera")) {
            registry.emplace<Camera>(selectedEntity, Camera{});
            statusMessage = "Added Camera component.";
        }
        if (!registry.has<Grid>(selectedEntity) && ImGui::MenuItem("Grid")) {
            registry.emplace<Grid>(selectedEntity, Grid{});
            statusMessage = "Added Grid component.";
        }
        if (!registry.has<SkeletonComponent>(selectedEntity) && ImGui::MenuItem("Skeleton")) {
            registry.emplace<SkeletonComponent>(selectedEntity, SkeletonComponent{});
            if (auto* material = registry.get<Material>(selectedEntity)) {
                std::string vertShader = (material->shaderName == "Lit") ? "skinned_lit.vert.spv" : "skinned.vert.spv";
                std::string fragShader = (material->shaderName == "Lit") ? "lit.frag.spv" : "unlit.frag.spv";
                PipelineHandle pipeline = renderer.createPipelineForShaders(
                    renderer.resolveShaderPath("build/shaders/" + vertShader),
                    renderer.resolveShaderPath("build/shaders/" + fragShader)
                );
                material->pipeline = pipeline.pipeline;
                material->pipelineLayout = pipeline.layout;
            }
            statusMessage = "Added Skeleton Component.";
        }
        if (!registry.has<AnimatorComponent>(selectedEntity) && ImGui::MenuItem("Animator")) {
            registry.emplace<AnimatorComponent>(selectedEntity, AnimatorComponent{});
            statusMessage = "Added Animator component.";
        }
        if (!registry.has<AnimationControllerComponent>(selectedEntity) && ImGui::MenuItem("Animation Controller")) {
            registry.emplace<AnimationControllerComponent>(selectedEntity, AnimationControllerComponent{});
            statusMessage = "Added Animation Controller component.";
        }
        if (!registry.has<IKSolverComponent>(selectedEntity) && ImGui::MenuItem("IK Solver")) {
            registry.emplace<IKSolverComponent>(selectedEntity, IKSolverComponent{});
            statusMessage = "Added IK Solver component.";
        }
        if (!registry.has<HierarchyComponent>(selectedEntity) && ImGui::MenuItem("Hierarchy Link")) {
            registry.emplace<HierarchyComponent>(selectedEntity, HierarchyComponent{});
            statusMessage = "Added Hierarchy Component.";
        }
        if (!registry.has<ColliderComponent>(selectedEntity) && ImGui::MenuItem("Collider")) {
            registry.emplace<ColliderComponent>(selectedEntity, ColliderComponent{});
            statusMessage = "Added Collider component.";
        }
        if (!registry.has<Engine::TilemapComponent>(selectedEntity) && ImGui::MenuItem("Tilemap")) {
            Engine::TilemapComponent tm{};
            tm.width = 10; tm.height = 10; tm.tileSize = 1.f;
            tm.tiles.assign(100, -1);
            registry.emplace<Engine::TilemapComponent>(selectedEntity, std::move(tm));
            statusMessage = "Added Tilemap component.";
        }
        if (!registry.has<Engine::CanvasComponent>(selectedEntity) && ImGui::MenuItem("UI Canvas")) {
            registry.emplace<Engine::CanvasComponent>(selectedEntity, Engine::CanvasComponent{});
            statusMessage = "Added Canvas component.";
        }
        if (!registry.has<Engine::RectTransform>(selectedEntity) && ImGui::MenuItem("UI RectTransform")) {
            registry.emplace<Engine::RectTransform>(selectedEntity, Engine::RectTransform{});
            statusMessage = "Added RectTransform component.";
        }
        if (!registry.has<Engine::UIPanelComponent>(selectedEntity) && ImGui::MenuItem("UI Panel")) {
            registry.emplace<Engine::UIPanelComponent>(selectedEntity, Engine::UIPanelComponent{});
            statusMessage = "Added Panel component.";
        }
        if (!registry.has<Engine::UIImageComponent>(selectedEntity) && ImGui::MenuItem("UI Image")) {
            registry.emplace<Engine::UIImageComponent>(selectedEntity, Engine::UIImageComponent{});
            statusMessage = "Added Image component.";
        }
        if (!registry.has<Engine::UITextComponent>(selectedEntity) && ImGui::MenuItem("UI Text")) {
            registry.emplace<Engine::UITextComponent>(selectedEntity, Engine::UITextComponent{});
            statusMessage = "Added Text component.";
        }
        if (!registry.has<Engine::UIButtonComponent>(selectedEntity) && ImGui::MenuItem("UI Button")) {
            registry.emplace<Engine::UIButtonComponent>(selectedEntity, Engine::UIButtonComponent{});
            statusMessage = "Added Button component.";
        }

        // Render reflected components dynamically (skip those with dedicated hardcoded menu items)
        for (const auto& refl : Engine::ComponentReflectionRegistry::getInstance().getReflections()) {
            if (!refl.has(registry, selectedEntity)) {
                if (refl.name == "Tilemap") continue; // handled by the hardcoded entry above
                std::string menuName = refl.name;
                if (menuName == "PlayerController") menuName = "Player Controller";
                if (ImGui::MenuItem(menuName.c_str())) {
                    refl.add(registry, selectedEntity);
                    statusMessage = "Added " + refl.name + " component.";
                }
            }
        }

        // Render dynamic plugin component add options
        for (auto& [compName, callback] : getDynamicAddCallbacks()) {
            if (ImGui::MenuItem(compName.c_str())) {
                callback(registry, selectedEntity);
                statusMessage = "Added " + compName + " component.";
            }
        }

        ImGui::EndPopup();
    }

    PopStyleVar(2);

    End();
}

void EditorUI::drawDebugPanel() {
    Begin("Debug", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
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

    Spacing();
    Separator();
    TextUnformatted("Physgun System Debug");
    Separator();

    bool hasPhysgun = false;
    for (auto [ent, script] : registry.view<PhysgunScript>()) {
        hasPhysgun = true;        
        Text("Entity ID: %d", ent.getId());
        if (script.isHolding) {
            Text("Held Entity ID: %d", script.heldEntity.getId());
            Text("Current Hold Distance: %.2f", script.currentHoldDistance);
        }
        Text("Script Ray Origin: (%.2f, %.2f, %.2f)", script.rayOrigin.x, script.rayOrigin.y, script.rayOrigin.z);
        Text("Script Ray Direction: (%.2f, %.2f, %.2f)", script.rayDirection.x, script.rayDirection.y, script.rayDirection.z);
        Text("Script Update Count: %d", script.updateCount);
        Text("Debug Show Ray: %s (Press R to toggle)", script.debugShowRay ? "ON" : "OFF");
        Text("Kp (Stiffness): %.1f", script.Kp);
        Text("Kd (Damping): %.1f", script.Kd);
        Text("Default Hold Dist: %.1f", script.holdDistance);
    }
    if (!hasPhysgun) {
        TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No active PhysgunScript found in scene.");
    }

    Spacing();
    Separator();
    TextUnformatted("Gameplay Camera Matrices from Renderer:");
    Text("Det(VP): %.4f", glm::determinant(renderer.getGameplayCameraViewProj()));
    Text("Pos: (%.2f, %.2f, %.2f)", 
         renderer.getGameplayCameraPosition().x, 
         renderer.getGameplayCameraPosition().y, 
         renderer.getGameplayCameraPosition().z);

    Spacing();
    Separator();
    Checkbox("Show Colliders", &showColliders);

    End();
}

void EditorUI::drawAssetBrowser() {
    Begin("Asset Browser", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (!std::filesystem::exists("assets")) {
        std::filesystem::create_directories("assets");
        std::filesystem::create_directories("assets/models");
        std::filesystem::create_directories("assets/textures");
        std::filesystem::create_directories("assets/prefabs");
    }

    // Static variables to maintain state for rename and create popups between frames
    static std::filesystem::path s_renameTargetPath;
    static char s_renameBuffer[256] = "";
    static std::filesystem::path s_createFolderParentPath;
    static char s_createFolderBuffer[256] = "";
    static std::filesystem::path s_createSceneParentPath;
    static char s_createSceneBuffer[256] = "";
    static std::filesystem::path s_createFileParentPath;
    static char s_createFileBuffer[256] = "";
    static bool s_openCreateFolderPopup = false;
    static bool s_openCreateScenePopup = false;
    static bool s_openCreateFilePopup = false;
    static bool s_openRenamePopup = false;

    // ---- Toolbar ----
    if (Button("Refresh")) {
        statusMessage = "Refreshed asset directories.";
    }
    Separator();

    // ---- Recursive Directory Tree drawing lambda ----
    std::function<void(const std::filesystem::path&)> drawDirectoryNode = [&](const std::filesystem::path& dirPath) {
        if (!std::filesystem::exists(dirPath)) return;

        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            std::string name = entry.path().filename().string();
            // Skip hidden items
            if (name.empty() || name[0] == '.') {
                continue;
            }

            std::string pathStr = entry.path().generic_string();

            if (entry.is_directory()) {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
                std::string label = "[] " + name + "##" + pathStr;
                bool open = TreeNodeEx(label.c_str(), flags);

                if (BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    SetDragDropPayload("DND_PAYLOAD_ASSET_PATH", pathStr.c_str(), pathStr.size() + 1);
                    Text("Dragging folder %s", name.c_str());
                    EndDragDropSource();
                }

                if (BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = AcceptDragDropPayload("DND_PAYLOAD_ASSET_PATH")) {
                        const char* srcPath = (const char*)payload->Data;
                        std::filesystem::path src(srcPath);
                        std::filesystem::path dest = entry.path() / src.filename();
                        
                        std::string srcStr = src.generic_string();
                        std::string destStr = dest.generic_string();
                        if (srcStr == destStr || destStr.rfind(srcStr + "/", 0) == 0) {
                            statusMessage = "Cannot move a folder into itself or its subfolder.";
                        } else {
                            try {
                                std::filesystem::rename(src, dest);
                                statusMessage = "Moved " + src.filename().string() + " to " + entry.path().filename().string();
                            } catch (const std::exception& e) {
                                statusMessage = std::string("Failed to move: ") + e.what();
                            }
                        }
                    }
                    EndDragDropTarget();
                }

                // Right click context menu on folders
                if (BeginPopupContextItem(pathStr.c_str())) {
                    TextDisabled("Folder: %s", name.c_str());
                    Separator();
                    if (BeginMenu("Create")) {
                        if (MenuItem("Folder")) {
                            s_createFolderParentPath = entry.path();
                            s_createFolderBuffer[0] = '\0';
                            s_openCreateFolderPopup = true;
                        }
                        if (MenuItem("Scene")) {
                            s_createSceneParentPath = entry.path();
                            s_createSceneBuffer[0] = '\0';
                            s_openCreateScenePopup = true;
                        }
                        if (MenuItem("File")) {
                            s_createFileParentPath = entry.path();
                            s_createFileBuffer[0] = '\0';
                            s_openCreateFilePopup = true;
                        }
                        if (MenuItem("Animation File (.anim)")) {
                            s_createFileParentPath = entry.path();
                            strcpy_s(s_createFileBuffer, "new_animation.anim");
                            s_openCreateFilePopup = true;
                        }

                        // Custom options registered to the asset browser menu
                        drawRegisteredAssetBrowserMenu(entry.path());

                        EndMenu();
                    }
                    if (MenuItem("Rename")) {
                        s_renameTargetPath = entry.path();
                        strncpy_s(s_renameBuffer, name.c_str(), sizeof(s_renameBuffer) - 1);
                        s_openRenamePopup = true;
                    }
                    PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
                    if (MenuItem("Delete Folder")) {
                        try {
                            std::filesystem::path activePath = entry.path();
                            std::filesystem::path sourcePath = std::filesystem::path("../../../sandbox_game") / activePath;
                            std::filesystem::remove_all(activePath);
                            if (std::filesystem::exists(sourcePath)) {
                                std::filesystem::remove_all(sourcePath);
                            }
                            statusMessage = "Deleted folder: " + name;
                        } catch (const std::exception& e) {
                            statusMessage = std::string("Failed to delete folder: ") + e.what();
                        }
                    }
                    PopStyleColor();
                    EndPopup();
                }

                if (open) {
                    drawDirectoryNode(entry.path());
                    TreePop();
                }
            } else if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                bool isModel = (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".FBX");
                bool isTexture = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga");
                bool isPrefab = (ext == ".prefab");
                bool isScene = (ext == ".json");
                bool isTileset = (ext == ".tileset");
                bool isTile    = (ext == ".tile");

                std::string prefix = "  ";
                if (isModel)   prefix = "[] ";
                else if (isTexture) prefix = "[] ";
                else if (isPrefab)  prefix = "[] ";
                else if (isScene)   prefix = "[] ";
                else if (isTileset) prefix = "[] ";
                else if (isTile)    prefix = "[] ";

                std::string labelStr = prefix + name + "##" + pathStr;
                Selectable(labelStr.c_str(), false, ImGuiSelectableFlags_AllowOverlap);

                // Right click context menu on files (must follow Selectable immediately to bind correctly)
                if (BeginPopupContextItem(pathStr.c_str())) {
                    TextDisabled("File: %s", name.c_str());
                    Separator();
                    if (isModel || isTexture) {
                        if (MenuItem("Import Settings...")) {
                            s_importSettingsAssetPath = entry.path();
                            s_triggerLoadImportSettings = true;
                        }
                        Separator();
                    }
                    if (isModel && MenuItem("Load Mesh to Selected")) {
                        if (hasSelection && registry.isValid(selectedEntity)) {
                            try {
                                int primCount = renderer.resourceManager->getMeshPrimitiveCount(pathStr);
                                auto emplaceOrReplaceMesh = [&](Entity ent, Mesh&& mesh) {
                                    if (registry.has<Mesh>(ent)) {
                                        registry.getRef<Mesh>(ent) = std::move(mesh);
                                    } else {
                                        registry.emplace<Mesh>(ent, std::move(mesh));
                                    }
                                };
                                if (primCount > 1) {
                                    for (int i = 0; i < primCount; ++i) {
                                        Entity subEntity = selectedEntity;
                                        if (i > 0) {
                                            subEntity = registry.create();
                                            registry.emplace<Name>(subEntity, Name{ name + "_primitive_" + std::to_string(i) });
                                            registry.emplace<Transform>(subEntity, Transform{ glm::vec3(0.f) });
                                            registry.emplace<HierarchyComponent>(subEntity, HierarchyComponent{ selectedEntity });
                                        }
                                        Mesh loadedMesh = renderer.resourceManager->loadMesh(pathStr, renderer, i);
                                        emplaceOrReplaceMesh(subEntity, std::move(loadedMesh));
                                    }
                                } else {
                                    Mesh loadedMesh = renderer.resourceManager->loadMesh(pathStr, renderer);
                                    emplaceOrReplaceMesh(selectedEntity, std::move(loadedMesh));
                                }

                                if (auto* material = registry.get<Material>(selectedEntity)) {
                                    bool hasSkin = entityHasSkin(registry, selectedEntity);
                                    PipelineHandle pipeline = renderer.createPipelineForShaders(
                                        hasSkin ? renderer.resolveShaderPath("build/shaders/skinned.vert.spv") : renderer.resolveShaderPath("build/shaders/unlit.vert.spv"),
                                        renderer.resolveShaderPath("build/shaders/unlit.frag.spv")
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
                    if (isTexture && MenuItem("Apply Texture to Selected Material")) {
                        if (hasSelection && registry.isValid(selectedEntity)) {
                            if (auto* material = registry.get<Material>(selectedEntity)) {
                                material->texturePath = pathStr;
                                renderer.resourceManager->updateMaterialDescriptorSet(*material, renderer);
                                statusMessage = "Applied texture to selected entity's material.";
                            } else {
                                statusMessage = "Selected entity has no Material component.";
                            }
                        } else {
                            statusMessage = "No entity selected.";
                        }
                    }
                    if (isPrefab && MenuItem("Instantiate Prefab")) {
                        SceneSerializer serializer(registry, renderer);
                        std::vector<Entity> loadedEntities;
                        Entity instantiated = serializer.deserializePrefab(pathStr, loadedEntities);
                        if (instantiated.getId() != Entity::INVALID_ENTITY) {
                            selectedEntity = instantiated;
                            hasSelection = true;
                            if (auto* n = registry.get<Name>(instantiated)) renameBuffer = n->value;
                            statusMessage = "Instantiated prefab.";
                        } else {
                            statusMessage = "Failed to instantiate prefab.";
                        }
                    }
                    if (isScene && MenuItem("Load Scene")) {
                        if (Scene* currentScene = sceneManager.getCurrentScene()) {
                            if (currentScene->loadFromFile(pathStr)) {
                                statusMessage = "Loaded scene " + name;
                                hasSelection = false;
                                selectedEntity = Entity();
                                renameBuffer.clear();
                            } else {
                                statusMessage = "Failed to load scene.";
                            }
                        }
                    }
                    if (MenuItem("Rename")) {
                        s_renameTargetPath = entry.path();
                        strncpy_s(s_renameBuffer, name.c_str(), sizeof(s_renameBuffer) - 1);
                        s_openRenamePopup = true;
                    }
                    PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
                    if (MenuItem("Delete File")) {
                        try {
                            std::filesystem::path activePath = entry.path();
                            std::filesystem::path sourcePath = std::filesystem::path("../../../sandbox_game") / activePath;
                            std::filesystem::remove(activePath);
                            if (std::filesystem::exists(sourcePath)) {
                                std::filesystem::remove(sourcePath);
                            }
                            statusMessage = "Deleted file: " + name;
                        } catch (const std::exception& e) {
                            statusMessage = std::string("Failed to delete file: ") + e.what();
                        }
                    }
                    PopStyleColor();
                    EndPopup();
                }

                if (BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    SetDragDropPayload("DND_PAYLOAD_ASSET_PATH", pathStr.c_str(), pathStr.size() + 1);
                    Text("Dragging %s", name.c_str());
                    EndDragDropSource();
                }
            }
        }
    };

    // Draw active running assets folder
    ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
    std::string rootLabel = "[] assets##assets_root";
    if (TreeNodeEx(rootLabel.c_str(), rootFlags)) {
        if (BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = AcceptDragDropPayload("DND_PAYLOAD_ASSET_PATH")) {
                const char* srcPath = (const char*)payload->Data;
                std::filesystem::path src(srcPath);
                std::filesystem::path dest = std::filesystem::path("assets") / src.filename();
                
                std::string srcStr = src.generic_string();
                std::string destStr = dest.generic_string();
                if (srcStr != destStr && destStr.rfind(srcStr + "/", 0) != 0) {
                    try {
                        std::filesystem::rename(src, dest);
                        statusMessage = "Moved " + src.filename().string() + " to assets root.";
                    } catch (const std::exception& e) {
                        statusMessage = std::string("Failed to move: ") + e.what();
                    }
                }
            }
            EndDragDropTarget();
        }

        // Right click context menu on assets root node
        if (BeginPopupContextItem("assets_root_ctx")) {
            if (BeginMenu("Create")) {
                if (MenuItem("Folder")) {
                    s_createFolderParentPath = "assets";
                    s_createFolderBuffer[0] = '\0';
                    s_openCreateFolderPopup = true;
                }
                if (MenuItem("Scene")) {
                    s_createSceneParentPath = "assets";
                    s_createSceneBuffer[0] = '\0';
                    s_openCreateScenePopup = true;
                }
                if (MenuItem("File")) {
                    s_createFileParentPath = "assets";
                    s_createFileBuffer[0] = '\0';
                    s_openCreateFilePopup = true;
                }
                if (MenuItem("Animation File (.anim)")) {
                    s_createFileParentPath = "assets";
                    strcpy_s(s_createFileBuffer, "new_animation.anim");
                    s_openCreateFilePopup = true;
                }
                drawRegisteredAssetBrowserMenu("assets");
                EndMenu();
            }
            EndPopup();
        }

        drawDirectoryNode("assets");
        TreePop();
    }

    // ---- Popups for File Creation and Renaming ----

    if (s_openCreateFolderPopup) {
        OpenPopup("Create Folder");
        s_openCreateFolderPopup = false;
    }
    if (BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Text("Folder Name:");
        InputText("##foldername", s_createFolderBuffer, sizeof(s_createFolderBuffer));
        if (Button("Create", ImVec2(120, 0))) {
            if (s_createFolderBuffer[0] != '\0') {
                std::filesystem::path newActive = s_createFolderParentPath / s_createFolderBuffer;
                std::filesystem::path newSource = std::filesystem::path("../../../sandbox_game") / newActive;
                std::filesystem::create_directories(newActive);
                std::filesystem::create_directories(newSource);
                statusMessage = "Created folder: " + std::string(s_createFolderBuffer);
            }
            CloseCurrentPopup();
        }
        SameLine();
        if (Button("Cancel", ImVec2(120, 0))) {
            CloseCurrentPopup();
        }
        EndPopup();
    }

    if (s_openCreateScenePopup) {
        OpenPopup("Create Scene");
        s_openCreateScenePopup = false;
    }
    if (BeginPopupModal("Create Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Text("Scene Name:");
        InputText("##scenename", s_createSceneBuffer, sizeof(s_createSceneBuffer));
        if (Button("Create", ImVec2(120, 0))) {
            if (s_createSceneBuffer[0] != '\0') {
                std::string sname = s_createSceneBuffer;
                if (sname.rfind(".json") == std::string::npos) sname += ".json";
                std::filesystem::path newActive = s_createSceneParentPath / sname;
                std::filesystem::path newSource = std::filesystem::path("../../../sandbox_game") / newActive;
                std::ofstream fActive(newActive);
                if (fActive.is_open()) { fActive << "[]"; fActive.close(); }
                std::ofstream fSource(newSource);
                if (fSource.is_open()) { fSource << "[]"; fSource.close(); }
                statusMessage = "Created scene: " + sname;
            }
            CloseCurrentPopup();
        }
        SameLine();
        if (Button("Cancel", ImVec2(120, 0))) {
            CloseCurrentPopup();
        }
        EndPopup();
    }

    if (s_openCreateFilePopup) {
        OpenPopup("Create File");
        s_openCreateFilePopup = false;
    }
    if (BeginPopupModal("Create File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Text("File Name (including extension):");
        InputText("##filename", s_createFileBuffer, sizeof(s_createFileBuffer));
        if (Button("Create", ImVec2(120, 0))) {
            if (s_createFileBuffer[0] != '\0') {
                std::filesystem::path newActive = s_createFileParentPath / s_createFileBuffer;
                std::filesystem::path newSource = std::filesystem::path("../../../sandbox_game") / newActive;
                auto writeNewFile = [](const std::filesystem::path& p) {
                    std::ofstream f(p, std::ios::binary);
                    if (f.is_open()) {
                        if (p.extension().string() == ".anim") {
                            char magic[4] = {'A', 'N', 'I', 'M'};
                            f.write(magic, 4);
                            uint32_t version = 2;
                            f.write(reinterpret_cast<const char*>(&version), sizeof(version));
                            uint32_t jointCount = 0;
                            f.write(reinterpret_cast<const char*>(&jointCount), sizeof(jointCount));
                            uint32_t animCount = 0;
                            f.write(reinterpret_cast<const char*>(&animCount), sizeof(animCount));
                        }
                        f.close();
                    }
                };
                writeNewFile(newActive);
                writeNewFile(newSource);
                if (newActive.extension().string() == ".anim") {
                    statusMessage = "Created animation file: " + std::string(s_createFileBuffer);
                } else {
                    statusMessage = "Created empty file: " + std::string(s_createFileBuffer);
                }
            }
            CloseCurrentPopup();
        }
        SameLine();
        if (Button("Cancel", ImVec2(120, 0))) {
            CloseCurrentPopup();
        }
        EndPopup();
    }

    if (s_openRenamePopup) {
        OpenPopup("Rename Asset");
        s_openRenamePopup = false;
    }
    if (BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Text("New Name:");
        InputText("##renamebuf", s_renameBuffer, sizeof(s_renameBuffer));
        if (Button("Rename", ImVec2(120, 0))) {
            if (s_renameBuffer[0] != '\0' && !s_renameTargetPath.empty()) {
                try {
                    std::filesystem::path parent = s_renameTargetPath.parent_path();
                    std::filesystem::path newActive = parent / s_renameBuffer;
                    std::filesystem::path newSource = std::filesystem::path("../../../sandbox_game") / newActive;
                    std::filesystem::path oldSource = std::filesystem::path("../../../sandbox_game") / s_renameTargetPath;

                    std::filesystem::rename(s_renameTargetPath, newActive);
                    if (std::filesystem::exists(oldSource)) {
                        std::filesystem::rename(oldSource, newSource);
                    }
                    statusMessage = "Renamed " + s_renameTargetPath.filename().string() + " to " + s_renameBuffer;
                } catch (const std::exception& e) {
                    statusMessage = std::string("Rename failed: ") + e.what();
                }
            }
            CloseCurrentPopup();
        }
        SameLine();
        if (Button("Cancel", ImVec2(120, 0))) {
            CloseCurrentPopup();
        }
        EndPopup();
    }

    End();
}

void EditorUI::drawImportSettingsWindow() {
    if (s_triggerLoadImportSettings) {
        loadImportSettingsMetadata(s_importSettingsAssetPath);
        s_openImportSettingsWindow = true;
        s_triggerLoadImportSettings = false;
    }

    if (!s_openImportSettingsWindow) return;

    Begin("Import Settings", &s_openImportSettingsWindow, ImGuiWindowFlags_AlwaysAutoResize);

    if (s_importSettingsAssetPath.empty()) {
        Text("No asset selected.");
        End();
        return;
    }

    std::string ext = s_importSettingsAssetPath.extension().string();
    bool isTexture = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga");
    if (isTexture) {
        Text("Source Texture: %s", s_importMetadata.assetPath.c_str());
        Separator();
        drawSectionHeader("Texture Import Settings");

        const char* filterModes[] = { "Nearest (Point)", "Bilinear", "Trilinear" };
        int currentFilterIdx = 1; // Bilinear
        if (s_importMetadata.filterMode == TextureFilterMode::Nearest) currentFilterIdx = 0;
        else if (s_importMetadata.filterMode == TextureFilterMode::Trilinear) currentFilterIdx = 2;

        if (Combo("Filter Mode", &currentFilterIdx, filterModes, IM_ARRAYSIZE(filterModes))) {
            if (currentFilterIdx == 0) s_importMetadata.filterMode = TextureFilterMode::Nearest;
            else if (currentFilterIdx == 2) s_importMetadata.filterMode = TextureFilterMode::Trilinear;
            else s_importMetadata.filterMode = TextureFilterMode::Bilinear;
        }

        Spacing();
        Separator();
        if (Button("Apply Settings")) {
            saveImportSettings();
            renderer.resourceManager->updateTextureFilterMode(s_importMetadata.assetPath, renderer, s_importMetadata.filterMode);
            statusMessage = "Texture import settings applied live!";
            s_openImportSettingsWindow = false;
        }
        SameLine();
        if (Button("Cancel")) {
            s_openImportSettingsWindow = false;
        }
        End();
        return;
    }

    Text("Source Asset: %s", s_importMetadata.assetPath.c_str());
    Separator();

    // 1. General Import Options
    drawSectionHeader("Import Settings");
    InputFloat("Scale Factor", &s_importMetadata.scale, 0.01f, 0.1f, "%.4f");
    Checkbox("Generate Missing Normals", &s_importMetadata.generateNormals);
    Checkbox("Allow Missing Vertex Positions", &s_importMetadata.allowMissingPos);
    Checkbox("Force In-Place (Strip Root Motion XZ)", &s_importMetadata.forceInPlace);

    // 2. Animations List & Extraction
    Spacing();
    drawSectionHeader("Animations");
    if (s_importMetadata.animations.empty()) {
        TextDisabled("No animations found in this asset.");
    } else {
        if (Button("Extract All Animations")) {
            SkeletonComponent tempSkel{};
            AnimatorComponent tempAnim{};
            if (renderer.resourceManager->loadSkeletonAndAnimations(s_importMetadata.assetPath, tempSkel, tempAnim)) {
                std::string baseName = s_importSettingsAssetPath.stem().string();
                std::string relativePath = "assets/animations/" + baseName + ".anim";
                
                std::filesystem::create_directories("assets/animations");
                bool success = renderer.resourceManager->saveBinarySkeletonAndAnimations(relativePath, tempSkel, tempAnim);
                
                std::filesystem::path sourceBase("../../../sandbox_game");
                if (std::filesystem::exists(sourceBase / "assets")) {
                    std::filesystem::create_directories(sourceBase / "assets/animations");
                    renderer.resourceManager->saveBinarySkeletonAndAnimations((sourceBase / relativePath).generic_string(), tempSkel, tempAnim);
                }
                
                if (success) {
                    statusMessage = "Extracted all animations to " + relativePath;
                } else {
                    statusMessage = "Failed to save animations.";
                }
            } else {
                statusMessage = "Failed to load animation source.";
            }
        }
        
        for (const auto& anim : s_importMetadata.animations) {
            Text("  - %s (%.2fs)", anim.name.c_str(), anim.duration);
            SameLine(320);
            PushID(anim.name.c_str());
            if (Button("Extract")) {
                SkeletonComponent tempSkel{};
                AnimatorComponent tempAnim{};
                if (renderer.resourceManager->loadSkeletonAndAnimations(s_importMetadata.assetPath, tempSkel, tempAnim)) {
                    std::vector<AnimationClip> filtered;
                    for (const auto& clip : tempAnim.animations) {
                        if (clip.name == anim.name) {
                            filtered.push_back(clip);
                        }
                    }
                    if (!filtered.empty()) {
                        tempAnim.animations = filtered;
                        std::string baseName = s_importSettingsAssetPath.stem().string();
                        std::string relativePath = "assets/animations/" + baseName + "_" + anim.name + ".anim";
                        
                        std::filesystem::create_directories("assets/animations");
                        bool success = renderer.resourceManager->saveBinarySkeletonAndAnimations(relativePath, tempSkel, tempAnim);
                        
                        std::filesystem::path sourceBase("../../../sandbox_game");
                        if (std::filesystem::exists(sourceBase / "assets")) {
                            std::filesystem::create_directories(sourceBase / "assets/animations");
                            renderer.resourceManager->saveBinarySkeletonAndAnimations((sourceBase / relativePath).generic_string(), tempSkel, tempAnim);
                        }
                        
                        if (success) {
                            statusMessage = "Extracted animation to " + relativePath;
                        } else {
                            statusMessage = "Failed to save binary animation.";
                        }
                    } else {
                        statusMessage = "Animation clip not found.";
                    }
                } else {
                    statusMessage = "Failed to load skeleton/animation source.";
                }
            }
            PopID();
        }
    }

    // 3. Embedded Textures List & Extraction
    Spacing();
    drawSectionHeader("Embedded Textures");
    if (s_importMetadata.textures.empty()) {
        TextDisabled("No embedded textures found in this asset.");
    } else {
        if (Button("Extract All Textures")) {
            ufbx_load_opts opts = { 0 };
            ufbx_error error;
            ufbx_scene* scene = ufbx_load_file(s_importMetadata.assetPath.c_str(), &opts, &error);
            if (scene) {
                int count = 0;
                for (size_t i = 0; i < scene->texture_files.count; ++i) {
                    ufbx_texture_file& tf = scene->texture_files.data[i];
                    if (tf.content.size > 0) {
                        std::string outName = std::filesystem::path(tf.filename.data ? tf.filename.data : "").filename().string();
                        if (outName.empty()) outName = "extracted_texture_" + std::to_string(i) + ".png";
                        std::string relativePath = "assets/textures/" + outName;
                        if (writeExtractedFile(relativePath, tf.content.data, tf.content.size)) {
                            count++;
                        }
                    }
                }
                statusMessage = "Extracted " + std::to_string(count) + " textures.";
                ufbx_free_scene(scene);
            } else {
                statusMessage = "Failed to open FBX scene.";
            }
        }
        
        for (const auto& tex : s_importMetadata.textures) {
            Text("  - %s (%s)", tex.name.c_str(), tex.hasEmbeddedContent ? "embedded" : "reference");
            if (tex.hasEmbeddedContent) {
                SameLine(320);
                PushID(static_cast<int>(tex.index));
                if (Button("Extract")) {
                    ufbx_load_opts opts = { 0 };
                    ufbx_error error;
                    ufbx_scene* scene = ufbx_load_file(s_importMetadata.assetPath.c_str(), &opts, &error);
                    if (scene) {
                        if (tex.index < scene->texture_files.count) {
                            ufbx_texture_file& tf = scene->texture_files.data[tex.index];
                            std::string outName = std::filesystem::path(tf.filename.data ? tf.filename.data : "").filename().string();
                            if (outName.empty()) outName = "extracted_texture_" + std::to_string(tex.index) + ".png";
                            std::string relativePath = "assets/textures/" + outName;
                            if (writeExtractedFile(relativePath, tf.content.data, tf.content.size)) {
                                statusMessage = "Extracted texture to " + relativePath;
                            } else {
                                statusMessage = "Failed to write extracted file.";
                            }
                        }
                        ufbx_free_scene(scene);
                    }
                }
                PopID();
            }
        }
    }

    Separator();
    Spacing();

    // 4. Import / Apply Button
    if (Button("Apply & Re-import", ImVec2(150, 30))) {
        saveImportSettings();
        
        renderer.resourceManager->clearMeshCache(s_importMetadata.assetPath);
        
        if (hasSelection && registry.isValid(selectedEntity)) {
            if (auto* mesh = registry.get<Mesh>(selectedEntity)) {
                if (mesh->gltfPath == s_importMetadata.assetPath) {
                    try {
                        int primCount = renderer.resourceManager->getMeshPrimitiveCount(s_importMetadata.assetPath);
                        if (primCount > 1) {
                            Mesh loaded = renderer.resourceManager->loadMesh(s_importMetadata.assetPath, renderer, 0);
                            mesh->vertices = loaded.vertices;
                            mesh->indices = loaded.indices;
                            mesh->vertexBuffer = loaded.vertexBuffer;
                            mesh->indexBuffer = loaded.indexBuffer;
                            mesh->id = loaded.id;
                        } else {
                            Mesh loaded = renderer.resourceManager->loadMesh(s_importMetadata.assetPath, renderer);
                            mesh->vertices = loaded.vertices;
                            mesh->indices = loaded.indices;
                            mesh->vertexBuffer = loaded.vertexBuffer;
                            mesh->indexBuffer = loaded.indexBuffer;
                            mesh->id = loaded.id;
                        }
                        statusMessage = "Applied import settings and re-imported active mesh!";
                    } catch (const std::exception& e) {
                        statusMessage = std::string("Re-import failed: ") + e.what();
                    }
                }
            }
        }
        s_openImportSettingsWindow = false;
    }
    SameLine();
    if (Button("Cancel", ImVec2(100, 30))) {
        s_openImportSettingsWindow = false;
    }

    End();
}

void EditorUI::drawBuildSettingsPanel() {
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 260.0f,
               ImGui::GetIO().DisplaySize.y * 0.5f - 210.0f),
        ImGuiCond_FirstUseEver
    );

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Build Settings", &showBuildSettings, flags)) {
        ImGui::End();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.75f, 0.2f, 1.0f));
    ImGui::Text("[ Build Settings ]");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::Text("Platform");
    ImGui::PopStyleColor();
    ImGui::SameLine(120);
    ImGui::Text("Windows x64");
    ImGui::Spacing();

    ImGui::Text("Output Path");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(260);
    static char outputBuf[512];
    strncpy_s(outputBuf, buildOutputPath.c_str(), sizeof(outputBuf) - 1);
    if (ImGui::InputText("##build_output", outputBuf, sizeof(outputBuf))) {
        buildOutputPath = outputBuf;
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
    if (ImGui::Button("...##browse")) {
        // Future: open folder browser dialog
    }
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::Text("Included in build:");
    ImGui::PopStyleColor();
    ImGui::BulletText("game_runtime.exe -> game.exe");
    ImGui::BulletText("engine.dll");
    ImGui::BulletText("plugins/  (engine plugins)");
    ImGui::BulletText("scripts/  (compiled user script DLLs)");
    ImGui::BulletText("assets/");
    ImGui::BulletText("scenes/");
    ImGui::BulletText("shaders/");
    ImGui::BulletText("project.settings");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!buildStatusMessage.empty()) {
        bool isError = buildStatusMessage.find("[ERROR]") != std::string::npos ||
                       buildStatusMessage.find("FAIL") != std::string::npos;
        ImVec4 statusColor = isError
            ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f)
            : ImVec4(0.3f, 0.85f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
        ImGui::TextWrapped("%s", buildStatusMessage.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    float buttonWidth = 180.0f;
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - buttonWidth) * 0.5f);

    if (buildInProgress) {
        ImGui::BeginDisabled();
        ImGui::Button("Building...", ImVec2(buttonWidth, 32));
        ImGui::EndDisabled();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.45f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f,  0.55f, 0.95f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.1f,  0.35f, 0.65f, 1.0f));

        if (ImGui::Button("Build Game", ImVec2(buttonWidth, 32))) {
            buildInProgress = true;
            buildStatusMessage = "Building...";

            std::filesystem::path outPath = std::filesystem::absolute(buildOutputPath);
            int result = buildGameCallback ? buildGameCallback(".", outPath.string()) : -1;

            if (result == 0) {
                buildStatusMessage = "[OK] Build succeeded -> " + outPath.string();
                std::cout << "[BuildSystem] Build completed successfully." << std::endl;
            } else {
                buildStatusMessage = "[ERROR] Build failed (exit code " + std::to_string(result) + ")";
                std::cerr << "[BuildSystem] Build failed with exit code: " << result << std::endl;
            }

            buildInProgress = false;
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::Spacing();
    ImGui::End();
}

void EditorUI::drawTilesetEditorWindow() {
    if (!s_openTilesetEditorWindow) return;

    ImGui::SetNextWindowSize(ImVec2(900, 650), ImGuiCond_FirstUseEver);
    ImGui::Begin("Tileset Editor", &s_openTilesetEditorWindow,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

    // LEFT PANEL: Tileset file list + new tileset button
    const float listW = 190.f;
    BeginChild("##tsFileList", ImVec2(listW, 0), true);
    {
        TextDisabled("Tilesets");
        Separator();
        Spacing();

        std::filesystem::path tilesetDir = "assets/tilesets";
        if (!std::filesystem::exists(tilesetDir))
            std::filesystem::create_directories(tilesetDir);

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(tilesetDir, ec)) {
            if (entry.path().extension() != ".tileset") continue;
            std::string fname = entry.path().stem().string();
            std::string fpath = entry.path().generic_string();
            bool selected = (fpath == s_editingTilesetPath);

            PushStyleColor(ImGuiCol_Header,        ImVec4(0.20f, 0.45f, 0.70f, 1.f));
            PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.55f, 0.85f, 1.f));
            if (Selectable(fname.c_str(), selected, 0, ImVec2(-1, 0))) {
                s_editingTilesetPath = fpath;
                s_editingTileset     = Engine::TilesetAsset::loadFromFile(fpath);
                s_tilesetLoaded      = true;
                s_tsPanOffset        = ImVec2(0.f, 0.f);
                Engine::invalidateTilesetCache(fpath);
            }
            PopStyleColor(2);
        }

        Spacing(); Separator(); Spacing();

        if (Button("+ New Tileset", ImVec2(-1, 0)))
            OpenPopup("##NewTilesetPopup");

        static char s_newTsName[128] = "NewTileset";
        SetNextWindowSize(ImVec2(280, 0));
        if (BeginPopup("##NewTilesetPopup")) {
            Text("Tileset name:");
            SetNextItemWidth(-1);
            InputText("##newtsname", s_newTsName, sizeof(s_newTsName));
            Spacing();
            if (Button("Create", ImVec2(120, 0))) {
                std::string safeName = s_newTsName;
                if (safeName.empty()) safeName = "NewTileset";
                std::string newPath = (tilesetDir / (safeName + ".tileset")).generic_string();
                Engine::TilesetAsset newTs;
                newTs.name       = safeName;
                newTs.filePath   = newPath;
                newTs.tileWidth  = 16;
                newTs.tileHeight = 16;
                Engine::TilesetAsset::saveToFile(newTs);
                s_editingTilesetPath = newPath;
                s_editingTileset     = std::move(newTs);
                s_tilesetLoaded      = true;
                s_tsPanOffset        = ImVec2(0.f, 0.f);
                statusMessage = "Created tileset: " + safeName;
                CloseCurrentPopup();
            }
            SameLine();
            if (Button("Cancel", ImVec2(120, 0))) CloseCurrentPopup();
            EndPopup();
        }

        // Separator + tileset settings if one is loaded
        if (s_tilesetLoaded) {
            Spacing(); Separator(); Spacing();
            TextDisabled("Settings");

            char nameBuf[128] = {};
            strncpy_s(nameBuf, s_editingTileset.name.c_str(), sizeof(nameBuf) - 1);
            SetNextItemWidth(-1);
            if (InputText("##tsname", nameBuf, sizeof(nameBuf)))
                s_editingTileset.name = nameBuf;

            TextDisabled("Tile W/H (px)");
            SetNextItemWidth(-1);
            DragInt("##tsTW", &s_editingTileset.tileWidth,  1.f, 1, 512, "W: %d px");
            SetNextItemWidth(-1);
            DragInt("##tsTH", &s_editingTileset.tileHeight, 1.f, 1, 512, "H: %d px");
            if (s_editingTileset.tileWidth  < 1) s_editingTileset.tileWidth  = 1;
            if (s_editingTileset.tileHeight < 1) s_editingTileset.tileHeight = 1;

            Spacing();
            PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.48f, 0.22f, 1.f));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.60f, 0.28f, 1.f));
            if (Button("Save Tileset", ImVec2(-1, 0))) {
                Engine::TilesetAsset::saveToFile(s_editingTileset);
                std::filesystem::path tsDir = std::filesystem::path(s_editingTileset.filePath).parent_path();
                std::filesystem::path tileSubDir = tsDir / s_editingTileset.name;
                std::filesystem::create_directories(tileSubDir);
                for (auto& tile : s_editingTileset.tiles) {
                    std::string tilePath = (tileSubDir / (tile.name + ".tile")).generic_string();
                    Engine::TilesetAsset::saveTileFile(tile, tilePath);
                }
                Engine::invalidateTilesetCache(s_editingTilesetPath);
                if (auto* ts = Engine::loadOrGetTileset(s_editingTilesetPath, renderer)) {
                    for (auto [tmEnt, tm] : registry.view<Engine::TilemapComponent>()) {
                        if (tm.tilesetPath == s_editingTilesetPath) {
                            tm.isDirty = true;
                            if (auto* mat = registry.get<Material>(tmEnt)) {
                                mat->descriptorSet = ts->atlas.descriptorSet;
                            }
                        }
                    }
                }
                statusMessage = "Saved tileset: " + s_editingTileset.name;
            }
            PopStyleColor(2);
        }
    }
    EndChild();

    SameLine();

    // RIGHT PANEL: Infinite grid palette canvas
    BeginChild("##tsGrid", ImVec2(0, 0), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (!s_tilesetLoaded) {
        ImVec2 sz = GetContentRegionAvail();
        ImVec2 cp = GetCursorScreenPos();
        GetWindowDrawList()->AddRectFilled(cp, ImVec2(cp.x+sz.x, cp.y+sz.y), IM_COL32(28,28,35,255));
        ImVec2 tc = ImVec2(cp.x + sz.x*0.5f - 180.f, cp.y + sz.y*0.5f - 10.f);
        GetWindowDrawList()->AddText(nullptr, 16.f, tc, IM_COL32(100,100,120,200),
            "Select or create a tileset on the left.");
    } else {
        // Build a map from (gridX,gridY) -> tile index for fast lookup
        std::unordered_map<uint64_t, int> cellMap;
        auto cellKey = [](int gx, int gy) -> uint64_t {
            return ((uint64_t)(uint32_t)gx) | (((uint64_t)(uint32_t)gy) << 32);
        };
        for (int i = 0; i < (int)s_editingTileset.tiles.size(); ++i) {
            auto& t = s_editingTileset.tiles[i];
            cellMap[cellKey(t.gridX, t.gridY)] = i;
        }

        // Canvas region
        ImVec2 canvasPos  = GetCursorScreenPos();
        ImVec2 canvasSize = GetContentRegionAvail();
        if (canvasSize.x < 10.f) canvasSize.x = 10.f;
        if (canvasSize.y < 10.f) canvasSize.y = 10.f;

        // Invisible button to capture mouse events
        InvisibleButton("##tsCanvas", canvasSize,
            ImGuiButtonFlags_MouseButtonLeft  |
            ImGuiButtonFlags_MouseButtonRight |
            ImGuiButtonFlags_MouseButtonMiddle);
        const bool canvasHovered = IsItemHovered();
        const bool canvasActive  = IsItemActive();

        ImVec2 mousePos = GetIO().MousePos;

        // --- Zoom (scroll wheel) ---
        if (canvasHovered) {
            float wheel = GetIO().MouseWheel;
            if (wheel != 0.f) {
                float zoomFactor = (wheel > 0) ? 1.12f : (1.f / 1.12f);
                float newCell = s_tsCellSize * zoomFactor;
                newCell = std::max(12.f, std::min(256.f, newCell));
                ImVec2 mouseInCanvas = ImVec2(mousePos.x - canvasPos.x, mousePos.y - canvasPos.y);
                float scale = newCell / s_tsCellSize;
                s_tsPanOffset.x = mouseInCanvas.x - scale * (mouseInCanvas.x - s_tsPanOffset.x);
                s_tsPanOffset.y = mouseInCanvas.y - scale * (mouseInCanvas.y - s_tsPanOffset.y);
                s_tsCellSize = newCell;
            }
        }

        // --- Pan (middle mouse or right mouse drag) ---
        bool wantPan = canvasActive && (
            IsMouseDown(ImGuiMouseButton_Middle) ||
            (IsMouseDown(ImGuiMouseButton_Right) && !IsAnyItemHovered()));

        if (wantPan && !s_tsIsPanning) {
            s_tsIsPanning = true;
            s_tsPanStart  = mousePos;
            s_tsPanOffsetStart = s_tsPanOffset;
        }
        if (!IsMouseDown(ImGuiMouseButton_Middle) && !IsMouseDown(ImGuiMouseButton_Right))
            s_tsIsPanning = false;

        if (s_tsIsPanning) {
            s_tsPanOffset.x = s_tsPanOffsetStart.x + (mousePos.x - s_tsPanStart.x);
            s_tsPanOffset.y = s_tsPanOffsetStart.y + (mousePos.y - s_tsPanStart.y);
        }

        // Draw grid lines and cells
        ImDrawList* dl = GetWindowDrawList();
        dl->PushClipRect(canvasPos,
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

        // Dark background
        dl->AddRectFilled(canvasPos,
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
            IM_COL32(28, 28, 36, 255));

        const float cs = s_tsCellSize;
        const float ox = canvasPos.x + s_tsPanOffset.x;
        const float oy = canvasPos.y + s_tsPanOffset.y;

        // Determine visible cell range
        int colMin = (int)std::floor((canvasPos.x - ox) / cs) - 1;
        int colMax = (int)std::ceil ((canvasPos.x + canvasSize.x - ox) / cs) + 1;
        int rowMin = (int)std::floor((canvasPos.y - oy) / cs) - 1;
        int rowMax = (int)std::ceil ((canvasPos.y + canvasSize.y - oy) / cs) + 1;

        const int MAX_RANGE = 64;
        if (colMax - colMin > MAX_RANGE) { colMin = -MAX_RANGE/2; colMax = MAX_RANGE/2; }
        if (rowMax - rowMin > MAX_RANGE) { rowMin = -MAX_RANGE/2; rowMax = MAX_RANGE/2; }

        // Grid lines
        ImU32 gridLineCol   = IM_COL32(55, 55, 70, 200);
        ImU32 originLineCol = IM_COL32(80, 80, 110, 255);
        for (int col = colMin; col <= colMax; ++col) {
            float x = ox + col * cs;
            ImU32 c = (col == 0) ? originLineCol : gridLineCol;
            dl->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), c, col == 0 ? 2.f : 1.f);
        }
        for (int row = rowMin; row <= rowMax; ++row) {
            float y = oy + row * cs;
            ImU32 c = (row == 0) ? originLineCol : gridLineCol;
            dl->AddLine(ImVec2(canvasPos.x, y), ImVec2(canvasPos.x + canvasSize.x, y), c, row == 0 ? 2.f : 1.f);
        }

        // Coordinate labels on empty cells (only when zoomed in enough)
        if (cs >= 48.f) {
            for (int col = colMin; col <= colMax; ++col) {
                for (int row = rowMin; row <= rowMax; ++row) {
                    auto it = cellMap.find(cellKey(col, row));
                    if (it == cellMap.end()) {
                        float x = ox + col * cs;
                        float y = oy + row * cs;
                        char lbl[32];
                        snprintf(lbl, sizeof(lbl), "%d,%d", col, row);
                        dl->AddText(ImVec2(x+3.f, y+3.f), IM_COL32(60,60,80,180), lbl);
                    }
                }
            }
        }

        // Draw placed tiles
        static int s_rightClickedTileIdx = -1;
        for (auto& [key, tileIdx] : cellMap) {
            if (tileIdx < 0 || tileIdx >= (int)s_editingTileset.tiles.size()) continue;
            auto& tile = s_editingTileset.tiles[tileIdx];
            float x = ox + tile.gridX * cs;
            float y = oy + tile.gridY * cs;
            ImVec2 tl = ImVec2(x, y);
            ImVec2 br = ImVec2(x + cs, y + cs);
            bool isSelected = (s_brushTileId == tileIdx);

            dl->AddRectFilled(tl, br,
                isSelected ? IM_COL32(30, 90, 180, 200) : IM_COL32(40, 40, 55, 220));

            // Texture thumbnail
            if (!tile.texturePath.empty()) {
                Texture* tex = renderer.resourceManager->loadTexture(tile.texturePath, renderer);
                if (tex && tex->descriptorSet != VK_NULL_HANDLE) {
                    dl->AddImage((ImTextureID)tex->descriptorSet, tl, br);
                }
            }

            // Solid tint
            if (tile.isSolid)
                dl->AddRectFilled(tl, br, IM_COL32(220, 40, 40, 70));

            // Border
            ImU32 borderCol = isSelected ? IM_COL32(80, 160, 255, 255) : IM_COL32(110, 110, 140, 200);
            dl->AddRect(tl, br, borderCol, 0.f, 0, isSelected ? 2.5f : 1.5f);

            // Tile name label strip
            if (cs >= 36.f) {
                float labelH = std::min(14.f, cs * 0.20f);
                ImVec2 lblTL = ImVec2(tl.x, br.y - labelH);
                dl->AddRectFilled(lblTL, br, IM_COL32(0, 0, 0, 160));
                dl->AddText(ImVec2(lblTL.x + 2.f, lblTL.y),
                    IM_COL32(220, 220, 220, 255), tile.name.c_str());
            }
        }

        // --- Mouse interaction ---
        ImVec2 mouseInCanvas = ImVec2(mousePos.x - canvasPos.x, mousePos.y - canvasPos.y);
        int hovCol = (int)std::floor((mouseInCanvas.x - s_tsPanOffset.x) / cs);
        int hovRow = (int)std::floor((mouseInCanvas.y - s_tsPanOffset.y) / cs);

        // Hover highlight
        if (canvasHovered && !s_tsIsPanning) {
            float hx = ox + hovCol * cs;
            float hy = oy + hovRow * cs;
            dl->AddRectFilled(ImVec2(hx, hy), ImVec2(hx + cs, hy + cs), IM_COL32(255, 255, 255, 18));
            dl->AddRect(ImVec2(hx, hy), ImVec2(hx + cs, hy + cs), IM_COL32(180, 180, 200, 120));

            // Tooltip
            auto it = cellMap.find(cellKey(hovCol, hovRow));
            BeginTooltip();
            if (it != cellMap.end() && it->second >= 0 && it->second < (int)s_editingTileset.tiles.size()) {
                auto& t = s_editingTileset.tiles[it->second];
                Text("[%d, %d]  ID=%d  %s", hovCol, hovRow, t.id, t.name.c_str());
                Text("Texture: %s", t.texturePath.c_str());
                Text(t.isSolid ? "Solid: YES" : "Solid: NO");
                Text("LMB=Select brush  RMB=Options");
            } else {
                Text("[%d, %d]  (empty)", hovCol, hovRow);
                Text("Drag a texture here to place a tile");
            }
            EndTooltip();
        }

        // Left-click: select/deselect brush
        if (canvasHovered && IsMouseClicked(ImGuiMouseButton_Left) && !s_tsIsPanning) {
            auto it = cellMap.find(cellKey(hovCol, hovRow));
            if (it != cellMap.end() && it->second >= 0 && it->second < (int)s_editingTileset.tiles.size()) {
                int idx = it->second;
                if (s_brushTileId == idx) {
                    s_brushTileId = -1;
                    s_brushModeActive = false;
                    statusMessage = "Brush cleared.";
                } else {
                    s_brushTileId = idx;
                    s_brushModeActive = true;
                    statusMessage = "Brush: " + s_editingTileset.tiles[idx].name;
                }
            }
        }

        // Right-click context menu
        if (canvasHovered && IsMouseClicked(ImGuiMouseButton_Right) && !s_tsIsPanning) {
            auto it = cellMap.find(cellKey(hovCol, hovRow));
            if (it != cellMap.end()) {
                s_rightClickedTileIdx = it->second;
                OpenPopup("##TileCtxMenu");
            }
        }
        if (BeginPopup("##TileCtxMenu")) {
            int idx = s_rightClickedTileIdx;
            if (idx >= 0 && idx < (int)s_editingTileset.tiles.size()) {
                auto& tile = s_editingTileset.tiles[idx];
                TextDisabled("%s  [%d,%d]", tile.name.c_str(), tile.gridX, tile.gridY);
                Separator();
                bool solid = tile.isSolid;
                if (Checkbox("Solid Collider", &solid)) {
                    tile.isSolid = solid;
                    std::filesystem::path tsDir = std::filesystem::path(s_editingTileset.filePath).parent_path();
                    std::string tilePath = (tsDir / s_editingTileset.name / (tile.name + ".tile")).generic_string();
                    Engine::TilesetAsset::saveTileFile(tile, tilePath);
                    Engine::TilesetAsset::saveToFile(s_editingTileset);
                    Engine::invalidateTilesetCache(s_editingTilesetPath);
                    if (auto* ts = Engine::loadOrGetTileset(s_editingTilesetPath, renderer)) {
                        for (auto [tmEnt, tm] : registry.view<Engine::TilemapComponent>()) {
                            if (tm.tilesetPath == s_editingTilesetPath) {
                                tm.isDirty = true;
                                if (auto* mat = registry.get<Material>(tmEnt)) {
                                    mat->descriptorSet = ts->atlas.descriptorSet;
                                }
                            }
                        }
                    }
                }
                Separator();
                PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.3f, 0.3f, 1.f));
                if (MenuItem("Remove Tile")) {
                    if (s_brushTileId == idx)       { s_brushTileId = -1; s_brushModeActive = false; }
                    else if (s_brushTileId > idx)   { s_brushTileId--; }
                    s_editingTileset.tiles.erase(s_editingTileset.tiles.begin() + idx);
                    for (int ti = 0; ti < (int)s_editingTileset.tiles.size(); ++ti)
                        s_editingTileset.tiles[ti].id = ti;
                    Engine::TilesetAsset::saveToFile(s_editingTileset);
                    Engine::invalidateTilesetCache(s_editingTilesetPath);
                    if (auto* ts = Engine::loadOrGetTileset(s_editingTilesetPath, renderer)) {
                        for (auto [tmEnt, tm] : registry.view<Engine::TilemapComponent>()) {
                            if (tm.tilesetPath == s_editingTilesetPath) {
                                tm.isDirty = true;
                                if (auto* mat = registry.get<Material>(tmEnt)) {
                                    mat->descriptorSet = ts->atlas.descriptorSet;
                                }
                            }
                        }
                    }
                    statusMessage = "Removed tile.";
                    s_rightClickedTileIdx = -1;
                }
                PopStyleColor();
            }
            EndPopup();
        }

        // --- Drag-drop: accept texture files onto the grid ---
        SetCursorScreenPos(canvasPos);
        InvisibleButton("##tsDropTarget", canvasSize);
        if (BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = AcceptDragDropPayload("DND_PAYLOAD_ASSET_PATH")) {
                std::string droppedPath = (const char*)payload->Data;
                auto ext = std::filesystem::path(droppedPath).extension().string();
                bool isImg = (ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".tga");
                if (isImg) {
                    ImVec2 dropMouse     = GetIO().MousePos;
                    ImVec2 dropInCanvas  = ImVec2(dropMouse.x - canvasPos.x, dropMouse.y - canvasPos.y);
                    int dropCol = (int)std::floor((dropInCanvas.x - s_tsPanOffset.x) / cs);
                    int dropRow = (int)std::floor((dropInCanvas.y - s_tsPanOffset.y) / cs);

                    auto it = cellMap.find(cellKey(dropCol, dropRow));
                    if (it != cellMap.end()) {
                        // Overwrite existing tile's texture
                        auto& existing   = s_editingTileset.tiles[it->second];
                        existing.texturePath = droppedPath;
                        existing.name        = std::filesystem::path(droppedPath).stem().string();
                        std::filesystem::path tsDir = std::filesystem::path(s_editingTileset.filePath).parent_path();
                        std::string tilePath = (tsDir / s_editingTileset.name / (existing.name + ".tile")).generic_string();
                        Engine::TilesetAsset::saveTileFile(existing, tilePath);
                        statusMessage = "Replaced tile at [" + std::to_string(dropCol) + "," + std::to_string(dropRow) + "]";
                    } else {
                        // New tile at this position
                        Engine::TileAsset newTile;
                        newTile.id          = static_cast<int>(s_editingTileset.tiles.size());
                        newTile.name        = std::filesystem::path(droppedPath).stem().string();
                        newTile.texturePath = droppedPath;
                        newTile.isSolid     = false;
                        newTile.gridX       = dropCol;
                        newTile.gridY       = dropRow;

                        std::filesystem::path tsDir = std::filesystem::path(s_editingTileset.filePath).parent_path();
                        std::filesystem::path tileSubDir = tsDir / s_editingTileset.name;
                        std::filesystem::create_directories(tileSubDir);
                        std::string tilePath = (tileSubDir / (newTile.name + ".tile")).generic_string();
                        Engine::TilesetAsset::saveTileFile(newTile, tilePath);
                        s_editingTileset.tiles.push_back(std::move(newTile));
                        statusMessage = "Added tile at [" + std::to_string(dropCol) + "," + std::to_string(dropRow) + "]";
                    }

                    Engine::TilesetAsset::saveToFile(s_editingTileset);
                    Engine::invalidateTilesetCache(s_editingTilesetPath);
                    if (auto* ts = Engine::loadOrGetTileset(s_editingTilesetPath, renderer)) {
                        for (auto [tmEnt, tm] : registry.view<Engine::TilemapComponent>()) {
                            if (tm.tilesetPath == s_editingTilesetPath) {
                                tm.isDirty = true;
                                if (auto* mat = registry.get<Material>(tmEnt)) {
                                    mat->descriptorSet = ts->atlas.descriptorSet;
                                }
                            }
                        }
                    }
                }
            }
            EndDragDropTarget();
        }

        dl->PopClipRect();

        // --- Bottom HUD bar ---
        {
            const float barH = 22.f;
            ImVec2 barTL = ImVec2(canvasPos.x, canvasPos.y + canvasSize.y - barH);
            ImVec2 barBR = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
            GetWindowDrawList()->AddRectFilled(barTL, barBR, IM_COL32(20, 20, 28, 220));

            char hudBuf[256];
            if (s_brushTileId >= 0 && s_brushTileId < (int)s_editingTileset.tiles.size()) {
                auto& bt = s_editingTileset.tiles[s_brushTileId];
                snprintf(hudBuf, sizeof(hudBuf),
                    "  Brush: [%d] %s  |  Zoom: %.0fpx  |  Scroll=Zoom  MMB/RMB=Pan  LMB=Select  RMB=Options",
                    s_brushTileId, bt.name.c_str(), cs);
            } else {
                snprintf(hudBuf, sizeof(hudBuf),
                    "  No brush selected  |  Zoom: %.0fpx  |  Scroll=Zoom  MMB/RMB=Pan  LMB=Select  Drag texture=Place",
                    cs);
            }
            GetWindowDrawList()->AddText(
                ImVec2(barTL.x + 4.f, barTL.y + 3.f),
                s_brushTileId >= 0 ? IM_COL32(100, 200, 255, 255) : IM_COL32(140, 140, 160, 220),
                hudBuf);
        }

        // --- Paint target dropdown (top-right corner overlay) ---
        if (s_tilesetLoaded) {
            const float comboW = 190.f;
            const float comboH = 22.f;
            SetCursorScreenPos(ImVec2(canvasPos.x + canvasSize.x - comboW - 4.f, canvasPos.y + 4.f));
            PushItemWidth(comboW);
            std::string previewLabel = "Select Tilemap...";
            for (auto [tmEnt, tm] : registry.view<Engine::TilemapComponent>()) {
                if (s_brushTilemapEntity == tmEnt) {
                    if (auto* n = registry.get<Name>(tmEnt)) previewLabel = n->value;
                    else previewLabel = "Entity " + std::to_string(tmEnt.getId());
                    break;
                }
            }
            if (BeginCombo("##tmTarget", previewLabel.c_str(), ImGuiComboFlags_HeightSmall)) {
                for (auto [tmEnt, tm] : registry.view<Engine::TilemapComponent>()) {
                    std::string lbl;
                    if (auto* n = registry.get<Name>(tmEnt)) lbl = n->value;
                    else lbl = "Entity " + std::to_string(tmEnt.getId());
                    bool sel = (s_brushTilemapEntity == tmEnt);
                    if (Selectable(lbl.c_str(), sel))
                        s_brushTilemapEntity = tmEnt;
                }
                EndCombo();
            }
            PopItemWidth();
        }
    }
    EndChild();

    End();
}

void EditorUI::drawAnimationEditorWindow() {
    if (!s_openAnimationEditorWindow) return;

    ImGui::SetNextWindowSize(ImVec2(800, 450), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Animation Editor", &s_openAnimationEditorWindow)) {
        ImGui::End();
        return;
    }

    if (!hasSelection || !registry.isValid(selectedEntity)) {
        ImGui::Text("Select an Entity with an AnimatorComponent to edit animations.");
        ImGui::End();
        return;
    }

    // Check if the entity has an AnimatorComponent
    AnimatorComponent* animator = registry.get<AnimatorComponent>(selectedEntity);
    if (!animator) {
        ImGui::Text("The selected entity does not have an AnimatorComponent.");
        if (ImGui::Button("Add Animator Component")) {
            registry.emplace<AnimatorComponent>(selectedEntity, AnimatorComponent{});
        }
        ImGui::End();
        return;
    }

    // Active Clip dropdown
    static char s_newClipName[128] = "New Clip";
    AnimationClip* activeClip = nullptr;
    if (animator->activeAnimationIndex >= 0 && animator->activeAnimationIndex < static_cast<int>(animator->animations.size())) {
        activeClip = &animator->animations[animator->activeAnimationIndex];
    }

    // Toolbar Row
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Active Clip:");
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    
    std::string previewLabel = activeClip ? activeClip->name : "None";
    if (ImGui::BeginCombo("##active_clip", previewLabel.c_str())) {
        for (int i = 0; i < static_cast<int>(animator->animations.size()); ++i) {
            bool isSelected = (i == animator->activeAnimationIndex);
            if (ImGui::Selectable(animator->animations[i].name.c_str(), isSelected)) {
                animator->activeAnimationIndex = i;
                animator->currentTime = 0.0f;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Create New Clip")) {
        ImGui::OpenPopup("CreateNewClipPopup");
    }

    // Modal popup to create new clip
    if (ImGui::BeginPopup("CreateNewClipPopup")) {
        ImGui::Text("Clip Name:");
        ImGui::InputText("##clip_name_input", s_newClipName, sizeof(s_newClipName));
        if (ImGui::Button("Create##btn")) {
            AnimationClip newClip;
            newClip.name = s_newClipName;
            newClip.duration = 2.0f; // Default 2 seconds
            animator->animations.push_back(newClip);
            animator->activeAnimationIndex = animator->animations.size() - 1;
            animator->currentTime = 0.0f;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    static char s_saveLoadPath[256] = "assets/animations/cube.anim";
    if (!animator->loadedAnimPath.empty() && strcmp(s_saveLoadPath, animator->loadedAnimPath.c_str()) != 0) {
        strcpy_s(s_saveLoadPath, animator->loadedAnimPath.c_str());
    }
    ImGui::PushItemWidth(200);
    ImGui::InputText("Path##save_load", s_saveLoadPath, sizeof(s_saveLoadPath));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        SkeletonComponent dummySkeleton;
        if (auto* skel = registry.get<SkeletonComponent>(selectedEntity)) {
            renderer.resourceManager->saveBinarySkeletonAndAnimations(s_saveLoadPath, *skel, *animator);
        } else {
            renderer.resourceManager->saveBinarySkeletonAndAnimations(s_saveLoadPath, dummySkeleton, *animator);
        }
        statusMessage = "Saved animations to: " + std::string(s_saveLoadPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        SkeletonComponent dummySkeleton;
        SkeletonComponent* skelPtr = &dummySkeleton;
        if (auto* skel = registry.get<SkeletonComponent>(selectedEntity)) {
            skelPtr = skel;
        }
        if (renderer.resourceManager->loadBinarySkeletonAndAnimations(s_saveLoadPath, *skelPtr, *animator, false)) {
            statusMessage = "Loaded animations from: " + std::string(s_saveLoadPath);
            animator->loadedAnimPath = s_saveLoadPath;
        } else {
            statusMessage = "Failed to load animations from: " + std::string(s_saveLoadPath);
        }
    }

    if (!activeClip) {
        ImGui::TextDisabled("No active animation clip selected. Create or load one to begin animating.");
        ImGui::End();
        return;
    }

    // Timeline settings and playback
    ImGui::Separator();
    
    // Play/Pause buttons
    bool isPlaying = (animator->playbackSpeed > 0.0f);
    if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
        if (isPlaying) {
            animator->playbackSpeed = 0.0f;
        } else {
            animator->playbackSpeed = 1.0f;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        animator->playbackSpeed = 0.0f;
        animator->currentTime = 0.0f;
    }
    
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Time:");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::DragFloat("##current_time", &animator->currentTime, 0.01f, 0.0f, activeClip->duration, "%.2fs");
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Duration:");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::DragFloat("##duration_input", &activeClip->duration, 0.1f, 0.1f, 100.0f, "%.1fs");
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::Checkbox("Loop", &animator->loop);

    ImGui::SameLine();
    if (ImGui::Button("Add Property")) {
        ImGui::OpenPopup("AddPropertyPopup");
    }

    if (ImGui::BeginPopup("AddPropertyPopup")) {
        auto& reflReg = Engine::ComponentReflectionRegistry::getInstance();
        for (const auto& refl : reflReg.getReflections()) {
            if (refl.has(registry, selectedEntity)) {
                if (ImGui::BeginMenu(refl.name.c_str())) {
                    for (const auto& field : refl.fields) {
                        if (field.type == Engine::FieldType::Float ||
                            field.type == Engine::FieldType::Bool ||
                            field.type == Engine::FieldType::Vec2 ||
                            field.type == Engine::FieldType::Vec3 ||
                            field.type == Engine::FieldType::Vec4) {
                            
                            bool exists = false;
                            for (const auto& chan : activeClip->propertyChannels) {
                                if (chan.componentName == refl.name && chan.fieldName == field.name) {
                                    exists = true;
                                    break;
                                }
                            }

                            if (!exists) {
                                if (ImGui::MenuItem(field.name.c_str())) {
                                    PropertyChannel newChan;
                                    newChan.componentName = refl.name;
                                    newChan.fieldName = field.name;
                                    newChan.type = field.type;
                                    activeClip->propertyChannels.push_back(newChan);
                                }
                            } else {
                                ImGui::MenuItem(field.name.c_str(), nullptr, false, false);
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    static int s_selectedTrackIndex = -1;
    static int s_selectedKeyIndex = -1;

    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    float leftColWidth = 250.0f;
    float rightColWidth = contentSize.x - leftColWidth - 8.0f;

    ImGui::BeginChild("##left_properties", ImVec2(leftColWidth, contentSize.y - 120.0f), true);
    ImGui::Text("Properties");
    ImGui::Separator();
    
    int trackToDelete = -1;
    for (int t = 0; t < static_cast<int>(activeClip->propertyChannels.size()); ++t) {
        auto& chan = activeClip->propertyChannels[t];
        
        ImGui::PushID(t);
        
        bool isSelected = (s_selectedTrackIndex == t);
        if (ImGui::Selectable("##track_select", isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 20))) {
            s_selectedTrackIndex = t;
            s_selectedKeyIndex = -1;
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 220.0f);
        
        ImGui::Text("%s.%s", chan.componentName.c_str(), chan.fieldName.c_str());
        
        ImGui::SameLine(leftColWidth - 80.0f);
        if (ImGui::Button("Key")) {
            auto& reflReg = Engine::ComponentReflectionRegistry::getInstance();
            const Engine::ComponentReflection* targetRefl = nullptr;
            for (const auto& refl : reflReg.getReflections()) {
                if (refl.name == chan.componentName) {
                    targetRefl = &refl;
                    break;
                }
            }
            if (targetRefl && targetRefl->has(registry, selectedEntity)) {
                void* compPtr = targetRefl->get(registry, selectedEntity);
                if (compPtr) {
                    const Engine::ComponentField* targetField = nullptr;
                    for (const auto& f : targetRefl->fields) {
                        if (f.name == chan.fieldName) {
                            targetField = &f;
                            break;
                        }
                    }
                    if (targetField) {
                        char* fieldPtr = static_cast<char*>(compPtr) + targetField->offset;
                        glm::vec4 capturedVal(0.0f);
                        
                        if (chan.type == Engine::FieldType::Float) {
                            capturedVal.x = *reinterpret_cast<float*>(fieldPtr);
                        } else if (chan.type == Engine::FieldType::Bool) {
                            capturedVal.x = *reinterpret_cast<bool*>(fieldPtr) ? 1.0f : 0.0f;
                        } else if (chan.type == Engine::FieldType::Vec2) {
                            auto* v = reinterpret_cast<glm::vec2*>(fieldPtr);
                            capturedVal = glm::vec4(v->x, v->y, 0.0f, 0.0f);
                        } else if (chan.type == Engine::FieldType::Vec3) {
                            auto* v = reinterpret_cast<glm::vec3*>(fieldPtr);
                            capturedVal = glm::vec4(v->x, v->y, v->z, 0.0f);
                        } else if (chan.type == Engine::FieldType::Vec4) {
                            capturedVal = *reinterpret_cast<glm::vec4*>(fieldPtr);
                        }

                        PropertyKeyframe newKey;
                        newKey.time = animator->currentTime;
                        newKey.value = capturedVal;

                        bool found = false;
                        for (auto& key : chan.keys) {
                            if (std::abs(key.time - animator->currentTime) < 0.01f) {
                                key.value = capturedVal;
                                found = true;
                                break;
                            }
                        }

                        if (!found) {
                            chan.keys.push_back(newKey);
                            std::sort(chan.keys.begin(), chan.keys.end(), [](const PropertyKeyframe& a, const PropertyKeyframe& b) {
                                return a.time < b.time;
                            });
                        }
                        statusMessage = "Added keyframe for " + chan.componentName + "." + chan.fieldName + " at " + std::to_string(animator->currentTime) + "s";
                    }
                }
            }
        }
        ImGui::SameLine(leftColWidth - 35.0f);
        if (ImGui::Button("X")) {
            trackToDelete = t;
        }

        ImGui::PopID();
    }

    if (trackToDelete != -1) {
        activeClip->propertyChannels.erase(activeClip->propertyChannels.begin() + trackToDelete);
        s_selectedTrackIndex = -1;
        s_selectedKeyIndex = -1;
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##right_timeline", ImVec2(rightColWidth, contentSize.y - 120.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 startCursorPos = ImGui::GetCursorScreenPos();
    float timelineDuration = activeClip->duration;
    
    float pixelsPerSecond = 150.0f;
    float trackWidth = timelineDuration * pixelsPerSecond;
    float trackHeight = 20.0f;

    ImVec2 rulerStart = startCursorPos;
    ImVec2 rulerEnd = ImVec2(rulerStart.x + trackWidth, rulerStart.y + 25.0f);
    
    drawList->AddRectFilled(rulerStart, rulerEnd, IM_COL32(40, 40, 40, 255));
    
    float playheadX = rulerStart.x + animator->currentTime * pixelsPerSecond;
    
    for (float tSec = 0.0f; tSec <= timelineDuration; tSec += 0.1f) {
        float xPos = rulerStart.x + tSec * pixelsPerSecond;
        bool isMajor = (std::fmod(tSec + 0.001f, 0.5f) < 0.01f);
        float tickH = isMajor ? 12.0f : 6.0f;
        drawList->AddLine(ImVec2(xPos, rulerStart.y + 25.0f - tickH), ImVec2(xPos, rulerStart.y + 25.0f), IM_COL32(150, 150, 150, 255));
        
        if (isMajor) {
            char label[16];
            snprintf(label, sizeof(label), "%.1fs", tSec);
            drawList->AddText(ImVec2(xPos + 2.0f, rulerStart.y + 2.0f), IM_COL32(200, 200, 200, 255), label);
        }
    }

    ImGui::InvisibleButton("##ruler_scrub", ImVec2(trackWidth, 25.0f));
    if (ImGui::IsItemActive()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float localMouseX = mousePos.x - rulerStart.x;
        animator->currentTime = glm::clamp(localMouseX / pixelsPerSecond, 0.0f, timelineDuration);
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.0f);

    for (int t = 0; t < static_cast<int>(activeClip->propertyChannels.size()); ++t) {
        auto& chan = activeClip->propertyChannels[t];
        ImVec2 laneStart = ImGui::GetCursorScreenPos();
        ImVec2 laneEnd = ImVec2(laneStart.x + trackWidth, laneStart.y + trackHeight);

        ImU32 laneColor = (t % 2 == 0) ? IM_COL32(50, 50, 50, 255) : IM_COL32(45, 45, 45, 255);
        if (s_selectedTrackIndex == t) {
            laneColor = IM_COL32(70, 70, 70, 255);
        }
        drawList->AddRectFilled(laneStart, laneEnd, laneColor);

        ImGui::PushID(t);
        ImGui::SetCursorScreenPos(laneStart);
        ImGui::InvisibleButton("##lane_btn", ImVec2(trackWidth, trackHeight));
        
        if (ImGui::IsItemClicked()) {
            s_selectedTrackIndex = t;
            s_selectedKeyIndex = -1;
            
            ImVec2 mPos = ImGui::GetMousePos();
            for (int k = 0; k < static_cast<int>(chan.keys.size()); ++k) {
                float keyX = laneStart.x + chan.keys[k].time * pixelsPerSecond;
                if (std::abs(mPos.x - keyX) < 8.0f) {
                    s_selectedKeyIndex = k;
                    animator->currentTime = chan.keys[k].time;
                    break;
                }
            }
        }
        ImGui::PopID();

        for (int k = 0; k < static_cast<int>(chan.keys.size()); ++k) {
            float keyX = laneStart.x + chan.keys[k].time * pixelsPerSecond;
            float keyY = laneStart.y + trackHeight * 0.5f;

            bool isKeySelected = (s_selectedTrackIndex == t && s_selectedKeyIndex == k);
            ImU32 keyColor = isKeySelected ? IM_COL32(255, 200, 50, 255) : IM_COL32(220, 220, 220, 255);
            
            ImVec2 points[4] = {
                ImVec2(keyX, keyY - 5.0f),
                ImVec2(keyX + 5.0f, keyY),
                ImVec2(keyX, keyY + 5.0f),
                ImVec2(keyX - 5.0f, keyY)
            };
            drawList->AddConvexPolyFilled(points, 4, keyColor);
            drawList->AddPolyline(points, 4, IM_COL32(0, 0, 0, 255), true, 1.0f);
        }

        ImGui::SetCursorScreenPos(ImVec2(laneStart.x, laneStart.y + trackHeight));
    }

    float playheadBottomY = ImGui::GetCursorScreenPos().y;
    drawList->AddLine(
        ImVec2(playheadX, rulerStart.y),
        ImVec2(playheadX, playheadBottomY),
        IM_COL32(70, 180, 255, 200),
        2.0f
    );
    drawList->AddTriangleFilled(
        ImVec2(playheadX - 6.0f, rulerStart.y + 20.0f),
        ImVec2(playheadX + 6.0f, rulerStart.y + 20.0f),
        ImVec2(playheadX, rulerStart.y + 25.0f),
        IM_COL32(70, 180, 255, 255)
    );

    ImGui::EndChild();

    ImGui::Separator();
    if (s_selectedTrackIndex >= 0 && s_selectedTrackIndex < static_cast<int>(activeClip->propertyChannels.size())) {
        auto& chan = activeClip->propertyChannels[s_selectedTrackIndex];
        if (s_selectedKeyIndex >= 0 && s_selectedKeyIndex < static_cast<int>(chan.keys.size())) {
            auto& key = chan.keys[s_selectedKeyIndex];
            
            ImGui::Text("Selected Keyframe details for: %s.%s", chan.componentName.c_str(), chan.fieldName.c_str());
            
            ImGui::PushItemWidth(150);
            if (ImGui::DragFloat("Key Time", &key.time, 0.01f, 0.0f, activeClip->duration, "%.2fs")) {
                std::sort(chan.keys.begin(), chan.keys.end(), [](const PropertyKeyframe& a, const PropertyKeyframe& b) {
                    return a.time < b.time;
                });
                for (int k = 0; k < static_cast<int>(chan.keys.size()); ++k) {
                    if (chan.keys[k].time == key.time) {
                        s_selectedKeyIndex = k;
                        break;
                    }
                }
            }
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            ImGui::PushItemWidth(250);
            
            if (chan.type == Engine::FieldType::Float) {
                ImGui::DragFloat("Value", &key.value.x, 0.05f);
            } else if (chan.type == Engine::FieldType::Bool) {
                bool bVal = (key.value.x > 0.5f);
                if (ImGui::Checkbox("Value", &bVal)) {
                    key.value.x = bVal ? 1.0f : 0.0f;
                }
            } else if (chan.type == Engine::FieldType::Vec2) {
                ImGui::DragFloat2("Value", &key.value.x, 0.05f);
            } else if (chan.type == Engine::FieldType::Vec3) {
                ImGui::DragFloat3("Value", &key.value.x, 0.05f);
            } else if (chan.type == Engine::FieldType::Vec4) {
                if (chan.fieldName.find("color") != std::string::npos || chan.fieldName.find("Color") != std::string::npos) {
                    ImGui::ColorEdit4("Value", &key.value.x);
                } else {
                    ImGui::DragFloat4("Value", &key.value.x, 0.05f);
                }
            }
            
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Delete Key")) {
                chan.keys.erase(chan.keys.begin() + s_selectedKeyIndex);
                s_selectedKeyIndex = -1;
            }
        } else {
            ImGui::TextDisabled("Select a keyframe diamond in the timeline to inspect/edit its properties.");
        }
    } else {
        ImGui::TextDisabled("Select a property track and keyframe diamond in the timeline to inspect/edit its properties.");
    }

    ImGui::End();
}

void EditorUI::drawNodeGraphDemoWindow() {
    if (!s_openNodeGraphDemoWindow) return;

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Node Graph Demo", &s_openNodeGraphDemoWindow)) {
        ImGui::End();
        return;
    }

    static Engine::NodeGraph graph;
    static bool initialized = false;
    static std::string s_serializedText = "";

    if (!initialized) {
        // Define pin types
        Engine::NodePinType pinFloat{ "float", IM_COL32(100, 200, 100, 255) };
        Engine::NodePinType pinExec{ "exec", IM_COL32(220, 220, 220, 255) };
        Engine::NodePinType pinDialogue{ "dialogue", IM_COL32(200, 100, 200, 255) };
        Engine::NodePinType pinColor{ "color", IM_COL32(200, 180, 50, 255) };

        // 1. Dialogue Node
        graph.registerNodeType("Dialogue", "DialogueSpeech", "Speech Node",
            [pinDialogue](uint32_t nodeId) {
                graph.addInputPin(nodeId, "Prev", pinDialogue);
                graph.addOutputPin(nodeId, "Next", pinDialogue);
            },
            [](Engine::Node& nodeRef) {
                ImGui::Text("Speech text:");
                char textBuf[128] = "";
                strncpy_s(textBuf, nodeRef.customData.c_str(), sizeof(textBuf) - 1);
                if (ImGui::InputText("##speechText", textBuf, sizeof(textBuf))) {
                    nodeRef.customData = textBuf;
                }
            }
        );

        // 2. Dialogue Choice Node
        graph.registerNodeType("Dialogue", "DialogueChoice", "Choice Node",
            [pinDialogue](uint32_t nodeId) {
                graph.addInputPin(nodeId, "Prev", pinDialogue);
                graph.addOutputPin(nodeId, "Option A", pinDialogue);
                graph.addOutputPin(nodeId, "Option B", pinDialogue);
            },
            [](Engine::Node& nodeRef) {
                ImGui::Text("Choice Details:");
                ImGui::TextDisabled("(Extendable custom properties)");
            }
        );

        // 3. Math Add Node
        graph.registerNodeType("Math", "MathAdd", "Add Node",
            [pinFloat](uint32_t nodeId) {
                graph.addInputPin(nodeId, "A", pinFloat);
                graph.addInputPin(nodeId, "B", pinFloat);
                graph.addOutputPin(nodeId, "Result", pinFloat);
            }
        );

        // 4. Conditional Branch Node
        graph.registerNodeType("Logic", "Branch", "Branch Node",
            [pinExec](uint32_t nodeId) {
                graph.addInputPin(nodeId, "In", pinExec);
                graph.addOutputPin(nodeId, "True", pinExec);
                graph.addOutputPin(nodeId, "False", pinExec);
            },
            [](Engine::Node& nodeRef) {
                bool val = (nodeRef.customData == "1");
                if (ImGui::Checkbox("Condition", &val)) {
                    nodeRef.customData = val ? "1" : "0";
                }
            }
        );

        // 5. Color Node
        graph.registerNodeType("Visual", "ColorConstant", "Color Constant",
            [pinColor](uint32_t nodeId) {
                graph.addOutputPin(nodeId, "Out", pinColor);
            },
            [](Engine::Node& nodeRef) {
                static float color[4] = { 0.15f, 0.45f, 0.8f, 1.0f };
                if (!nodeRef.customData.empty()) {
                    std::stringstream ss(nodeRef.customData);
                    ss >> color[0] >> color[1] >> color[2] >> color[3];
                }
                if (ImGui::ColorEdit4("Color", color, ImGuiColorEditFlags_NoInputs)) {
                    std::stringstream ss;
                    ss << color[0] << " " << color[1] << " " << color[2] << " " << color[3];
                    nodeRef.customData = ss.str();
                }
            }
        );

        // Pre-spawn default demonstration nodes
        uint32_t n1Id = graph.createNode("Speech Node", "DialogueSpeech", ImVec2(50.0f, 150.0f));
        Engine::Node* n1 = graph.findNode(n1Id);
        if (n1) {
            n1->customData = "Hello adventurer! Welcome to the realm of Antigravity.";
            n1->customWidgetCallback = [](Engine::Node& nodeRef) {
                ImGui::Text("Speech text:");
                char textBuf[128] = "";
                strncpy_s(textBuf, nodeRef.customData.c_str(), sizeof(textBuf) - 1);
                if (ImGui::InputText("##speechText", textBuf, sizeof(textBuf))) {
                    nodeRef.customData = textBuf;
                }
            };
        }
        graph.addInputPin(n1Id, "Prev", pinDialogue);
        uint32_t n1OutId = graph.addOutputPin(n1Id, "Next", pinDialogue);

        uint32_t n2Id = graph.createNode("Choice Node", "DialogueChoice", ImVec2(320.0f, 120.0f));
        Engine::Node* n2 = graph.findNode(n2Id);
        if (n2) {
            n2->customWidgetCallback = [](Engine::Node& nodeRef) {
                ImGui::Text("Choice Details:");
                ImGui::TextDisabled("(Extendable custom properties)");
            };
        }
        uint32_t n2InId = graph.addInputPin(n2Id, "Prev", pinDialogue);
        graph.addOutputPin(n2Id, "Option A", pinDialogue);
        graph.addOutputPin(n2Id, "Option B", pinDialogue);

        // Pre-link them
        graph.addLink(n1OutId, n2InId);

        s_serializedText = graph.serialize();
        initialized = true;
    }

    // Canvas Toolbar
    if (ImGui::Button("Clear Canvas")) {
        graph.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save JSON")) {
        s_serializedText = graph.serialize();
        std::ofstream out("assets/node_graph_demo.json");
        if (out.is_open()) {
            out << s_serializedText;
            out.close();
            statusMessage = "Saved node graph to assets/node_graph_demo.json";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load JSON")) {
        std::ifstream in("assets/node_graph_demo.json");
        if (in.is_open()) {
            std::stringstream ss;
            ss << in.rdbuf();
            in.close();
            if (graph.deserialize(ss.str())) {
                s_serializedText = ss.str();
                statusMessage = "Loaded node graph from assets/node_graph_demo.json";
            }
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|  R-Click canvas to create nodes. Drag output-to-input slots to link. Drag headers to move.");

    ImGui::Separator();

    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    float canvasW = contentSize.x * 0.70f;
    float jsonW = contentSize.x - canvasW - 8.0f;

    graph.draw("DemoEditor", ImVec2(canvasW, contentSize.y));

    ImGui::SameLine();
    ImGui::BeginChild("##json_output", ImVec2(jsonW, contentSize.y), true);
    ImGui::Text("Serialized JSON Output:");
    ImGui::Separator();
    
    char* jsonPtr = s_serializedText.empty() ? const_cast<char*>("") : &s_serializedText[0];
    ImGui::InputTextMultiline("##json_text", jsonPtr, s_serializedText.size() + 1, ImVec2(-1.0f, -1.0f), ImGuiInputTextFlags_ReadOnly);
    
    ImGui::EndChild();

    ImGui::End();
}

