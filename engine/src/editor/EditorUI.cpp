#include "editor/EditorUI.hpp"
#include "scenes/JSONUtils.hpp"
#include "ufbx.h"
#include "cgltf.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <fstream>

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
#include "ecs/components/EditorCamera.hpp"
#include "ecs/components/AnimationController.hpp"
#include "ecs/components/IKSolver.hpp"
#include "ecs/components/RigidBody.hpp"
#include "ecs/components/Collider.hpp"
#include <functional>
#include "renderer/VulkanRenderer.hpp"
#include "scenes/Scene.hpp"
#include "scenes/SceneManager.hpp"
#include "scenes/SceneSerializer.hpp"
#include "renderer/ResourceManager.hpp"
#include <filesystem>
#include "ImGuizmo.h"

using namespace ImGui;
using namespace std;

struct ImportSettingsMetadata {
    std::string assetPath;
    float scale = 1.0f;
    bool generateNormals = true;
    bool allowMissingPos = false;
    bool forceInPlace = false;
    
    struct AnimMetadata {
        std::string name;
        double duration = 0.0;
    };
    std::vector<AnimMetadata> animations;
    
    struct TexMetadata {
        std::string name;
        size_t index = 0;
        bool hasEmbeddedContent = false;
        size_t contentSize = 0;
    };
    std::vector<TexMetadata> textures;
};

static bool s_openImportSettingsWindow = false;
static bool s_triggerLoadImportSettings = false;
static ImportSettingsMetadata s_importMetadata;
static std::filesystem::path s_importSettingsAssetPath;

static void loadImportSettingsMetadata(const std::filesystem::path& path) {
    s_importMetadata = ImportSettingsMetadata{};
    s_importMetadata.assetPath = path.generic_string();
    
    std::filesystem::path importPath = path.string() + ".import";
    if (std::filesystem::exists(importPath)) {
        try {
            std::ifstream f(importPath);
            if (f.is_open()) {
                std::stringstream ss;
                ss << f.rdbuf();
                std::string content = ss.str();
                float scaleVal = 1.0f;
                if (JSONUtils::extractFloatValue(content, "scale", scaleVal)) {
                    s_importMetadata.scale = scaleVal;
                }
                s_importMetadata.generateNormals = (content.find("\"generateNormals\": true") != std::string::npos || content.find("\"generateNormals\":true") != std::string::npos || content.find("\"generateNormals\": 1") != std::string::npos);
                s_importMetadata.allowMissingPos = (content.find("\"allowMissingPos\": true") != std::string::npos || content.find("\"allowMissingPos\":true") != std::string::npos || content.find("\"allowMissingPos\": 1") != std::string::npos);
                s_importMetadata.forceInPlace = (content.find("\"forceInPlace\": true") != std::string::npos || content.find("\"forceInPlace\":true") != std::string::npos || content.find("\"forceInPlace\": 1") != std::string::npos);
            }
        } catch (...) {}
    }
    
    std::string ext = path.extension().string();
    if (ext == ".fbx" || ext == ".FBX") {
        ufbx_load_opts opts = { 0 };
        ufbx_error error;
        ufbx_scene* scene = ufbx_load_file(path.string().c_str(), &opts, &error);
        if (scene) {
            for (size_t i = 0; i < scene->anim_stacks.count; ++i) {
                ufbx_anim_stack* stack = scene->anim_stacks.data[i];
                std::string name = stack->name.data ? stack->name.data : "UnnamedAnim";
                double dur = stack->time_end - stack->time_begin;
                s_importMetadata.animations.push_back({ name, dur });
            }
            for (size_t i = 0; i < scene->texture_files.count; ++i) {
                ufbx_texture_file& tf = scene->texture_files.data[i];
                std::string name = std::filesystem::path(tf.filename.data ? tf.filename.data : "").filename().string();
                if (name.empty()) name = "texture_" + std::to_string(i);
                s_importMetadata.textures.push_back({ name, i, tf.content.size > 0, tf.content.size });
            }
            ufbx_free_scene(scene);
        }
    } else if (ext == ".gltf" || ext == ".glb") {
        cgltf_options options = {};
        cgltf_data* data = nullptr;
        cgltf_result result = cgltf_parse_file(&options, path.string().c_str(), &data);
        if (result == cgltf_result_success) {
            for (cgltf_size i = 0; i < data->animations_count; ++i) {
                cgltf_animation& anim = data->animations[i];
                std::string name = anim.name ? anim.name : "UnnamedAnim";
                double max_time = 0.0;
                for (cgltf_size j = 0; j < anim.channels_count; ++j) {
                    if (anim.channels[j].sampler && anim.channels[j].sampler->input) {
                        cgltf_accessor* input = anim.channels[j].sampler->input;
                        if (input->count > 0) {
                            cgltf_float outVal = 0.0f;
                            cgltf_accessor_read_float(input, input->count - 1, &outVal, 1);
                            max_time = std::max(max_time, (double)outVal);
                        }
                    }
                }
                s_importMetadata.animations.push_back({ name, max_time });
            }
            cgltf_free(data);
        }
    }
}

