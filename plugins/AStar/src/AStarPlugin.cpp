#include "core/Plugin.hpp"
#include "editor/EditorUI.hpp"
#include "scenes/ComponentSerializerRegistry.hpp"
#include "scenes/JSONUtils.hpp"
#include "AStarSystem.hpp"
#include <iostream>

PLUGIN_API void initPlugin(PluginContext* context) {
    ImGui::SetCurrentContext(context->imguiContext);

    // 1. Register the component editor UI callback for AStarAgent
    EditorUI::registerComponentInspector("AStar Agent", [](Registry& registry, Entity entity) {
        auto* agent = registry.get<AStarAgent>(entity);
        if (!agent) return;

        if (ImGui::CollapsingHeader("AStar Agent", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputInt("Target X", &agent->targetX);
            ImGui::InputInt("Target Y", &agent->targetY);
            ImGui::Checkbox("Allow Diagonal", &agent->allowDiagonal);
            ImGui::Checkbox("Show Debug Path", &agent->showDebugPath);

            ImGui::Separator();
            ImGui::Text("Path Length: %zu nodes", agent->path.size());
            if (ImGui::TreeNode("Path Points")) {
                for (size_t i = 0; i < agent->path.size(); ++i) {
                    ImGui::Text("[%zu] (%d, %d)", i, agent->path[i].x, agent->path[i].y);
                }
                ImGui::TreePop();
            }

            ImGui::Spacing();
            if (ImGui::Button("Remove Component", ImVec2(-1, 24))) {
                registry.remove<AStarAgent>(entity);
            }
        }
    });

    // 2. Register "+ Add Component" menu option callback
    EditorUI::registerComponentAddCallback("AStar Agent", [](Registry& registry, Entity entity) {
        registry.emplace<AStarAgent>(entity, AStarAgent{});
    });

    // 3. Register Scene Serializer/Deserializer callbacks
    ComponentSerializerRegistry::getInstance().registerComponent(
        "AStarAgent",
        // Serializer
        [](Registry& registry, Entity entity, std::ostream& out, int indent) {
            if (auto* agent = registry.get<AStarAgent>(entity)) {
                out << ",\n";
                out << JSONUtils::indent(indent) << "\"entityType\": \"AStarAgent\",\n";
                out << JSONUtils::indent(indent) << "\"targetX\": " << agent->targetX << ",\n";
                out << JSONUtils::indent(indent) << "\"targetY\": " << agent->targetY << ",\n";
                out << JSONUtils::indent(indent) << "\"allowDiagonal\": " << (agent->allowDiagonal ? "true" : "false") << ",\n";
                out << JSONUtils::indent(indent) << "\"showDebugPath\": " << (agent->showDebugPath ? "true" : "false");
            }
        },
        // Deserializer
        [](Registry& registry, VulkanRenderer&, Entity entity, const std::string& json) {
            if (json.find("\"entityType\": \"AStarAgent\"") != std::string::npos ||
                json.find("\"targetX\"") != std::string::npos) {
                
                auto& agent = registry.emplace<AStarAgent>(entity, AStarAgent{});
                
                float targetXVal = 0.0f;
                if (JSONUtils::extractFloatValue(json, "targetX", targetXVal)) {
                    agent.targetX = static_cast<int>(targetXVal);
                }
                
                float targetYVal = 0.0f;
                if (JSONUtils::extractFloatValue(json, "targetY", targetYVal)) {
                    agent.targetY = static_cast<int>(targetYVal);
                }
                
                agent.allowDiagonal = (json.find("\"allowDiagonal\": true") != std::string::npos || json.find("\"allowDiagonal\":true") != std::string::npos);
                agent.showDebugPath = (json.find("\"showDebugPath\": true") != std::string::npos || json.find("\"showDebugPath\":true") != std::string::npos);
            }
        }
    );

    // 4. Instantiate AStarSystem and add it to the system manager
    auto astarSystem = std::make_shared<AStarSystem>(*context->registry, *context->renderer, *context->editorMode);
    context->systemManager->addSystem(astarSystem);

    std::cout << "[AStarPlugin] Plugin DLL initialized. Pathfinding system registered." << std::endl;
}

PLUGIN_API void shutdownPlugin(PluginContext* context) {
    std::cout << "[AStarPlugin] Plugin DLL shutting down." << std::endl;
}
