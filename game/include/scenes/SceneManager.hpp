#pragma once

#include <memory>

class Scene;

class SceneManager {
public:
    void changeScene(std::unique_ptr<Scene> nextScene);
    void update(float dt);
    Scene* getCurrentScene() const { return currentScene.get(); }

private:
    std::unique_ptr<Scene> currentScene;
};