static void saveImportSettings() {
    std::string relativePath = s_importMetadata.assetPath + ".import";
    try {
        // Write active copy
        std::ofstream f(relativePath);
        if (f.is_open()) {
            f << "{\n"
              << "  \"scale\": " << s_importMetadata.scale << ",\n"
              << "  \"generateNormals\": " << (s_importMetadata.generateNormals ? "true" : "false") << ",\n"
              << "  \"allowMissingPos\": " << (s_importMetadata.allowMissingPos ? "true" : "false") << ",\n"
              << "  \"forceInPlace\": " << (s_importMetadata.forceInPlace ? "true" : "false") << "\n"
              << "}\n";
            f.close();
        }
        
        // Write source copy
        std::filesystem::path sourceBase("../../../sandbox_game");
        if (std::filesystem::exists(sourceBase / "assets")) {
            std::filesystem::path sourcePath = sourceBase / relativePath;
            if (!sourcePath.parent_path().empty()) {
                std::filesystem::create_directories(sourcePath.parent_path());
            }
            std::ofstream fSrc(sourcePath);
            if (fSrc.is_open()) {
                fSrc << "{\n"
                     << "  \"scale\": " << s_importMetadata.scale << ",\n"
                     << "  \"generateNormals\": " << (s_importMetadata.generateNormals ? "true" : "false") << ",\n"
                     << "  \"allowMissingPos\": " << (s_importMetadata.allowMissingPos ? "true" : "false") << ",\n"
                     << "  \"forceInPlace\": " << (s_importMetadata.forceInPlace ? "true" : "false") << "\n"
                     << "}\n";
                fSrc.close();
            }
        }
    } catch (...) {}
}

static bool writeExtractedFile(const std::string& relativePath, const void* data, size_t size) {
    // 1. Write to active run folder (makes it show up in the editor instantly)
    std::filesystem::path activePath(relativePath);
    if (!activePath.parent_path().empty()) {
        std::filesystem::create_directories(activePath.parent_path());
    }
    std::ofstream activeOut(activePath, std::ios::binary);
    if (!activeOut.is_open()) return false;
    activeOut.write(reinterpret_cast<const char*>(data), size);
    activeOut.close();

    // 2. Write to source folder if it exists (makes it persistent across builds/cleans)
    std::filesystem::path sourceBase("../../../sandbox_game");
    if (std::filesystem::exists(sourceBase / "assets")) {
        std::filesystem::path sourcePath = sourceBase / relativePath;
        if (!sourcePath.parent_path().empty()) {
            std::filesystem::create_directories(sourcePath.parent_path());
        }
        std::ofstream sourceOut(sourcePath, std::ios::binary);
        if (sourceOut.is_open()) {
            sourceOut.write(reinterpret_cast<const char*>(data), size);
            sourceOut.close();
        }
    }
    return true;
}

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

    // --- Setup Custom Engine Theme (Charcoal Dark Theme) ---
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    
    // Color Palette
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.85f, 0.85f, 0.87f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.12f, 0.12f, 0.14f, 1.00f); // Unreal Charcoal
    colors[ImGuiCol_ChildBg]                = ImVec4(0.10f, 0.10f, 0.12f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.14f, 0.14f, 0.16f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.36f, 0.36f, 0.40f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 0.48f, 0.80f, 1.00f); // Sleek Accent Blue
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]        = ImVec4(0.00f, 0.58f, 0.90f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.00f, 0.38f, 0.70f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.26f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.00f, 0.38f, 0.70f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.00f, 0.38f, 0.70f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

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
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
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
    handleViewportPicking();
    
    // 7. Float Import Settings panel
    drawImportSettingsWindow();
}

/**
 * @brief Draws Gizmo handle overlay to translate/rotate selected entities.
 */
void EditorUI::drawGizmo()
{
    if (!hasSelection || editorMode.flyMode)
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

    glm::mat4 parentWorldMatrix = glm::mat4(1.0f);
    bool hasParent = false;
    if (auto* hierarchy = registry.get<HierarchyComponent>(selectedEntity)) {
        if (hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent)) {
            parentWorldMatrix = getEntityWorldMatrix(hierarchy->parent);
            hasParent = true;
        }
    }

    glm::mat4 worldMatrix = parentWorldMatrix * transform->matrix();

    // Safety guard: do not invoke ImGuizmo if worldMatrix contains NaNs/Infs
    bool hasNaN = false;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float val = worldMatrix[col][row];
            if (std::isnan(val) || std::isinf(val)) {
                hasNaN = true;
                break;
            }
        }
        if (hasNaN) break;
    }

    if (hasNaN) {
        // Reset transform components to clean states to recover from NaN
        transform->position = glm::vec3(0.0f);
        transform->rotation = glm::vec3(0.0f);
        transform->scale = glm::vec3(1.0f);
        worldMatrix = parentWorldMatrix * transform->matrix();
    }

    ImGuizmo::Manipulate(
        &view[0][0],
        &proj[0][0],
        ImGuizmo::TRANSLATE,
        ImGuizmo::LOCAL,
        &worldMatrix[0][0]
    );

    if (ImGuizmo::IsUsing()) {
        bool worldNaN = false;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float val = worldMatrix[col][row];
                if (std::isnan(val) || std::isinf(val)) {
                    worldNaN = true;
                    break;
                }
            }
            if (worldNaN) break;
        }

        if (!worldNaN) {
            glm::mat4 newLocalMatrix = worldMatrix;
            if (hasParent) {
                float det = glm::determinant(parentWorldMatrix);
                if (std::abs(det) > 1e-5f) {
                    newLocalMatrix = glm::inverse(parentWorldMatrix) * worldMatrix;
                }
            }
            decomposeMatrixToTransform(newLocalMatrix, *transform);
        }

        if (auto* rb = registry.get<RigidBodyComponent>(selectedEntity)) {
            rb->velocity = glm::vec3(0.0f);
            rb->force = glm::vec3(0.0f);
        }
    }
}

/**
 * @brief Decomposes raw matrix transform parameters to target transform object.
 * @param mat Target source matrix.
 * @param t Target transform destination reference.
 */
