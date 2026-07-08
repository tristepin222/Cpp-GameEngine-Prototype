#include "SandboxApp.hpp"
#include "PlayerControllerSystem.hpp"
#include "GameMetadataComponent.hpp"
#include "ecs/components/PlayerControllerComponent.hpp"
#include <imgui.h>

SandboxApp::SandboxApp() : Application() {}

void SandboxApp::onStart() {
    // Register custom game-level components with serialization registry
    registerComponent<GameMetadataComponent>("GameMetadata");

    // Instantiate and register the PlayerControllerSystem with the ECS SystemManager
    auto pcSystem = std::make_shared<PlayerControllerSystem>(registry, getRenderer(), getEditorMode());
    systemManager.addSystem(pcSystem);
}
