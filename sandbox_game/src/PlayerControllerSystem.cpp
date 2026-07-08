#include "PlayerControllerSystem.hpp"
#include "ecs/components/PlayerControllerComponent.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/RigidBody.hpp"
#include "ecs/components/Name.hpp"
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
        break; // Use the first camera we find
    }

    // Loop over entities with PlayerControllerComponent and Transform
    for (auto [entity, pc, transform] : registry.view<PlayerControllerComponent, Transform>()) {
        auto* rb = registry.get<RigidBodyComponent>(entity);
        if (!rb) continue;

        pc.debugRunningCount++;

        static int printCounter = 0;
        if (++printCounter % 60 == 0) {
            std::cout << "[PlayerControllerSystem] pos=(" << transform.position.x << "," << transform.position.y << "," << transform.position.z 
                      << ") rb_vel=(" << rb->velocity.x << "," << rb->velocity.y << "," << rb->velocity.z << ") speed=" << pc.speed << std::endl;
        }

        // 1. Movement WASD
        glm::vec3 moveDir(0.0f);
        if (renderer.getKey(GLFW_KEY_W)) { moveDir += camForward; std::cout << "[PlayerController] W pressed! camForward=(" << camForward.x << "," << camForward.z << ")" << std::endl; }
        if (renderer.getKey(GLFW_KEY_S)) { moveDir -= camForward; std::cout << "[PlayerController] S pressed!" << std::endl; }
        if (renderer.getKey(GLFW_KEY_A)) { moveDir -= camRight; std::cout << "[PlayerController] A pressed!" << std::endl; }
        if (renderer.getKey(GLFW_KEY_D)) { moveDir += camRight; std::cout << "[PlayerController] D pressed!" << std::endl; }

        pc.debugMoveDirLength = glm::length(moveDir);

        if (glm::length(moveDir) > 1e-4f) {
            moveDir = glm::normalize(moveDir);
            glm::vec3 targetVel = moveDir * pc.speed;
            rb->velocity.x = targetVel.x;
            rb->velocity.z = targetVel.z;
            rb->sleeping = false;
            rb->sleepTimer = 0.0f;
            std::cout << "[PlayerController] Moving: vel.x=" << rb->velocity.x << " vel.z=" << rb->velocity.z << std::endl;
        } else {
            // Decay horizontal velocity to stop sliding instantly if keys are released
            rb->velocity.x *= std::exp(-15.0f * dt);
            rb->velocity.z *= std::exp(-15.0f * dt);
        }
        pc.debugRbVelocity = rb->velocity;

        // 2. Jump (Space)
        bool spaceDown = renderer.getKey(GLFW_KEY_SPACE);
        if (spaceDown && !pc.wasJumpPressed) {
            // Grounded check: check if vertical velocity is small and the body has had collision contact
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

                std::cout << "[Player Interaction] Interacted with: " << nameStr << "!" << std::endl;

                // Physical push reaction (push up and away from player)
                auto* otherRb = registry.get<RigidBodyComponent>(closestEntity);
                auto* closestTransform = registry.get<Transform>(closestEntity);
                glm::vec3 pushDir = glm::vec3(0.0f, 1.0f, 0.0f);
                if (closestTransform) {
                    pushDir = closestTransform->position - transform.position;
                    pushDir.y = 0.0f; // horizontal push
                }
                if (glm::length(pushDir) < 1e-4f) {
                    pushDir = glm::vec3(0.0f, 1.0f, 0.0f);
                } else {
                    pushDir = glm::normalize(pushDir);
                    pushDir.y = 0.5f; // push slightly upwards
                    pushDir = glm::normalize(pushDir);
                }
                otherRb->velocity = pushDir * 6.0f;
                otherRb->sleeping = false;
                otherRb->sleepTimer = 0.0f;
            }
        }
        pc.wasInteractPressed = interactDown;
    }
}