void EditorUI::decomposeMatrixToTransform(const glm::mat4& mat, Transform& t)
{
    // Safety check BEFORE calling ImGuizmo decomposition to prevent infinite loops in ImGuizmo
    bool hasNaNOrInf = false;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float val = mat[col][row];
            if (std::isnan(val) || std::isinf(val)) {
                hasNaNOrInf = true;
                break;
            }
        }
        if (hasNaNOrInf) break;
    }

    if (hasNaNOrInf) {
        t.position = glm::vec3(0.0f);
        t.rotation = glm::vec3(0.0f);
        t.scale = glm::vec3(1.0f);
        return;
    }

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

    // Keep rotation and scale unchanged to prevent decomposition drift/erratic rotation when using translation gizmo
    /*
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
    */

    // Safety guards to prevent NaN/Inf propagation to the C++ transform state
    if (std::isnan(t.position.x) || std::isinf(t.position.x) ||
        std::isnan(t.position.y) || std::isinf(t.position.y) ||
        std::isnan(t.position.z) || std::isinf(t.position.z)) {
        t.position = glm::vec3(0.0f);
    }
    if (std::isnan(t.rotation.x) || std::isinf(t.rotation.x) ||
        std::isnan(t.rotation.y) || std::isinf(t.rotation.y) ||
        std::isnan(t.rotation.z) || std::isinf(t.rotation.z)) {
        t.rotation = glm::vec3(0.0f);
    }
    if (std::isnan(t.scale.x) || std::isinf(t.scale.x) || t.scale.x < 1e-4f ||
        std::isnan(t.scale.y) || std::isinf(t.scale.y) || t.scale.y < 1e-4f ||
        std::isnan(t.scale.z) || std::isinf(t.scale.z) || t.scale.z < 1e-4f) {
        t.scale = glm::vec3(1.0f);
    }
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

/**
 * @brief Inspector panel routing control fields based on components.
 */
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
    drawRigidBodyEditor();
    drawColliderEditor();
    drawGridEditor();
    drawCameraEditor();

    ImGui::Separator();
    if (ImGui::Button("+ Add Component", ImVec2(-1, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!registry.has<Material>(selectedEntity) && ImGui::MenuItem("Material")) {
            glm::vec4 color(1.0f);
            registry.emplace<Material>(selectedEntity, Material{ color });
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
            statusMessage = "Added Skeleton component.";
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
        if (!registry.has<RigidBodyComponent>(selectedEntity) && ImGui::MenuItem("RigidBody")) {
            registry.emplace<RigidBodyComponent>(selectedEntity, RigidBodyComponent{});
            statusMessage = "Added RigidBody component.";
        }
        if (!registry.has<ColliderComponent>(selectedEntity) && ImGui::MenuItem("Collider")) {
            registry.emplace<ColliderComponent>(selectedEntity, ColliderComponent{});
            statusMessage = "Added Collider component.";
        }
        ImGui::EndPopup();
    }

    PopStyleVar(2);

    End();
}

/**
 * @brief Renders details panel of raycasts, metrics, and picking.
 */
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
    Checkbox("Show Colliders", &showColliders);

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
    if (!material) {
        return;
    }

    bool open = CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##Material", ImVec2(24, 20))) {
        registry.remove<Material>(selectedEntity);
        statusMessage = "Removed Material component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) {
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

    if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = AcceptDragDropPayload("DND_PAYLOAD_ASSET_PATH")) {
            const char* droppedPath = (const char*)payload->Data;
            std::string pathStr(droppedPath);
            auto ext = std::filesystem::path(pathStr).extension().string();
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
                material->texturePath = pathStr;
                if (auto* tex = renderer.resourceManager->loadTexture(pathStr, renderer)) {
                    material->descriptorSet = tex->descriptorSet;
                    statusMessage = "Assigned texture via drag-and-drop: " + pathStr;
                }
            } else {
                statusMessage = "Error: Dropped asset is not a valid texture file.";
            }
        }
        EndDragDropTarget();
    }
}

/**
 * @brief Renders inline controls for editing Mesh geometry and glTF paths.
 */
