#include "core/Plugin.hpp"
#include "ecs/System.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/RigidBody.hpp"
#include "ecs/components/EditorCamera.hpp"
#include "ecs/components/Collider.hpp"
#include "ecs/components/PlayerControllerComponent.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/PhysgunScript.hpp"
#include "ecs/components/CinemachineComponent.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "meta/ComponentReflection.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstddef>

namespace Engine {

    struct Ray {
        glm::vec3 origin{ 0.0f };
        glm::vec3 direction{ 0.0f, 0.0f, -1.0f };
    };

    inline glm::mat4 getEntityWorldMatrix(Registry& registry, Entity entity, int depth = 0) {
        if (depth > 100) return glm::mat4(1.0f);
        glm::mat4 m = glm::mat4(1.0f);
        if (auto* t = registry.get<Transform>(entity)) {
            m = t->matrix();
        }
        if (auto* h = registry.get<HierarchyComponent>(entity)) {
            if (h->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(h->parent)) {
                m = getEntityWorldMatrix(registry, h->parent, depth + 1) * m;
            }
        }
        return m;
    }

    inline glm::vec3 getEntityWorldPosition(Registry& registry, Entity entity) {
        glm::mat4 m = getEntityWorldMatrix(registry, entity);
        return glm::vec3(m[3]);
    }

    class PhysgunSystem : public System {
    private:
        Registry& registry;
        VulkanRenderer& renderer;
        EditorModeState& editorMode;
        bool fKeyPressed = false;
        bool rKeyPressed = false;

    public:
        PhysgunSystem(Registry& reg, VulkanRenderer& rend, EditorModeState& mode)
            : registry(reg), renderer(rend), editorMode(mode) {}

        void update(float dt) override {
            if (!editorMode.isPlaying) {
                // Clear hold state when stopping play mode
                for (auto [ent, script] : registry.view<PhysgunScript>()) {
                    script.isHolding = false;
                    script.heldEntity = Entity();
                }
                return;
            }

            for (auto [ent, script] : registry.view<PhysgunScript>()) {
                script.updateCount++;
            }

            // --- 1. Camera Mode Switching (F Key) ---
            bool fDown = renderer.getKey(GLFW_KEY_F);
            if (fDown && !fKeyPressed) {
                fKeyPressed = true;
                for (auto [vcamEnt, vcam] : registry.view<CinemachineVirtualCamera>()) {
                    if (vcam.active) {
                        if (vcam.mode == CinemachineMode::ThirdPersonFollow) {
                            vcam.mode = CinemachineMode::FirstPerson;
                            vcam.followOffset = glm::vec3(0.0f, 0.0f, 0.0f);
                            vcam.followDamping = 0.0f;
                            vcam.lookAtDamping = 0.0f;
                            std::cout << "[PhysgunScript] Switched camera to First Person." << std::endl;
                        } else {
                            vcam.mode = CinemachineMode::ThirdPersonFollow;
                            vcam.followOffset = glm::vec3(0.0f, 4.0f, 8.0f);
                            vcam.followDamping = 2.0f;
                            vcam.lookAtDamping = 2.0f;
                            std::cout << "[PhysgunScript] Switched camera to Third Person Follow." << std::endl;
                        }
                        break;
                    }
                }
            } else if (!fDown) {
                fKeyPressed = false;
            }

            // --- Toggle Debug Ray on R Press ---
            bool rDown = renderer.getKey(GLFW_KEY_R);
            if (rDown && !rKeyPressed) {
                rKeyPressed = true;
                for (auto [ent, script] : registry.view<PhysgunScript>()) {
                    script.debugShowRay = !script.debugShowRay;
                }
            } else if (!rDown) {
                rKeyPressed = false;
            }

            // --- 2. Calculate Ray Direction from active camera transform ---
            float pitch = 0.0f;
            float yaw = 0.0f;

            for (auto [camEntity, cam, camTransform] : registry.view<Camera, Transform>()) {
                if (registry.has<EditorCamera>(camEntity)) continue;

                yaw = camTransform.rotation.y;
                pitch = camTransform.rotation.x;

                // Adjust pitch in 3rd Person Follow to use the user's raw orbit pitch,
                // bypassing the vertical offset angle of the camera look-at target.
                for (auto [vcamEnt, vcam] : registry.view<CinemachineVirtualCamera>()) {
                    if (vcam.active) {
                        if (vcam.mode == CinemachineMode::ThirdPersonFollow) {
                            pitch = -vcam.orbitPitch;
                        }
                        break;
                    }
                }
                break;
            }

            float pitchRad = glm::radians(pitch);
            float yawRad = glm::radians(yaw);

            glm::vec3 camForward;
            camForward.x = cos(pitchRad) * cos(yawRad);
            camForward.y = sin(pitchRad);
            camForward.z = cos(pitchRad) * sin(yawRad);
            if (glm::length(camForward) > 1e-4f) {
                camForward = glm::normalize(camForward);
            }

            Ray ray;
            ray.direction = camForward;

            // --- 3. Physics Gun Grab & Carry Logic ---
            bool mouseDown = renderer.getMouseButton(GLFW_MOUSE_BUTTON_LEFT);

            for (auto [scriptEnt, script] : registry.view<PhysgunScript>()) {
                glm::vec3 actualOrigin(0.0f);
                bool originSet = false;

                if (script.originEntity.getId() != Entity::INVALID_ENTITY && registry.isValid(script.originEntity)) {
                    actualOrigin = getEntityWorldPosition(registry, script.originEntity);
                    originSet = true;
                }

                if (!originSet) {
                    if (auto* playerTrans = registry.get<Transform>(scriptEnt)) {
                        actualOrigin = playerTrans->position + glm::vec3(0.0f, 0.5f, 0.0f);
                    }
                }

                ray.origin = actualOrigin;
                script.rayOrigin = ray.origin;
                script.rayDirection = ray.direction;

                if (mouseDown) {
                    if (!script.isHolding) {
                        float closestDist = 999999.0f;
                        Entity hitEntity;

                        for (auto [ent, transform, rb] : registry.view<Transform, RigidBodyComponent>()) {
                            if (rb.type == RigidBodyType::Static) continue;
                            if (registry.has<PlayerControllerComponent>(ent)) continue; // Don't pick up the player

                            // Calculate bounding sphere of the rigid body (scale-aware)
                            float worldRadius = 1.0f;
                            float maxScale = std::max({ transform.scale.x, transform.scale.y, transform.scale.z });
                            if (auto* col = registry.get<ColliderComponent>(ent)) {
                                if (col->shape == ColliderShape::Sphere) {
                                    worldRadius = col->radius * maxScale;
                                } else if (col->shape == ColliderShape::AABB || col->shape == ColliderShape::OBB) {
                                    worldRadius = glm::length(col->extents) * maxScale;
                                }
                            } else {
                                worldRadius = maxScale;
                            }

                            // Ray-Sphere intersection
                            glm::vec3 oc = ray.origin - transform.position;
                            float b = glm::dot(oc, ray.direction);
                            float c = glm::dot(oc, oc) - worldRadius * worldRadius;

                            if (c > 0.0f && b > 0.0f) continue;
                            float discriminant = b * b - c;

                            if (discriminant >= 0.0f) {
                                float t = -b - std::sqrt(discriminant);
                                if (t < 0.0f) t = -b + std::sqrt(discriminant);
                                if (t >= 0.0f && t < closestDist) {
                                    closestDist = t;
                                    hitEntity = ent;
                                }
                            }
                        }

                        if (hitEntity.getId() != Entity::INVALID_ENTITY) {
                            script.isHolding = true;
                            script.heldEntity = hitEntity;
                            script.currentHoldDistance = script.holdDistance;
                            std::cout << "[PhysgunScript] Grabbed entity ID: " << hitEntity.getId() << " at distance: " << script.currentHoldDistance << std::endl;
                        }
                    } else {
                        // Carrying the entity
                        if (registry.isValid(script.heldEntity)) {
                            if (auto* trans = registry.get<Transform>(script.heldEntity)) {
                                if (auto* rb = registry.get<RigidBodyComponent>(script.heldEntity)) {
                                    // Adjust distance with scroll wheel or keyboard keys (Q/E)
                                    float wheel = ImGui::GetIO().MouseWheel;
                                    script.currentHoldDistance += wheel * 1.5f;

                                    if (renderer.getKey(GLFW_KEY_Q)) {
                                        script.currentHoldDistance -= 10.0f * dt;
                                    }
                                    if (renderer.getKey(GLFW_KEY_E)) {
                                        script.currentHoldDistance += 10.0f * dt;
                                    }

                                    script.currentHoldDistance = std::max(1.5f, script.currentHoldDistance);

                                    // Target position
                                    glm::vec3 targetPos = ray.origin + ray.direction * script.currentHoldDistance;
                                    glm::vec3 toTarget = targetPos - trans->position;

                                    // Spring PD controller: Force = Kp * displacement - Kd * velocity
                                    glm::vec3 springForce = (toTarget * script.Kp) - (rb->velocity * script.Kd);

                                    // Apply force directly
                                    rb->force += springForce;
                                    rb->sleeping = false;
                                    rb->sleepTimer = 0.0f;
                                }
                            }
                        } else {
                            script.isHolding = false;
                        }
                    }
                } else {
                    if (script.isHolding) {
                        std::cout << "[PhysgunScript] Dropped entity" << std::endl;
                    }
                    script.isHolding = false;
                }
            }
        }
    };

} // namespace Engine

