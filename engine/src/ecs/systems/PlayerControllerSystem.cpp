#include "ecs/systems/PlayerControllerSystem.hpp"
#include "ecs/components/PlayerControllerComponent.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/RigidBody.hpp"
#include "ecs/components/Name.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

PlayerControllerSystem::PlayerControllerSystem(Registry& reg, VulkanRenderer& renderer, EditorModeState& editorMode)
    : registry(reg), renderer(renderer), editorMode(editorMode) {}

void PlayerControllerSystem::update(float dt) {
    if (!editorMode.isPlaying) {
        return;
    }

    GLFWwindow* window = renderer.getWindow();
    if (!window) return;

    // Find camera direction (projected on horizontal plane)
    glm::vec3 camForward(0.0f, 0.0f, -1.0f);
    glm::vec3 camRight(1.0f, 0.0f, 0.0f);

    for (auto [camEntity, cam, camTransform] : registry.view<Camera, Transform>()) {
        float yaw = camTransform.rotation.y;
        camForward.x = cos(glm::radians(yaw));
        camForward.y = 0.0f;
        camForward.z = sin(glm::radians(yaw));
        if (glm::length(camForward) > 1e-4f) {
            camForward = glm::normalize(camForward);
        }
        camRight = glm::normalize(glm::cross(camForward, glm::vec3(0.0f, 1.0f, 0.0f)));
        break; // Use the first camera found
    }

    // Process all entities with PlayerControllerComponent + Transform
    for (auto [entity, pc, transform] : registry.view<PlayerControllerComponent, Transform>()) {
        auto* rb = registry.get<RigidBodyComponent>(entity);
        if (!rb) continue;

        pc.debugRunningCount++;

        // 1. WASD Movement
        glm::vec3 moveDir(0.0f);
        if (renderer.getKey(GLFW_KEY_W)) moveDir += camForward;
        if (renderer.getKey(GLFW_KEY_S)) moveDir -= camForward;
        if (renderer.getKey(GLFW_KEY_A)) moveDir -= camRight;
        if (renderer.getKey(GLFW_KEY_D)) moveDir += camRight;

        pc.debugMoveDirLength = glm::length(moveDir);

        if (glm::length(moveDir) > 1e-4f) {
            moveDir = glm::normalize(moveDir);
            glm::vec3 targetVel = moveDir * pc.speed;
            rb->velocity.x = targetVel.x;
            rb->velocity.z = targetVel.z;
            rb->sleeping = false;
            rb->sleepTimer = 0.0f;
        } else {
            // Decay horizontal velocity when no keys held
            rb->velocity.x *= std::exp(-15.0f * dt);
            rb->velocity.z *= std::exp(-15.0f * dt);
        }
        pc.debugRbVelocity = rb->velocity;

        // 2. Jump (Space)
        bool spaceDown = renderer.getKey(GLFW_KEY_SPACE);
        if (spaceDown && !pc.wasJumpPressed) {
            if (rb->hadContactThisFrame || std::abs(rb->velocity.y) < 0.05f) {
                rb->velocity.y = pc.jumpForce;
                rb->sleeping = false;
                rb->sleepTimer = 0.0f;
            }
        }
        pc.wasJumpPressed = spaceDown;

        // 3. Interact (E)
        bool interactDown = renderer.getKey(GLFW_KEY_E);
        if (interactDown && !pc.wasInteractPressed) {
            Entity closestEntity = Entity();
            float closestDist = pc.interactRange;

            for (auto [otherEntity, otherTransform, otherRb] : registry.view<Transform, RigidBodyComponent>()) {
                if (otherEntity == entity) continue;
                float dist = glm::distance(transform.position, otherTransform.position);
                if (dist < closestDist) {
                    closestDist = dist;
                    closestEntity = otherEntity;
                }
            }

            if (closestEntity.getId() != Entity::INVALID_ENTITY && registry.isValid(closestEntity)) {
                std::string nameStr = "Entity " + std::to_string(closestEntity.getId());
                if (auto* nameComp = registry.get<Name>(closestEntity)) {
                    nameStr = nameComp->value;
                }
                std::cout << "[PlayerController] Interacted with: " << nameStr << std::endl;

                auto* otherRb = registry.get<RigidBodyComponent>(closestEntity);
                auto* closestTransform = registry.get<Transform>(closestEntity);
                if (otherRb) {
                    glm::vec3 pushDir(0.0f, 1.0f, 0.0f);
                    if (closestTransform) {
                        pushDir = closestTransform->position - transform.position;
                        pushDir.y = 0.0f;
                    }
                    if (glm::length(pushDir) < 1e-4f) {
                        pushDir = glm::vec3(0.0f, 1.0f, 0.0f);
                    } else {
                        pushDir = glm::normalize(pushDir);
                        pushDir.y = 0.5f;
                        pushDir = glm::normalize(pushDir);
                    }
                    otherRb->velocity = pushDir * 6.0f;
                    otherRb->sleeping = false;
                    otherRb->sleepTimer = 0.0f;
                }
            }
        }
        pc.wasInteractPressed = interactDown;
    }
}