void EditorUI::drawMeshEditor() {
    Mesh* mesh = registry.get<Mesh>(selectedEntity);
    if (!mesh || registry.has<Grid>(selectedEntity)) {
        return;
    }

    bool open = CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##Mesh", ImVec2(24, 20))) {
        registry.remove<Mesh>(selectedEntity);
        statusMessage = "Removed Mesh component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) {
        return;
    }

    char gltfBuf[256]{};
    snprintf(gltfBuf, sizeof(gltfBuf), "%s", mesh->gltfPath.c_str());
    if (InputText("glTF Path", gltfBuf, sizeof(gltfBuf))) {
        mesh->gltfPath = gltfBuf;
    }

    if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = AcceptDragDropPayload("DND_PAYLOAD_ASSET_PATH")) {
            const char* droppedPath = (const char*)payload->Data;
            std::string pathStr(droppedPath);
            auto ext = std::filesystem::path(pathStr).extension().string();
            if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".FBX") {
                try {
                    int primCount = renderer.resourceManager->getMeshPrimitiveCount(pathStr);
                    if (primCount > 1) {
                        std::string baseName = "Model";
                        if (auto* nameComp = registry.get<Name>(selectedEntity)) {
                            baseName = nameComp->value;
                        }
                        
                        mesh->gltfPath = pathStr;
                        mesh->primitiveIndex = 0;
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

                        Scene* currentScene = sceneManager.getCurrentScene();
                        for (int i = 1; i < primCount; ++i) {
                            Entity child = registry.create();
                            if (child.getId() != Entity::INVALID_ENTITY) {
                                registry.emplace<Transform>(child, Transform{});
                                registry.emplace<PrimitiveType>(child, PrimitiveType{ PrimitiveKind::Cube });
                                registry.emplace<HierarchyComponent>(child, HierarchyComponent{ selectedEntity });
                                
                                Mesh childMesh{};
                                childMesh.gltfPath = pathStr;
                                childMesh.primitiveIndex = i;
                                Mesh loadedChild = renderer.resourceManager->loadMesh(pathStr, renderer, i);
                                childMesh.vertices = loadedChild.vertices;
                                childMesh.indices = loadedChild.indices;
                                childMesh.vertexBuffer = loadedChild.vertexBuffer;
                                childMesh.indexBuffer = loadedChild.indexBuffer;
                                childMesh.id = loadedChild.id;
                                childMesh.nodeName = loadedChild.nodeName;

                                std::string childName = loadedChild.nodeName.empty() ? (baseName + "_part" + std::to_string(i)) : loadedChild.nodeName;
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
                            }
                        }
                        statusMessage = "Dropped & loaded split glTF: " + pathStr;
                    } else {
                        mesh->gltfPath = pathStr;
                        mesh->primitiveIndex = -1;
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
                        statusMessage = "Dropped & loaded glTF: " + pathStr;
                    }
                } catch (const std::exception& e) {
                    statusMessage = std::string("Failed to load dropped model: ") + e.what();
                }
            } else {
                statusMessage = "Error: Dropped asset is not a glTF model.";
            }
        }
        EndDragDropTarget();
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
    static bool s_openCreateFolderPopup;
    static bool s_openCreateScenePopup;
    static bool s_openRenamePopup;

    // ---- Toolbar ----
    if (Button("+ New Folder")) {
        s_createFolderParentPath = "assets";
        s_createFolderBuffer[0] = '\0';
        OpenPopup("CreateFolderPopup");
    }
    SameLine();
    if (Button("+ New Scene")) {
        s_createSceneParentPath = "assets";
        s_createSceneBuffer[0] = '\0';
        OpenPopup("CreateScenePopup");
    }
    SameLine();
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
                    if (MenuItem("Create Subfolder")) {
                        s_createFolderParentPath = entry.path();
                        s_createFolderBuffer[0] = '\0';
                        s_openCreateFolderPopup = true;
                    }
                    if (MenuItem("Create New Scene")) {
                        s_createSceneParentPath = entry.path();
                        s_createSceneBuffer[0] = '\0';
                        s_openCreateScenePopup = true;
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

                std::string prefix = "  ";
                if (isModel) prefix = "[] ";
                else if (isTexture) prefix = "[] ";
                else if (isPrefab) prefix = "[] ";
                else if (isScene) prefix = "[] ";

                std::string labelStr = prefix + name + "##" + pathStr;
                Selectable(labelStr.c_str(), false, ImGuiSelectableFlags_AllowOverlap);

                // Right click context menu on files (must follow Selectable immediately to bind correctly)
                if (BeginPopupContextItem(pathStr.c_str())) {
                    TextDisabled("File: %s", name.c_str());
                    Separator();
                    if (isModel) {
                        if (MenuItem("Import Settings...")) {
                            s_importSettingsAssetPath = entry.path();
                            s_triggerLoadImportSettings = true;
                        }
                        Separator();
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
                            
                            std::filesystem::path activeImport = activePath.string() + ".import";
                            std::filesystem::path sourceImport = sourcePath.string() + ".import";
                            
                            std::filesystem::remove(activePath);
                            if (std::filesystem::exists(sourcePath)) {
                                std::filesystem::remove(sourcePath);
                            }
                            if (std::filesystem::exists(activeImport)) {
                                std::filesystem::remove(activeImport);
                            }
                            if (std::filesystem::exists(sourceImport)) {
                                std::filesystem::remove(sourceImport);
                            }
                            statusMessage = "Deleted file: " + name;
                        } catch (const std::exception& e) {
                            statusMessage = std::string("Failed to delete file: ") + e.what();
                        }
                    }
                    PopStyleColor();
                    EndPopup();
                }

                if (isScene) {
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        Scene* currentScene = sceneManager.getCurrentScene();
                        if (currentScene && currentScene->loadFromFile(pathStr)) {
                            scenePath = pathStr;
                            statusMessage = "Scene loaded from " + pathStr;
                            hasSelection = false;
                            selectedEntity = Entity();
                            renameBuffer.clear();
                        } else {
                            statusMessage = "Failed to load scene from " + pathStr;
                        }
                    }
                }

                if (BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    SetDragDropPayload("DND_PAYLOAD_ASSET_PATH", pathStr.c_str(), pathStr.size() + 1);
                    Text("Dragging %s", name.c_str());
                    EndDragDropSource();
                }

                // Use/Instantiate buttons next to files
                if (isModel) {
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
                } else if (isTexture) {
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
                } else if (isPrefab) {
                    SameLine();
                    PushID(pathStr.c_str());
                    if (Button("Instantiate")) {
                        Scene* currentScene = sceneManager.getCurrentScene();
                        if (currentScene) {
                            SceneSerializer serializer(registry, renderer);
                            std::vector<Entity> loadedEntities;
                            Entity parent = hasSelection ? selectedEntity : Entity();
                            Entity root = serializer.deserializePrefab(pathStr, loadedEntities, parent);
                            if (root.getId() != Entity::INVALID_ENTITY) {
                                for (Entity e : loadedEntities) {
                                    currentScene->trackEntity(e);
                                }
                                selectedEntity = root;
                                hasSelection = true;
                                if (auto* n = registry.get<Name>(root)) {
                                    renameBuffer = n->value;
                                }
                                statusMessage = "Instantiated prefab: " + entry.path().filename().string();
                            } else {
                                statusMessage = "Failed to instantiate prefab.";
                            }
                        }
                    }
                    PopID();
                } else if (isScene) {
                    SameLine();
                    PushID(pathStr.c_str());
                    if (Button("Load")) {
                        Scene* currentScene = sceneManager.getCurrentScene();
                        if (currentScene && currentScene->loadFromFile(pathStr)) {
                            scenePath = pathStr;
                            statusMessage = "Scene loaded from " + pathStr;
                            hasSelection = false;
                            selectedEntity = Entity();
                            renameBuffer.clear();
                        } else {
                            statusMessage = "Failed to load scene.";
                        }
                    }
                    PopID();
                }


            }
        }
    };

    // Draw the asset directory tree recursively starting at "assets"
    ImGui::BeginChild("AssetTreeChild", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    drawDirectoryNode("assets");
    ImGui::EndChild();

    if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = AcceptDragDropPayload("DND_PAYLOAD_ASSET_PATH")) {
            const char* srcPath = (const char*)payload->Data;
            std::filesystem::path src(srcPath);
            std::filesystem::path dest = std::filesystem::path("assets") / src.filename();
            
            std::string srcStr = src.generic_string();
            std::string destStr = dest.generic_string();
            if (srcStr == destStr || destStr.rfind(srcStr + "/", 0) == 0) {
                statusMessage = "Cannot move a folder into itself or its subfolder.";
            } else {
                try {
                    std::filesystem::rename(src, dest);
                    statusMessage = "Moved " + src.filename().string() + " to assets root";
                } catch (const std::exception& e) {
                    statusMessage = std::string("Failed to move: ") + e.what();
                }
            }
        }
        EndDragDropTarget();
    }

    if (s_openCreateFolderPopup) {
        OpenPopup("CreateFolderPopup");
        s_openCreateFolderPopup = false;
    }
    if (s_openCreateScenePopup) {
        OpenPopup("CreateScenePopup");
        s_openCreateScenePopup = false;
    }
    if (s_openRenamePopup) {
        OpenPopup("RenameAssetPopup");
        s_openRenamePopup = false;
    }

    // ---- Popups ----
    if (BeginPopup("RenameAssetPopup")) {
        Text("Rename: %s", s_renameTargetPath.filename().string().c_str());
        InputText("New Name", s_renameBuffer, sizeof(s_renameBuffer));
        if (Button("OK", ImVec2(120, 0))) {
            if (strlen(s_renameBuffer) > 0) {
                auto newPath = s_renameTargetPath.parent_path() / s_renameBuffer;
                try {
                    std::filesystem::rename(s_renameTargetPath, newPath);
                    statusMessage = "Renamed asset successfully.";
                    CloseCurrentPopup();
                } catch (const std::exception& e) {
                    statusMessage = std::string("Failed to rename: ") + e.what();
                }
            }
        }
        SameLine();
        if (Button("Cancel", ImVec2(120, 0))) {
            CloseCurrentPopup();
        }
        EndPopup();
    }

    if (BeginPopup("CreateFolderPopup")) {
        Text("Create Folder under: %s", s_createFolderParentPath.generic_string().c_str());
        InputText("Folder Name", s_createFolderBuffer, sizeof(s_createFolderBuffer));
        if (Button("Create", ImVec2(120, 0))) {
            if (strlen(s_createFolderBuffer) > 0) {
                auto newDir = s_createFolderParentPath / s_createFolderBuffer;
                try {
                    std::filesystem::create_directories(newDir);
                    statusMessage = "Folder created successfully.";
                    CloseCurrentPopup();
                } catch (const std::exception& e) {
                    statusMessage = std::string("Failed to create folder: ") + e.what();
                }
            }
        }
        SameLine();
        if (Button("Cancel", ImVec2(120, 0))) {
            CloseCurrentPopup();
        }
        EndPopup();
    }

    if (BeginPopup("CreateScenePopup")) {
        Text("Create Scene under: %s", s_createSceneParentPath.generic_string().c_str());
        InputText("Scene Name", s_createSceneBuffer, sizeof(s_createSceneBuffer));
        if (Button("Create", ImVec2(120, 0))) {
            if (strlen(s_createSceneBuffer) > 0) {
                std::string sceneName = s_createSceneBuffer;
                if (sceneName.length() < 5 || sceneName.substr(sceneName.length() - 5) != ".json") {
                    sceneName += ".json";
                }
                auto newScenePath = s_createSceneParentPath / sceneName;
                try {
                    std::ofstream out(newScenePath);
                    out << "{\n  \"scene\": \"" << s_createSceneBuffer << "\",\n  \"entities\": []\n}\n";
                    out.close();
                    statusMessage = "Scene created successfully: " + sceneName;
                    CloseCurrentPopup();
                } catch (const std::exception& e) {
                    statusMessage = std::string("Failed to create scene: ") + e.what();
                }
            }
        }
        SameLine();
        if (Button("Cancel", ImVec2(120, 0))) {
            CloseCurrentPopup();
        }
        EndPopup();
    }

    End();
}

