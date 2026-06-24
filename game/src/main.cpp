#include <GLFW/glfw3.h>
#include <iostream>
#include "renderer/VulkanRenderer.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/RenderSystem.hpp"
#include "ecs/SystemManager.hpp"
#include "ecs/components/primitives.hpp"
#include "ecs/Components/Camera.hpp"
#include "ecs/systems/CameraSystem.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/systems/InputSystem.hpp"
#include "editor/EditorModeState.hpp"
#include "editor/EditorUI.hpp"
#include "scenes/SceneManager.hpp"
#include "scenes/TestScene.hpp"


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "ECS Vulkan Engine", nullptr, nullptr);

    VulkanRenderer renderer(window);
	Registry registry;
    EditorModeState editorMode;

    renderer.createInstanceBuffer(10000);
    // Create systems
    auto movementSystem = std::make_shared<MovementSystem>(registry);
    auto renderSystem = std::make_shared<RenderSystem>(registry, renderer);
	auto cameraSystem = std::make_shared<CameraSystem>(registry, renderer);
    auto inputSystem = std::make_shared<InputSystem>(registry, renderer, editorMode);

    SystemManager sysManager;
    sysManager.addSystem(inputSystem);
    sysManager.addSystem(cameraSystem);
    sysManager.addSystem(movementSystem);
    sysManager.addSystem(renderSystem);

    SceneManager sceneManager;
    sceneManager.changeScene(std::make_unique<TestScene>(registry, renderer));
    EditorUI editorUI(registry, renderer, sceneManager, editorMode);
    editorUI.initialize(window);

    while (!renderer.shouldClose()) {
        glfwPollEvents();
        float dt = renderer.getDeltaTime();

        sceneManager.update(dt);
		sysManager.updateAll(dt);
        editorUI.beginFrame();
        editorUI.drawPanels();
        renderSystem->drawFrame([&editorUI](VkCommandBuffer cmd) {
            editorUI.render(cmd);
        });
    }

    editorUI.shutdown();
    renderer.cleanup();
    glfwTerminate();


    return EXIT_SUCCESS;
}
