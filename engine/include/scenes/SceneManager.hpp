#pragma once

#include <memory>

class Scene;

/**
 * @class SceneManager
 * @brief Manages loading, unloading, updating, and switching between active Scenes.
 */
class SceneManager {
public:
    /**
     * @brief Switches the active scene, triggering unload on the old and load on the new scene.
     * @param nextScene Unique pointer to the incoming scene.
     */
    void changeScene(std::unique_ptr<Scene> nextScene);
    /**
     * @brief Updates the active scene context.
     * @param dt Delta time in seconds.
     */
    void update(float dt);
    /**
     * @brief Gets the currently active scene.
     * @return Raw pointer to active scene.
     */
    Scene* getCurrentScene() const { return currentScene.get(); }

private:
    /** @brief Unique pointer holding the current active scene context. */
    std::unique_ptr<Scene> currentScene;
};
