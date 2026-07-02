#include "core/Application.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "ecs/systems/RenderSystem.hpp"
#include "ecs/systems/CameraSystem.hpp"
#include "ecs/systems/InputSystem.hpp"
#include "ecs/systems/AnimationSystem.hpp"
#include "scenes/Scene.hpp"
#include "scenes/JSONUtils.hpp"
#include "scenes/DefaultScene.hpp"

namespace Engine {

    Application::Application(const ApplicationConfig& cfg) : config(cfg) {
        loadConfig();
        initEngine();
    }

    Application::~Application() {
        cleanupEngine();
    }

    void Application::loadConfig() {
        std::ifstream file("project.settings");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();

            std::string titleVal = JSONUtils::extractStringValue(content, "title");
            if (!titleVal.empty()) config.title = titleVal;

            float w = 0.0f, h = 0.0f;
            if (JSONUtils::extractFloatValue(content, "width", w)) config.width = static_cast<int>(w);
            if (JSONUtils::extractFloatValue(content, "height", h)) config.height = static_cast<int>(h);

            if (content.find("\"enableEditor\": false") != std::string::npos) {
                config.enableEditor = false;
            } else if (content.find("\"enableEditor\": true") != std::string::npos) {
                config.enableEditor = true;
            }

            std::string sceneVal = JSONUtils::extractStringValue(content, "startScenePath");
            if (!sceneVal.empty()) config.startScenePath = sceneVal;

            std::cout << "[Application] Config loaded from project.settings" << std::endl;
        } else {
            std::cout << "[Application] project.settings not found, using configurations from code" << std::endl;
        }
    }

    void Application::initEngine() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
        if (!window) {
            throw std::runtime_error("Failed to create GLFW window");
        }

        renderer = std::make_unique<VulkanRenderer>(window);
        renderer->createInstanceBuffer(10000);

        // Instantiate standard engine systems
        renderSystem = std::make_shared<RenderSystem>(registry, *renderer);
        auto cameraSystem = std::make_shared<CameraSystem>(registry, *renderer);
        auto inputSystem = std::make_shared<InputSystem>(registry, *renderer, editorMode);
        auto animationSystem = std::make_shared<AnimationSystem>(registry, *renderer);

        systemManager.addSystem(inputSystem);
        systemManager.addSystem(cameraSystem);
        systemManager.addSystem(animationSystem);
        systemManager.addSystem(renderSystem);

        // Setup initial editor fly mode based on whether editor UI is present
        if (!config.enableEditor) {
            editorMode.flyMode = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            editorMode.flyMode = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

            // Initialize editor UI overlay
            editorUI = std::make_unique<EditorUI>(registry, *renderer, sceneManager, editorMode);
            editorUI->initialize(window);
        }

        // Load generic data-driven scene automatically
        sceneManager.changeScene(std::make_unique<DefaultScene>(registry, *renderer, config.startScenePath));

        running = true;
    }

    void Application::cleanupEngine() {
        if (editorUI) {
            editorUI->shutdown();
            editorUI.reset();
        }
        if (renderer) {
            renderer->cleanup();
            renderer.reset();
        }
        if (window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
    }

    void Application::run() {
        onStart();

        while (running && !renderer->shouldClose()) {
            glfwPollEvents();
            float dt = renderer->getDeltaTime();

            // Run user update callback
            onUpdate(dt);

            // Tick scenes and ECS systems
            sceneManager.update(dt);
            systemManager.updateAll(dt);

            if (config.enableEditor && editorUI) {
                // Draw ImGui editor and render current frames
                editorUI->beginFrame();
                editorUI->drawPanels();

                renderSystem->drawFrame([this](VkCommandBuffer cmd) {
                    editorUI->render(cmd);
                });
            } else {
                // Standalone mode: draw viewport fullscreen (no UI)
                renderSystem->drawFrame();
            }
        }

        onShutdown();
    }

    void Application::quit() {
        running = false;
    }

} // namespace Engine
