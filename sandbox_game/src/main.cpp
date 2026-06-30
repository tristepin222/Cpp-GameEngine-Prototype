#include <GLFW/glfw3.h>
#include <iostream>
#include "renderer/VulkanRenderer.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
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
#include "scenes/ComponentSerializerRegistry.hpp"
#include "GameMetadataComponent.hpp"

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "ECS Vulkan Engine", nullptr, nullptr);

    VulkanRenderer renderer(window);
    Registry registry;
    EditorModeState editorMode;

    // Register custom game-level component with serialization registry
    ComponentSerializerRegistry::getInstance().registerComponent(
        "GameMetadata",
        [](Registry& reg, Entity entity, std::ostream& out, int indent) {
            if (auto* comp = reg.get<GameMetadataComponent>(entity)) {
                out << ",\n" << JSONUtils::indent(indent) << "\"importance\": " << comp->importance << ",\n";
                out << JSONUtils::indent(indent) << "\"tag\": " << JSONUtils::quote(comp->tag);
            }
        },
        [](Registry& reg, VulkanRenderer&, Entity entity, const std::string& json) {
            float importance = 0.0f;
            if (JSONUtils::extractFloatValue(json, "importance", importance)) {
                std::string tag = JSONUtils::extractStringValue(json, "tag");
                reg.emplace<GameMetadataComponent>(entity, GameMetadataComponent{ importance, tag });
            }
        }
    );

    renderer.createInstanceBuffer(10000);
    // Create systems
    auto renderSystem = std::make_shared<RenderSystem>(registry, renderer);
	auto cameraSystem = std::make_shared<CameraSystem>(registry, renderer);
    auto inputSystem = std::make_shared<InputSystem>(registry, renderer, editorMode);

    SystemManager sysManager;
    sysManager.addSystem(inputSystem);
    sysManager.addSystem(cameraSystem);
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
