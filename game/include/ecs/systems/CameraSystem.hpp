#pragma once
#include "../Registry.hpp"
#include "../components/Camera.hpp"
#include <glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include "../System.hpp"
#include "../../renderer/VulkanRenderer.hpp"
#include "../components/Transform.hpp"
#include "../components/inputComponent.hpp"

class CameraSystem : public System {
public:
    CameraSystem(Registry& reg) : registry(reg) {}

    void update(float dt) override {
        for (auto [entity, cam, transform, input] : registry.view<Camera, Transform, InputComponent>()) {
            // Update rotation
            transform.rotation.y += input.look.x * cam.mouseSensitivity;
            transform.rotation.x += input.look.y * cam.mouseSensitivity;
            transform.rotation.x = glm::clamp(transform.rotation.x, -89.f, 89.f);

            // Compute forward/right/up
            glm::vec3 forward;
            forward.x = cos(glm::radians(transform.rotation.x)) * cos(glm::radians(transform.rotation.y));
            forward.y = sin(glm::radians(transform.rotation.x));
            forward.z = cos(glm::radians(transform.rotation.x)) * sin(glm::radians(transform.rotation.y));
            forward = glm::normalize(forward);

            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
            glm::vec3 up = glm::normalize(glm::cross(right, forward));

			

            glm::vec3 move =
                forward * input.movement.z +
                right * input.movement.x +
                up * input.movement.y;

            transform.position += move * cam.moveSpeed * dt;

			std::cout << "Camera Pos: " << transform.position.x << ", " << transform.position.y << ", " << transform.position.z << "\n";
			std::cout << "Camera Rot: " << transform.rotation.x << ", " << transform.rotation.y << ", " << transform.rotation.z << "\n";
        }
    }

private:
    Registry& registry;
};
