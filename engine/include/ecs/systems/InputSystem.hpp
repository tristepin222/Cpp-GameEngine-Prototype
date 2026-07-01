#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include "ecs/System.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/inputComponent.hpp"
#include "editor/EditorModeState.hpp"
#include "renderer/VulkanRenderer.hpp"

/**
 * @class InputSystem
 * @brief System that handles input from keyboard and mouse, updating InputComponents and switching camera modes.
 */
class InputSystem : public System {
public:
    /**
     * @brief Construct a new Input System object.
     * @param reg Reference to the ECS Registry.
     * @param renderer Reference to the Vulkan Renderer.
     * @param editorMode Reference to the Editor Mode State.
     */
    InputSystem(Registry& reg, VulkanRenderer& renderer, EditorModeState& editorMode)
        : registry(reg), renderer(renderer), editorMode(editorMode) {
    }

    /**
     * @brief Updates inputs for entities and handles cursor mode switches.
     * @param dt Delta time in seconds.
     */
    void update(float dt) override {

        double x, y;
        static double lastX = 0, lastY = 0;
        glfwGetCursorPos(renderer.getWindow(), &x, &y);

        switchMode(lastY, lastX);


        if (!editorMode.flyMode) {
            lastX = x;
            lastY = y;

            for (auto [e, input] : registry.view<InputComponent>()) {
                input.look = glm::vec2(0.0f);
                input.movement = glm::vec3(0.0f);
            }
            return;
        }

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

    /**
     * @brief Toggles between editor/fly mode and handles cursor state cache.
     * @param lastY Reference to stored last Y position of the cursor.
     * @param lastX Reference to stored last X position of the cursor.
     */
    void switchMode(double& lastY, double& lastX) {
        static bool prevTabDown = false;

        GLFWwindow* window = renderer.getWindow();
        bool tabDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;

        bool tabPressed = tabDown && !prevTabDown;
        prevTabDown = tabDown;

        if (tabPressed) {
            editorMode.flyMode = !editorMode.flyMode;

            applyCursorMode();

            double x, y;
            glfwGetCursorPos(window, &x, &y);
            lastX = x;
            lastY = y;
        }
    }


    /**
     * @brief Configures GLFW cursor modes based on current fly/editor state.
     */
    void applyCursorMode() {
        GLFWwindow* window = renderer.getWindow();

        if (editorMode.flyMode) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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
