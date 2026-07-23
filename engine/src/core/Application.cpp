#include "core/Application.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include "ecs/systems/RenderSystem.hpp"
#include "ecs/systems/CameraSystem.hpp"
#include "ecs/systems/InputSystem.hpp"
#include "ecs/systems/AnimationSystem.hpp"
#include "ecs/systems/PhysicsSystem.hpp"
#include "ecs/systems/PlayerControllerSystem.hpp"
#include "ecs/systems/AudioSystem.hpp"
#include "ecs/systems/TilemapSystem.hpp"
#include "ecs/systems/UISystem.hpp"
#include "scenes/Scene.hpp"
#include "scenes/JSONUtils.hpp"
#include "scenes/DefaultScene.hpp"
#include "scenes/SceneManagement.hpp"
#include "core/JobSystem.hpp"
#include "ecs/components/EditorCamera.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/inputComponent.hpp"


namespace Engine {

    Application::Application(const ApplicationConfig& cfg) : config(cfg) {
        // Resolve the project path and change CWD to it so that all relative asset
        // paths (assets/, scenes/, shaders/) work regardless of where the exe lives.
        std::filesystem::path projectPath = std::filesystem::absolute(config.projectPath);
        if (std::filesystem::is_directory(projectPath)) {
            std::filesystem::current_path(projectPath);
            config.projectPath = projectPath.string();
            std::cout << "[Application] Working directory set to project: " << projectPath.string() << std::endl;
        } else {
            std::cerr << "[Application] WARNING: projectPath does not exist: " << projectPath.string() << std::endl;
        }
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

            // The editor state (enableEditor) is strictly determined by the entry point binary (editor.exe or game_runtime.exe) and shouldn't be overridden by project settings.

            std::string sceneVal = JSONUtils::extractStringValue(content, "startScenePath");
            if (!sceneVal.empty()) config.startScenePath = sceneVal;

            std::cout << "[Application] Config loaded from project.settings" << std::endl;
        } else {
            std::cout << "[Application] project.settings not found, using configurations from code" << std::endl;
        }
    }

    void Application::initEngine() {
        JobSystem::getInstance().initialize(); // Initialize Job System thread pool

        std::cout << "[Application] Job System initialized successfully" << std::endl;

        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
        if (!window) {
            throw std::runtime_error("Failed to create GLFW window");
        }

        std::cout << "[Application] GLFW window created successfully" << std::endl;

        renderer = std::make_unique<VulkanRenderer>(window, config.exeDir);

        std::cout << "[Application] VulkanRenderer initialized successfully" << std::endl;

        // Instantiate standard engine systems
        renderSystem = std::make_shared<RenderSystem>(registry, *renderer);
        auto cameraSystem = std::make_shared<CameraSystem>(registry, *renderer, editorMode);
        auto inputSystem = std::make_shared<InputSystem>(registry, *renderer, editorMode);
        auto animationSystem = std::make_shared<AnimationSystem>(registry, *renderer, editorMode);
        auto physicsSystem = std::make_shared<PhysicsSystem>(registry, editorMode);
        auto playerControllerSystem = std::make_shared<PlayerControllerSystem>(registry, *renderer, editorMode);
        auto audioSystem = std::make_shared<AudioSystem>(registry, editorMode);
        auto tilemapSystem = std::make_shared<TilemapSystem>(registry, *renderer);
        uiSystem = std::make_shared<UISystem>(registry, *renderer);

        systemManager.addSystem(inputSystem);
        systemManager.addSystem(cameraSystem);
        systemManager.addSystem(tilemapSystem);
        systemManager.addSystem(physicsSystem);
        systemManager.addSystem(animationSystem);
        systemManager.addSystem(playerControllerSystem);
        systemManager.addSystem(audioSystem);
        systemManager.addSystem(renderSystem);
        systemManager.addSystem(uiSystem);

        // Spawn persistent Editor Camera
        Entity editorCam = registry.create();
        registry.emplace<Name>(editorCam, Name{"EditorCamera"});
        registry.emplace<Transform>(editorCam, Transform{ glm::vec3(0.0f, 2.0f, 5.0f) });
        registry.emplace<Camera>(editorCam, Camera{});
        registry.emplace<InputComponent>(editorCam, InputComponent{});
        registry.emplace<EditorCamera>(editorCam, EditorCamera{});

        // Initialize editor UI overlay (always initialized to support ImGui Game UI rendering)
        editorUI = std::make_unique<EditorUI>(registry, *renderer, sceneManager, editorMode, config.startScenePath,
            [this](const std::string& projectPath, const std::string& outPath) {
                return buildGame(projectPath, outPath);
            });
        editorUI->initialize(window);

        // Setup initial editor fly mode based on whether editor UI is present
        if (!config.enableEditor) {
            editorMode.isPlaying = true;
            editorMode.flyMode = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            editorMode.flyMode = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        // Load engine-level plugins, then project-level scripts
        pluginManager = std::make_unique<PluginManager>(registry, systemManager, *renderer, editorMode);
        pluginManager->setExeDirectory(config.exeDir);
        pluginManager->loadPlugins();
        pluginManager->loadScripts(config.projectPath);

        sceneManager.setContext(&registry, renderer.get());
        SceneManagement::setSceneManager(&sceneManager);
        sceneManager.changeScene(std::make_unique<DefaultScene>(registry, *renderer, config.startScenePath));

        running = true;

        std::cout << "[Application] Engine initialized successfully" << std::endl;
    }

    void Application::cleanupEngine() {
        // 1. Stop the game loop / flush pending work first
        JobSystem::getInstance().shutdown();

        // 2. Unload scripts/plugins (they may reference Vulkan objects via systems)
        if (pluginManager) {
            pluginManager->unloadPlugins();
            pluginManager.reset();
        }

        // 3. Shut down all ECS systems (they hold Vulkan pipelines, descriptors, etc.)
        systemManager.clear();
        renderSystem.reset();

        // 4. Unload current scene and destroy all entities/components (releases any Vulkan-backed resources)
        sceneManager.changeScene(nullptr);
        registry.clear();

        // 5. Shut down the editor UI (destroys ImGui Vulkan backend, descriptor sets)
        if (editorUI) {
            editorUI->shutdown();
            editorUI.reset();
        }

        // 6. Destroy the Vulkan renderer (device, swapchain, instance)
        if (renderer) {
            renderer->cleanup();
            renderer.reset();
        }

        // 7. Destroy the window and GLFW
        if (window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
    }

    int Application::buildGame(const std::string& projectPath, const std::string& outPath) {
        if (pluginManager) {
            pluginManager->unloadPlugins();
        }

        std::filesystem::path outPathFs = std::filesystem::absolute(outPath);
        outPathFs = outPathFs.lexically_normal();
        std::string outPathArg = outPathFs.string();
        if (!outPathArg.empty() && (outPathArg.back() == '\\' || outPathArg.back() == '/')) {
            outPathArg.pop_back();
        }

        std::string batchPath = "build_game_package.bat";
        if (!config.exeDir.empty()) {
            std::filesystem::path exeBatch = std::filesystem::path(config.exeDir) / "build_game_package.bat";
            if (std::filesystem::exists(exeBatch)) {
                batchPath = exeBatch.string();
            }
        }

        std::string cmd = "\"\"" + batchPath + "\" \"" + projectPath + "\" \"" + outPathArg + "\"\"";
        std::cout << "[BuildSystem] Running: " << cmd << std::endl;

        int result = std::system(cmd.c_str());

        if (pluginManager) {
            pluginManager->loadPlugins();
            pluginManager->loadScripts(config.projectPath);
        }

        return result;
    }

    void Application::run() {
        onStart();

        while (running && !renderer->shouldClose()) {
            glfwPollEvents();

            if (editorMode.pendingPlay) {
                editorMode.pendingPlay = false;
                if (Scene* currentScene = sceneManager.getCurrentScene()) {
                    currentScene->saveToFile("assets/scenes/.play_temp.json");
                }
                editorMode.isPlaying = true;
                editorMode.flyMode = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            if (editorMode.pendingStop) {
                editorMode.pendingStop = false;
                editorMode.isPlaying = false;
                editorMode.flyMode = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                if (Scene* currentScene = sceneManager.getCurrentScene()) {
                    currentScene->loadFromFile("assets/scenes/.play_temp.json");
                    try {
                        std::filesystem::remove("assets/scenes/.play_temp.json");
                    } catch (...) {}
                }
            }

            float dt = renderer->getDeltaTime();

            // Run user update callback
            onUpdate(dt);

            // Tick scenes and ECS systems
            sceneManager.update(dt);
            systemManager.updateAll(dt);

            if (uiSystem) {
                uiSystem->setEditorActive(config.enableEditor);
                uiSystem->setPlaying(editorMode.isPlaying);
            }

            if (editorUI) {
                if (config.enableEditor) {
                    editorUI->beginFrame();

                    // Draw Game UI canvas first so it renders beneath editor UI panels and windows
                    if (uiSystem) {
                        uiSystem->draw();
                    }

                    // Draw ImGui editor panels second (menu bar, inspector, floating editor windows)
                    editorUI->drawPanels();

                    renderSystem->drawFrame([this](VkCommandBuffer cmd) {
                        editorUI->render(cmd);
                    });
                } else {

                    // Standalone mode: draw viewport and Game UI fullscreen
                    editorUI->beginFrame();
                    if (uiSystem) {
                        uiSystem->draw();
                    }

                    renderSystem->drawFrame([this](VkCommandBuffer cmd) {
                        editorUI->render(cmd);
                    });
                }
            } else {
                // Fallback: draw viewport fullscreen with no UI overlay if editorUI is somehow null
                renderSystem->drawFrame();
            }
        }

        onShutdown();
    }

    void Application::quit() {
        running = false;
    }

} // namespace Engine