/**
 * @brief Drag inputs for grid dimensions.
 */
void EditorUI::drawGridEditor() {
    Grid* grid = registry.get<Grid>(selectedEntity);
    if (!grid) {
        return;
    }

    bool open = CollapsingHeader("Grid", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##Grid", ImVec2(24, 20))) {
        registry.remove<Grid>(selectedEntity);
        statusMessage = "Removed Grid component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) {
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
    if (!camera) {
        return;
    }

    bool open = CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##Camera", ImVec2(24, 20))) {
        registry.remove<Camera>(selectedEntity);
        statusMessage = "Removed Camera component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) {
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
    if (!skeleton) {
        return;
    }

    bool open = CollapsingHeader("Skeleton", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##Skeleton", ImVec2(24, 20))) {
        registry.remove<SkeletonComponent>(selectedEntity);
        statusMessage = "Removed Skeleton component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) {
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
    AnimatorComponent* animator = registry.get<AnimatorComponent>(selectedEntity);
    if (!animator) {
        return;
    }

    bool open = CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##Animator", ImVec2(24, 20))) {
        registry.remove<AnimatorComponent>(selectedEntity);
        statusMessage = "Removed Animator component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) {
        return;
    }

    if (auto* hierarchy = registry.get<HierarchyComponent>(selectedEntity)) {
        if (hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent) && registry.has<AnimatorComponent>(hierarchy->parent)) {
            TextUnformatted("Animation is driven by parent entity.");
            return;
        }
    }

    // Binary anim loader/saver utility controls
    static Entity lastSelectedEntity{};
    static char animPathBuf[256] = "";
    if (selectedEntity != lastSelectedEntity) {
        lastSelectedEntity = selectedEntity;
        strncpy_s(animPathBuf, animator->loadedAnimPath.c_str(), sizeof(animPathBuf) - 1);
    }

    InputText("Anim Path", animPathBuf, sizeof(animPathBuf));
    if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = AcceptDragDropPayload("DND_PAYLOAD_ASSET_PATH")) {
            const char* droppedPath = (const char*)payload->Data;
            std::string pathStr(droppedPath);
            auto ext = std::filesystem::path(pathStr).extension().string();
            if (ext == ".fbx" || ext == ".FBX" || ext == ".anim" || ext == ".gltf" || ext == ".glb") {
                strncpy_s(animPathBuf, pathStr.c_str(), sizeof(animPathBuf) - 1);
                
                SkeletonComponent* skeleton = registry.get<SkeletonComponent>(selectedEntity);
                if (!skeleton) {
                    SkeletonComponent newSkel{};
                    registry.emplace<SkeletonComponent>(selectedEntity, std::move(newSkel));
                    skeleton = registry.get<SkeletonComponent>(selectedEntity);
                }
                if (renderer.resourceManager->loadSkeletonAndAnimations(pathStr, *skeleton, *animator)) {
                    animator->loadedAnimPath = pathStr;
                    if (auto* material = registry.get<Material>(selectedEntity)) {
                        bool hasSkin = entityHasSkin(registry, selectedEntity);
                        PipelineHandle pipeline = renderer.createPipelineForShaders(
                            hasSkin ? "build/shaders/skinned.vert.spv" : "build/shaders/unlit.vert.spv",
                            "build/shaders/unlit.frag.spv"
                        );
                        material->pipeline = pipeline.pipeline;
                        material->pipelineLayout = pipeline.layout;
                    }
                    statusMessage = "Loaded animation successfully via drag & drop.";
                } else {
                    statusMessage = "Failed to load animation via drag & drop.";
                }
            }
        }
        EndDragDropTarget();
    }
    
    if (Button("Load Anim")) {
        std::string pathStr(animPathBuf);
        SkeletonComponent* skeleton = registry.get<SkeletonComponent>(selectedEntity);
        if (!skeleton) {
            SkeletonComponent newSkel{};
            registry.emplace<SkeletonComponent>(selectedEntity, std::move(newSkel));
            skeleton = registry.get<SkeletonComponent>(selectedEntity);
        }
        if (renderer.resourceManager->loadSkeletonAndAnimations(pathStr, *skeleton, *animator)) {
            animator->loadedAnimPath = pathStr;
            if (auto* material = registry.get<Material>(selectedEntity)) {
                bool hasSkin = entityHasSkin(registry, selectedEntity);
                PipelineHandle pipeline = renderer.createPipelineForShaders(
                    hasSkin ? "build/shaders/skinned.vert.spv" : "build/shaders/unlit.vert.spv",
                    "build/shaders/unlit.frag.spv"
                );
                material->pipeline = pipeline.pipeline;
                material->pipelineLayout = pipeline.layout;
            }
            statusMessage = "Loaded animation successfully.";
        } else {
            statusMessage = "Failed to load animation.";
        }
    }
    
    SameLine();
    if (Button("Save Binary (.anim)")) {
        SkeletonComponent* skeleton = registry.get<SkeletonComponent>(selectedEntity);
        if (skeleton) {
            std::filesystem::create_directories("sandbox_game/assets/animations");
            std::string savePath = "sandbox_game/assets/animations/model.anim";
            if (auto* nameComp = registry.get<Name>(selectedEntity)) {
                savePath = "sandbox_game/assets/animations/" + nameComp->value + ".anim";
            }
            if (renderer.resourceManager->saveBinarySkeletonAndAnimations(savePath, *skeleton, *animator)) {
                statusMessage = "Saved binary animation to " + savePath;
            } else {
                statusMessage = "Failed to save binary animation.";
            }
        } else {
            statusMessage = "No skeleton to save.";
        }
    }
    
    Separator();
    
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
}