PLUGIN_API void initPlugin(PluginContext* context) {
    ImGui::SetCurrentContext(context->imguiContext);

    // Register components using the Engine's automatic reflection serialization system
    Engine::ComponentReflection pgRefl;
    pgRefl.name = "PhysgunScript";
    pgRefl.fields = {
        { "Kp", Engine::FieldType::Float, offsetof(Engine::PhysgunScript, Kp) },
        { "Kd", Engine::FieldType::Float, offsetof(Engine::PhysgunScript, Kd) },
        { "holdDistance", Engine::FieldType::Float, offsetof(Engine::PhysgunScript, holdDistance) },
        { "isHolding", Engine::FieldType::Bool, offsetof(Engine::PhysgunScript, isHolding) },
        { "currentHoldDistance", Engine::FieldType::Float, offsetof(Engine::PhysgunScript, currentHoldDistance) },
        { "debugShowRay", Engine::FieldType::Bool, offsetof(Engine::PhysgunScript, debugShowRay) }
    };
    pgRefl.add = [](Registry& reg, Entity e) { reg.emplace<Engine::PhysgunScript>(e, Engine::PhysgunScript{}); };
    pgRefl.has = [](Registry& reg, Entity e) { return reg.has<Engine::PhysgunScript>(e); };
    pgRefl.remove = [](Registry& reg, Entity e) { reg.remove<Engine::PhysgunScript>(e); };
    pgRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<Engine::PhysgunScript>(e)); };

    Engine::ComponentReflectionRegistry::getInstance().registerComponent(pgRefl);

    // Add the physics gun system
    context->systemManager->addSystem(std::make_shared<Engine::PhysgunSystem>(*context->registry, *context->renderer, *context->editorMode));
    std::cout << "[PhysgunPlugin] Initialized successfully using reflection registry." << std::endl;
}

PLUGIN_API void shutdownPlugin(PluginContext* context) {
    std::cout << "[PhysgunPlugin] Shutdown." << std::endl;
}
