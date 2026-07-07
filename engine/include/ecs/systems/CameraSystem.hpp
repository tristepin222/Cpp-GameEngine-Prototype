#pragma once
#include "../Registry.hpp"
#include "../components/Camera.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include "../System.hpp"
#include "../../renderer/VulkanRenderer.hpp"
#include "../components/Transform.hpp"
#include "../components/inputComponent.hpp"
#include "editor/EditorModeState.hpp"
#include "ecs/components/EditorCamera.hpp"

/**
 * @class CameraSystem
 * @brief System that processes camera entities, updating their orientation, movement, aspect ratio, and submitting camera matrices to the renderer.
 */
class CameraSystem : public System {
public:
    /**
     * @brief Construct a new Camera System object.
     * @param reg Reference to the ECS Registry.
     * @param renderer Reference to the Vulkan Renderer.
     * @param editorMode Reference to the Editor Mode State.
     */
    CameraSystem(Registry& reg, VulkanRenderer& renderer, EditorModeState& editorMode)
        : registry(reg), renderer(renderer), editorMode(editorMode) {}

    /**
     * @brief Updates the camera's rotation, movement, aspect ratio, and view-projection matrices.
     * @param dt Delta time in seconds.
     */
    void update(float dt) override {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(renderer.getWindow(), &width, &height);
        float aspect = 1.0f;
        if (height > 0) {
            aspect = static_cast<float>(width) / static_cast<float>(height);
        }

        if (!editorMode.isPlaying) {
            // Editor Mode: update and render through the Editor Camera ONLY
            for (auto [entity, cam, transform, input] : registry.view<Camera, Transform, InputComponent>()) {
                if (registry.has<EditorCamera>(entity)) {
                    cam.aspect = aspect;

                    // Update rotation (fly mode)
                    transform.rotation.y += input.look.x * cam.mouseSensitivity;
                    transform.rotation.x += input.look.y * cam.mouseSensitivity;
                    transform.rotation.x = glm::clamp(transform.rotation.x, -89.f, 89.f);

                    // Compute directions
                    glm::vec3 forward;
                    forward.x = cos(glm::radians(transform.rotation.x)) * cos(glm::radians(transform.rotation.y));
                    forward.y = sin(glm::radians(transform.rotation.x));
                    forward.z = cos(glm::radians(transform.rotation.x)) * sin(glm::radians(transform.rotation.y));
                    
                    float forwardLen = glm::length(forward);
                    forward = (forwardLen > 1e-4f) ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, -1.0f);

                    glm::vec3 crossProduct = glm::cross(forward, glm::vec3(0, 1, 0));
                    glm::vec3 right = glm::vec3(1, 0, 0);
                    if (glm::length(crossProduct) > 1e-4f) {
                        right = glm::normalize(crossProduct);
                    }

                    glm::vec3 upCross = glm::cross(right, forward);
                    glm::vec3 up = glm::vec3(0, 1, 0);
                    if (glm::length(upCross) > 1e-4f) {
                        up = glm::normalize(upCross);
                    }

                    glm::vec3 move =
                        forward * input.movement.z +
                        right * input.movement.x +
                        up * input.movement.y;

                    transform.position += move * cam.moveSpeed * dt;
                    renderer.setActiveCamera(cam.projection(), transform.position, cam.view(transform));
                    return; // Done
                }
            }
        } else {
            // Play Mode: update and render through the Scene's Camera (the first non-editor camera)
            Entity fallbackCam = Entity();
            for (auto [entity, cam, transform] : registry.view<Camera, Transform>()) {
                if (!registry.has<EditorCamera>(entity)) {
                    cam.aspect = aspect;
                    // Note: Scene cameras are animated/controlled by scripts/physics, not editor controls
                    renderer.setActiveCamera(cam.projection(), transform.position, cam.view(transform));
                    return; // Done
                } else {
                    fallbackCam = entity;
                }
            }

            // Fallback to Editor Camera if no scene camera exists
            if (fallbackCam.getId() != Entity::INVALID_ENTITY && registry.isValid(fallbackCam)) {
                auto* cam = registry.get<Camera>(fallbackCam);
                auto* transform = registry.get<Transform>(fallbackCam);
                if (cam && transform) {
                    cam->aspect = aspect;
                    renderer.setActiveCamera(cam->projection(), transform->position, cam->view(*transform));
                }
            }
        }
    }

private:
    /** @brief Reference to the entity registry. */
    Registry& registry;
    /** @brief Reference to the renderer. */
    VulkanRenderer& renderer;
    /** @brief Reference to the editor mode state. */
    EditorModeState& editorMode;
};
