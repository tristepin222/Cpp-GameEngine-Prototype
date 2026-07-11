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

            const char* modes[] = { "Third Person Follow", "First Person", "Fixed LookAt" };
            int currentModeIdx = static_cast<int>(vcam->mode);
            if (ImGui::Combo("Camera Mode", &currentModeIdx, modes, IM_ARRAYSIZE(modes))) {
                vcam->mode = static_cast<CinemachineMode>(currentModeIdx);
            }

            // Helper lambda for Target selection dropdown
            auto drawTargetSelector = [&](const char* label, Entity& target) {
                std::string targetLabel = "None";
                if (target.getId() != Entity::INVALID_ENTITY && registry.isValid(target)) {
                    if (auto* nameComp = registry.get<Name>(target)) {
                        targetLabel = nameComp->value;
                    } else {
                        targetLabel = "Entity " + std::to_string(target.getId());
                    }
                }

                if (ImGui::BeginCombo(label, targetLabel.c_str())) {
                    if (ImGui::Selectable("None", target.getId() == Entity::INVALID_ENTITY)) {
                        target = Entity();
                    }
                    for (auto [ent, nameComp] : registry.view<Name>()) {
                        if (ent != entity) { // Prevent self-targeting
                            bool isSelected = (ent == target);
                            if (ImGui::Selectable(nameComp.value.c_str(), isSelected)) {
                                target = ent;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            };

            if (vcam->mode == CinemachineMode::ThirdPersonFollow) {
                drawTargetSelector("Follow Target", vcam->followTarget);
                drawTargetSelector("LookAt Target (Optional)", vcam->lookAtTarget);

                ImGui::DragFloat3("Follow Offset", &vcam->followOffset[0], 0.05f);
                ImGui::SliderFloat("Follow Damping", &vcam->followDamping, 0.0f, 10.0f, "%.2f");
                ImGui::SliderFloat("LookAt Damping", &vcam->lookAtDamping, 0.0f, 10.0f, "%.2f");

                ImGui::Checkbox("Enable Mouse Orbit", &vcam->mouseOrbit);
                if (vcam->mouseOrbit) {
                    ImGui::SliderFloat("Orbit Sensitivity", &vcam->orbitSensitivity, 0.01f, 1.0f, "%.2f");
                    ImGui::DragFloat("Orbit Yaw", &vcam->orbitYaw, 0.5f);
                    ImGui::DragFloat("Orbit Pitch", &vcam->orbitPitch, 0.5f, -80.0f, 80.0f);
                }
            } else if (vcam->mode == CinemachineMode::FirstPerson) {
                drawTargetSelector("Follow Target (Player)", vcam->followTarget);

                char boneBuf[128];
                snprintf(boneBuf, sizeof(boneBuf), "%s", vcam->lockToBone.c_str());
                if (ImGui::InputText("Lock to Bone", boneBuf, sizeof(boneBuf))) {
                    vcam->lockToBone = boneBuf;
                }

                ImGui::DragFloat3("Eye Offset", &vcam->followOffset[0], 0.05f);
                ImGui::SliderFloat("Follow Damping", &vcam->followDamping, 0.0f, 10.0f, "%.2f");

                ImGui::Checkbox("Enable Mouse Look", &vcam->mouseLook);
                if (vcam->mouseLook) {
                    ImGui::SliderFloat("Look Sensitivity", &vcam->orbitSensitivity, 0.01f, 1.0f, "%.2f");
                    ImGui::DragFloat("Camera Yaw", &vcam->cameraYaw, 0.5f);
                    ImGui::DragFloat("Camera Pitch", &vcam->cameraPitch, 0.5f, -89.0f, 89.0f);
                }
            } else if (vcam->mode == CinemachineMode::FixedLookAt) {
                drawTargetSelector("LookAt Target", vcam->lookAtTarget);
                ImGui::SliderFloat("LookAt Damping", &vcam->lookAtDamping, 0.0f, 10.0f, "%.2f");
            }

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
                out << JSONUtils::indent(indent) << "\"mode\": " << static_cast<float>(vcam->mode) << ",\n";
                out << JSONUtils::indent(indent) << "\"mouseOrbit\": " << (vcam->mouseOrbit ? "true" : "false") << ",\n";
                out << JSONUtils::indent(indent) << "\"mouseLook\": " << (vcam->mouseLook ? "true" : "false") << ",\n";
                out << JSONUtils::indent(indent) << "\"orbitSensitivity\": " << vcam->orbitSensitivity << ",\n";
                out << JSONUtils::indent(indent) << "\"orbitYaw\": " << vcam->orbitYaw << ",\n";
                out << JSONUtils::indent(indent) << "\"orbitPitch\": " << vcam->orbitPitch << ",\n";
                out << JSONUtils::indent(indent) << "\"cameraYaw\": " << vcam->cameraYaw << ",\n";
                out << JSONUtils::indent(indent) << "\"cameraPitch\": " << vcam->cameraPitch << ",\n";
                out << JSONUtils::indent(indent) << "\"followTarget\": " << static_cast<float>(vcam->followTarget.getId()) << ",\n";
                out << JSONUtils::indent(indent) << "\"lookAtTarget\": " << static_cast<float>(vcam->lookAtTarget.getId()) << ",\n";
                out << JSONUtils::indent(indent) << "\"followTargetName\": \"" << followName << "\",\n";
                out << JSONUtils::indent(indent) << "\"lookAtTargetName\": \"" << lookAtName << "\",\n";
                out << JSONUtils::indent(indent) << "\"lockToBone\": \"" << vcam->lockToBone << "\",\n";
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
                
                float modeVal = 0.0f;
                if (JSONUtils::extractFloatValue(json, "mode", modeVal)) {
                    vcam.mode = static_cast<CinemachineMode>(static_cast<int>(modeVal));
                }

                if (json.find("\"mouseOrbit\"") != std::string::npos) {
                    vcam.mouseOrbit = (json.find("\"mouseOrbit\": true") != std::string::npos || json.find("\"mouseOrbit\":true") != std::string::npos);
                } else {
                    vcam.mouseOrbit = true;
                }

                if (json.find("\"mouseLook\"") != std::string::npos) {
                    vcam.mouseLook = (json.find("\"mouseLook\": true") != std::string::npos || json.find("\"mouseLook\":true") != std::string::npos);
                } else {
                    vcam.mouseLook = true;
                }

                JSONUtils::extractFloatValue(json, "orbitSensitivity", vcam.orbitSensitivity);
                JSONUtils::extractFloatValue(json, "orbitYaw", vcam.orbitYaw);
                JSONUtils::extractFloatValue(json, "orbitPitch", vcam.orbitPitch);
                JSONUtils::extractFloatValue(json, "cameraYaw", vcam.cameraYaw);
                JSONUtils::extractFloatValue(json, "cameraPitch", vcam.cameraPitch);

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
                vcam.lockToBone = JSONUtils::extractStringValue(json, "lockToBone");
                if (vcam.lockToBone.empty() && json.find("\"lockToBone\"") == std::string::npos) {
                    vcam.lockToBone = "Head"; // Default fallback if not defined in older scene files
                }
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