void EditorUI::drawHierarchyEditor() {
    HierarchyComponent* hierarchy = registry.get<HierarchyComponent>(selectedEntity);
    if (!hierarchy) {
        return;
    }

    bool open = CollapsingHeader("Hierarchy Link", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##Hierarchy", ImVec2(24, 20))) {
        registry.remove<HierarchyComponent>(selectedEntity);
        statusMessage = "Removed Hierarchy component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) {
        return;
    }

    std::string parentNameStr = "None";
    if (hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent)) {
        if (auto* parentNameComp = registry.get<Name>(hierarchy->parent)) {
            parentNameStr = parentNameComp->value + " (" + std::to_string(hierarchy->parent.getId()) + ")";
        } else {
            parentNameStr = "Entity " + std::to_string(hierarchy->parent.getId());
        }
    }
    Text("Parent: %s", parentNameStr.c_str());

    // Option to clear parent
    if (hierarchy->parent.getId() != Entity::INVALID_ENTITY) {
        if (Button("Clear Parent")) {
            hierarchy->parent = Entity();
            statusMessage = "Cleared parent entity link.";
        }
    }

    // List other entities to set as parent
    if (TreeNode("Choose Parent")) {
        for (auto [otherEntity, nameComp] : registry.view<Name>()) {
            if (otherEntity == selectedEntity) continue;
            
            // Prevent cyclic parenting
            bool isDescendant = false;
            Entity check = otherEntity;
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
            if (isDescendant) continue;

            if (Selectable((nameComp.value + "##parentChoice" + std::to_string(otherEntity.getId())).c_str())) {
                hierarchy->parent = otherEntity;
                statusMessage = "Linked parent entity.";
            }
        }
        TreePop();
    }
}

