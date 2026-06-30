#include "scenes/SceneManager.hpp"

#include "scenes/Scene.hpp"

void SceneManager::changeScene(std::unique_ptr<Scene> nextScene) {
    if (currentScene) {
        currentScene->unload();
    }

    currentScene = std::move(nextScene);

    if (currentScene) {
        currentScene->load();
    }
}

void SceneManager::update(float dt) {
    if (currentScene) {
        currentScene->update(dt);
    }
}
