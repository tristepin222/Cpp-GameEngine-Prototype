#pragma once
#include <glm/glm.hpp>
#include <glfw3.h>
#include "ecs/System.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/inputComponent.hpp"
#include "renderer/VulkanRenderer.hpp"

class InputSystem : public System {
public:
    InputSystem(Registry& reg, VulkanRenderer& renderer)
        : registry(reg), renderer(renderer) {
    }

    void update(float dt) override {
        double x, y;
        static double lastX = 0, lastY = 0;
        glfwGetCursorPos(renderer.getWindow(), &x, &y);

        double dx = x - lastX;
        double dy = lastY - y; // invert Y
        lastX = x;
        lastY = y;

        for (auto [e, input] : registry.view<InputComponent>()) {
            input.look = glm::vec2(dx, dy);

            GLFWwindow* window = renderer.getWindow();
            input.movement = glm::vec3(
                (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS),
                (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS),
                (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            );
        }
    }

private:
    Registry& registry;
    VulkanRenderer& renderer;
};