void EditorUI::drawIKSolverEditor() {
    IKSolverComponent* ik = registry.get<IKSolverComponent>(selectedEntity);
    if (!ik) {
        return;
    }

    bool open = CollapsingHeader("IK Solver", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##IKSolver", ImVec2(24, 20))) {
        registry.remove<IKSolverComponent>(selectedEntity);
        statusMessage = "Removed IK Solver component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) {
        return;
    }

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

void EditorUI::drawAnimationControllerEditor() {
    AnimationControllerComponent* controller = registry.get<AnimationControllerComponent>(selectedEntity);
    if (!controller) {
        return;
    }

    bool open = CollapsingHeader("Animation Controller", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##AnimController", ImVec2(24, 20))) {
        registry.remove<AnimationControllerComponent>(selectedEntity);
        statusMessage = "Removed Animation Controller component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) {
        return;
    }

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
    if (auto* animator = registry.get<AnimatorComponent>(selectedEntity)) {
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
    } else {
        TextDisabled("Add an Animator component to load state machines.");
    }
}

void EditorUI::drawRigidBodyEditor() {
    RigidBodyComponent* rb = registry.get<RigidBodyComponent>(selectedEntity);
    if (!rb) return;

    bool open = CollapsingHeader("RigidBody", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##RigidBody", ImVec2(24, 20))) {
        registry.remove<RigidBodyComponent>(selectedEntity);
        statusMessage = "Removed RigidBody component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) return;

    // Type selection
    const char* types[] = { "Dynamic", "Static" };
    int currentType = (rb->type == RigidBodyType::Static) ? 1 : 0;
    if (Combo("Body Type", &currentType, types, 2)) {
        rb->type = (currentType == 1) ? RigidBodyType::Static : RigidBodyType::Dynamic;
    }

    if (DragFloat("Mass", &rb->mass, 0.05f, 0.001f, 1000.0f)) {
        if (rb->mass < 0.001f) rb->mass = 0.001f;
    }
    DragFloat3("Velocity", &rb->velocity[0], 0.05f);
    DragFloat3("Force Accumulator", &rb->force[0], 0.05f);
    DragFloat("Gravity Scale", &rb->gravityScale, 0.05f, -10.0f, 10.0f);
    if (SliderFloat("Restitution", &rb->restitution, 0.0f, 1.0f)) {
        if (rb->restitution < 0.0f) rb->restitution = 0.0f;
        if (rb->restitution > 1.0f) rb->restitution = 1.0f;
    }
    if (SliderFloat("Friction", &rb->friction, 0.0f, 1.0f)) {
        if (rb->friction < 0.0f) rb->friction = 0.0f;
        if (rb->friction > 1.0f) rb->friction = 1.0f;
    }
    if (DragFloat("Linear Drag", &rb->linearDrag, 0.01f, 0.0f, 10.0f)) {
        if (rb->linearDrag < 0.0f) rb->linearDrag = 0.0f;
    }
    if (DragFloat("Angular Drag", &rb->angularDrag, 0.01f, 0.0f, 10.0f)) {
        if (rb->angularDrag < 0.0f) rb->angularDrag = 0.0f;
    }
}

void EditorUI::drawColliderEditor() {
    ColliderComponent* col = registry.get<ColliderComponent>(selectedEntity);
    if (!col) return;

    bool open = CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen);
    SameLine(ImGui::GetWindowWidth() - 40.0f);
    PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    if (Button("X##Collider", ImVec2(24, 20))) {
        registry.remove<ColliderComponent>(selectedEntity);
        statusMessage = "Removed Collider component.";
        PopStyleColor(2);
        return;
    }
    PopStyleColor(2);

    if (!open) return;

    // Shape Selection
    const char* shapes[] = { "Sphere", "AABB", "OBB" };
    int currentShape = 1;
    if (col->shape == ColliderShape::Sphere) currentShape = 0;
    else if (col->shape == ColliderShape::OBB) currentShape = 2;

    if (Combo("Shape", &currentShape, shapes, 3)) {
        if (currentShape == 0) col->shape = ColliderShape::Sphere;
        else if (currentShape == 2) col->shape = ColliderShape::OBB;
        else col->shape = ColliderShape::AABB;
    }

    if (col->shape == ColliderShape::Sphere) {
        if (DragFloat("Radius", &col->radius, 0.05f, 0.001f, 100.0f)) {
            if (col->radius < 0.001f) col->radius = 0.001f;
        }
    } else {
        if (DragFloat3("Half-Extents", &col->extents[0], 0.05f, 0.001f, 100.0f)) {
            if (col->extents.x < 0.001f) col->extents.x = 0.001f;
            if (col->extents.y < 0.001f) col->extents.y = 0.001f;
            if (col->extents.z < 0.001f) col->extents.z = 0.001f;
        }
    }

    DragFloat3("Center Offset", &col->offset[0], 0.05f);
}

glm::mat4 EditorUI::getEntityWorldMatrix(Entity entity, int depth) {
    if (depth > 100) return glm::mat4(1.0f);
    glm::mat4 m = glm::mat4(1.0f);
    if (auto* t = registry.get<Transform>(entity)) {
        m = t->matrix();
    }
    if (auto* h = registry.get<HierarchyComponent>(entity)) {
        if (h->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(h->parent)) {
            m = getEntityWorldMatrix(h->parent, depth + 1) * m;
        }
    }
    return m;
}

void EditorUI::drawColliderDebugOverlay() {
    if (!showColliders) return;

    ImGuiIO& io = ImGui::GetIO();
    glm::mat4 viewProj = renderer.getActiveCameraViewProj();

    auto projectToScreen = [&](const glm::vec3& worldPos, ImVec2& screenPos) -> bool {
        glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);
        if (clipPos.w < 0.0001f) return false;
        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
        screenPos.x = (ndc.x + 1.0f) * 0.5f * io.DisplaySize.x;
        screenPos.y = (ndc.y + 1.0f) * 0.5f * io.DisplaySize.y;
        return true;
    };

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    for (auto [entity, transform, col] : registry.view<Transform, ColliderComponent>()) {
        glm::mat4 worldM = getEntityWorldMatrix(entity);
        ImU32 color = (hasSelection && entity == selectedEntity) ? ImColor(0, 255, 0, 255) : ImColor(255, 120, 0, 200);

        if (col.shape == ColliderShape::Sphere) {
            const int segments = 24;
            float radius = col.radius;
            glm::vec3 center = glm::vec3(worldM * glm::vec4(col.offset, 1.0f));

            auto drawRing = [&](const glm::vec3& u, const glm::vec3& v) {
                ImVec2 prevScreen;
                bool prevValid = false;
                for (int step = 0; step <= segments; ++step) {
                    float angle = (float)step / (float)segments * 2.0f * 3.14159265f;
                    glm::vec3 offset = radius * (std::cos(angle) * u + std::sin(angle) * v);
                    ImVec2 currScreen;
                    if (projectToScreen(center + offset, currScreen)) {
                        if (prevValid) {
                            drawList->AddLine(prevScreen, currScreen, color, 1.5f);
                        }
                        prevScreen = currScreen;
                        prevValid = true;
                    } else {
                        prevValid = false;
                    }
                }
            };

            glm::vec3 axisX(1.0f, 0.0f, 0.0f);
            glm::vec3 axisY(0.0f, 1.0f, 0.0f);
            glm::vec3 axisZ(0.0f, 0.0f, 1.0f);

            drawRing(axisX, axisY);
            drawRing(axisX, axisZ);
            drawRing(axisY, axisZ);

        } else if (col.shape == ColliderShape::AABB) {
            glm::vec3 center = glm::vec3(worldM * glm::vec4(col.offset, 1.0f));
            glm::vec3 worldCorners[8] = {
                center + col.extents * glm::vec3(-1, -1, -1),
                center + col.extents * glm::vec3(1, -1, -1),
                center + col.extents * glm::vec3(1, 1, -1),
                center + col.extents * glm::vec3(-1, 1, -1),
                center + col.extents * glm::vec3(-1, -1, 1),
                center + col.extents * glm::vec3(1, -1, 1),
                center + col.extents * glm::vec3(1, 1, 1),
                center + col.extents * glm::vec3(-1, 1, 1)
            };

            ImVec2 screenCorners[8];
            bool valid[8];

            for (int k = 0; k < 8; ++k) {
                valid[k] = projectToScreen(worldCorners[k], screenCorners[k]);
            }

            auto drawEdge = [&](int i, int j) {
                if (valid[i] && valid[j]) {
                    drawList->AddLine(screenCorners[i], screenCorners[j], color, 1.5f);
                }
            };

            drawEdge(0, 1); drawEdge(1, 2); drawEdge(2, 3); drawEdge(3, 0);
            drawEdge(4, 5); drawEdge(5, 6); drawEdge(6, 7); drawEdge(7, 4);
            drawEdge(0, 4); drawEdge(1, 5); drawEdge(2, 6); drawEdge(3, 7);

        } else {
            glm::vec3 localCorners[8] = {
                col.offset + col.extents * glm::vec3(-1, -1, -1),
                col.offset + col.extents * glm::vec3(1, -1, -1),
                col.offset + col.extents * glm::vec3(1, 1, -1),
                col.offset + col.extents * glm::vec3(-1, 1, -1),
                col.offset + col.extents * glm::vec3(-1, -1, 1),
                col.offset + col.extents * glm::vec3(1, -1, 1),
                col.offset + col.extents * glm::vec3(1, 1, 1),
                col.offset + col.extents * glm::vec3(-1, 1, 1)
            };

            glm::vec3 worldCorners[8];
            ImVec2 screenCorners[8];
            bool valid[8];

            for (int k = 0; k < 8; ++k) {
                worldCorners[k] = glm::vec3(worldM * glm::vec4(localCorners[k], 1.0f));
                valid[k] = projectToScreen(worldCorners[k], screenCorners[k]);
            }

            auto drawEdge = [&](int i, int j) {
                if (valid[i] && valid[j]) {
                    drawList->AddLine(screenCorners[i], screenCorners[j], color, 1.5f);
                }
            };

            drawEdge(0, 1); drawEdge(1, 2); drawEdge(2, 3); drawEdge(3, 0);
            drawEdge(4, 5); drawEdge(5, 6); drawEdge(6, 7); drawEdge(7, 4);
            drawEdge(0, 4); drawEdge(1, 5); drawEdge(2, 6); drawEdge(3, 7);
        }
    }
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

