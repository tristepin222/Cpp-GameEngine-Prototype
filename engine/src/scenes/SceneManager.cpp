#include "scenes/SceneManager.hpp"

#include "scenes/Scene.hpp"

/**
 * @brief Transitions to a new active scene, unloading the previous one.
 * @param nextScene Unique pointer to the new scene to activate.
 */
void SceneManager::changeScene(std::unique_ptr<Scene> nextScene) {
    if (currentScene) {
        currentScene->unload();
    }

    currentScene = std::move(nextScene);

    if (currentScene) {
        currentScene->load();
    }
}

/**
 * @brief Updates the currently active scene.
 * @param dt Frame time interval.
 */
void SceneManager::update(float dt) {
    if (currentScene) {
        currentScene->update(dt);
    }
}
