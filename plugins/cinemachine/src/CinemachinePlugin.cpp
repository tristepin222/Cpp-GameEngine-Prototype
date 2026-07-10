#include "core/Plugin.hpp"
#include "editor/EditorUI.hpp"
#include "ecs/components/Name.hpp"
#include "scenes/ComponentSerializerRegistry.hpp"
#include "CinemachineComponent.hpp"
#include "CinemachineSystem.hpp"
#include <iostream>

PLUGIN_API void initPlugin(PluginContext* context) {
    ImGui::SetCurrentContext(context->imguiContext);

    // 1. Register the component editor UI callback
    EditorUI::registerComponentInspector("Cinemachine Virtual Camera", [](Registry& registry, Entity entity) {
        auto* vcam = registry.get<CinemachineVirtualCamera>(entity);
        if (!vcam) return;

        if (ImGui::CollapsingHeader("Cinemachine Virtual Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Active", &vcam->active);
            ImGui::DragInt("Priority", &vcam->priority, 1, 0, 1000);
            ImGui::SliderFloat("FOV", &vcam->fov, 10.0f, 120.0f, "%.1f");

            ImGui::Separator();
            ImGui::TextUnformatted("Tracking Configuration");

            // Follow Target Dropdown
            std::string followLabel = "None";
            if (vcam->followTarget.getId() != Entity::INVALID_ENTITY && registry.isValid(vcam->followTarget)) {
                if (auto* nameComp = registry.get<Name>(vcam->followTarget)) {
                    followLabel = nameComp->value;
                } else {
                    followLabel = "Entity " + std::to_string(vcam->followTarget.getId());
                }
            }

            if (ImGui::BeginCombo("Follow Target", followLabel.c_str())) {
                if (ImGui::Selectable("None", vcam->followTarget.getId() == Entity::INVALID_ENTITY)) {
                    vcam->followTarget = Entity();
                }
                for (auto [ent, nameComp] : registry.view<Name>()) {
                    if (ent != entity) { // Prevent self-targeting
                        bool isSelected = (ent == vcam->followTarget);
                        if (ImGui::Selectable(nameComp.value.c_str(), isSelected)) {
                            vcam->followTarget = ent;
                        }
                    }
                }
                ImGui::EndCombo();
            }

            // LookAt Target Dropdown
            std::string lookAtLabel = "None";
            if (vcam->lookAtTarget.getId() != Entity::INVALID_ENTITY && registry.isValid(vcam->lookAtTarget)) {
                if (auto* nameComp = registry.get<Name>(vcam->lookAtTarget)) {
                    lookAtLabel = nameComp->value;
                } else {
                    lookAtLabel = "Entity " + std::to_string(vcam->lookAtTarget.getId());
                }
            }

            if (ImGui::BeginCombo("LookAt Target", lookAtLabel.c_str())) {
                if (ImGui::Selectable("None", vcam->lookAtTarget.getId() == Entity::INVALID_ENTITY)) {
                    vcam->lookAtTarget = Entity();
                }
                for (auto [ent, nameComp] : registry.view<Name>()) {
                    if (ent != entity) { // Prevent self-targeting
                        bool isSelected = (ent == vcam->lookAtTarget);
                        if (ImGui::Selectable(nameComp.value.c_str(), isSelected)) {
                            vcam->lookAtTarget = ent;
                        }
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::DragFloat3("Follow Offset", &vcam->followOffset[0], 0.05f);
            ImGui::SliderFloat("Follow Damping", &vcam->followDamping, 0.0f, 10.0f, "%.2f");
            ImGui::SliderFloat("LookAt Damping", &vcam->lookAtDamping, 0.0f, 10.0f, "%.2f");

            ImGui::Spacing();
            if (ImGui::Button("Remove Component", ImVec2(-1, 24))) {
                registry.remove<CinemachineVirtualCamera>(entity);
            }
        }
    });

    // 2. Register "+ Add Component" menu option callback
    EditorUI::registerComponentAddCallback("Cinemachine Virtual Camera", [](Registry& registry, Entity entity) {
        registry.emplace<CinemachineVirtualCamera>(entity, CinemachineVirtualCamera{});
    });

    // 3. Register Scene Serializer/Deserializer callbacks for component saving/loading
    ComponentSerializerRegistry::getInstance().registerComponent(
        "CinemachineVirtualCamera",
        // Serializer
        [](Registry& registry, Entity entity, std::ostream& out, int indent) {
            if (auto* vcam = registry.get<CinemachineVirtualCamera>(entity)) {
                std::string followName = "";
                if (vcam->followTarget.getId() != Entity::INVALID_ENTITY && registry.isValid(vcam->followTarget)) {
                    if (auto* nameComp = registry.get<Name>(vcam->followTarget)) {
                        followName = nameComp->value;
                    }
                }
                std::string lookAtName = "";
                if (vcam->lookAtTarget.getId() != Entity::INVALID_ENTITY && registry.isValid(vcam->lookAtTarget)) {
                    if (auto* nameComp = registry.get<Name>(vcam->lookAtTarget)) {
                        lookAtName = nameComp->value;
                    }
                }

                out << ",\n";
                out << JSONUtils::indent(indent) << "\"entityType\": \"CinemachineVirtualCamera\",\n";
                out << JSONUtils::indent(indent) << "\"active\": " << (vcam->active ? "true" : "false") << ",\n";
                out << JSONUtils::indent(indent) << "\"priority\": " << static_cast<float>(vcam->priority) << ",\n";
                out << JSONUtils::indent(indent) << "\"fov\": " << vcam->fov << ",\n";
                out << JSONUtils::indent(indent) << "\"followTarget\": " << static_cast<float>(vcam->followTarget.getId()) << ",\n";
                out << JSONUtils::indent(indent) << "\"lookAtTarget\": " << static_cast<float>(vcam->lookAtTarget.getId()) << ",\n";
                out << JSONUtils::indent(indent) << "\"followTargetName\": \"" << followName << "\",\n";
                out << JSONUtils::indent(indent) << "\"lookAtTargetName\": \"" << lookAtName << "\",\n";
                out << JSONUtils::indent(indent) << "\"followDamping\": " << vcam->followDamping << ",\n";
                out << JSONUtils::indent(indent) << "\"lookAtDamping\": " << vcam->lookAtDamping << ",\n";
                out << JSONUtils::indent(indent) << "\"followOffsetX\": " << vcam->followOffset.x << ",\n";
                out << JSONUtils::indent(indent) << "\"followOffsetY\": " << vcam->followOffset.y << ",\n";
                out << JSONUtils::indent(indent) << "\"followOffsetZ\": " << vcam->followOffset.z;
            }
        },
        // Deserializer
        [](Registry& registry, VulkanRenderer&, Entity entity, const std::string& json) {
            if (json.find("\"entityType\": \"CinemachineVirtualCamera\"") != std::string::npos ||
                json.find("\"followOffsetX\"") != std::string::npos) {
                
                auto& vcam = registry.emplace<CinemachineVirtualCamera>(entity, CinemachineVirtualCamera{});
                vcam.active = (json.find("\"active\": true") != std::string::npos || json.find("\"active\":true") != std::string::npos);
                
                float priorityVal = 10.0f;
                if (JSONUtils::extractFloatValue(json, "priority", priorityVal)) {
                    vcam.priority = static_cast<int>(priorityVal);
                }
                
                JSONUtils::extractFloatValue(json, "fov", vcam.fov);
                JSONUtils::extractFloatValue(json, "followDamping", vcam.followDamping);
                JSONUtils::extractFloatValue(json, "lookAtDamping", vcam.lookAtDamping);
                
                JSONUtils::extractFloatValue(json, "followOffsetX", vcam.followOffset.x);
                JSONUtils::extractFloatValue(json, "followOffsetY", vcam.followOffset.y);
                JSONUtils::extractFloatValue(json, "followOffsetZ", vcam.followOffset.z);
                
                float followIdVal = static_cast<float>(Entity::INVALID_ENTITY);
                if (JSONUtils::extractFloatValue(json, "followTarget", followIdVal)) {
                    vcam.followTarget = Entity(static_cast<std::uint32_t>(followIdVal));
                }
                
                float lookAtIdVal = static_cast<float>(Entity::INVALID_ENTITY);
                if (JSONUtils::extractFloatValue(json, "lookAtTarget", lookAtIdVal)) {
                    vcam.lookAtTarget = Entity(static_cast<std::uint32_t>(lookAtIdVal));
                }

                vcam.followTargetName = JSONUtils::extractStringValue(json, "followTargetName");
                vcam.lookAtTargetName = JSONUtils::extractStringValue(json, "lookAtTargetName");
            }
        }
    );

    // 4. Register the system
    auto cinemachineSystem = std::make_shared<CinemachineSystem>(*context->registry, *context->editorMode);
    context->systemManager->addSystem(cinemachineSystem);

    std::cout << "[CinemachinePlugin] Plugin DLL initialized. Custom system and component registered." << std::endl;
}

PLUGIN_API void shutdownPlugin(PluginContext* context) {
    std::cout << "[CinemachinePlugin] Plugin DLL shutting down." << std::endl;
}
