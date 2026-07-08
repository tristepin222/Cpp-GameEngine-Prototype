#pragma once
#include "ecs/System.hpp"
#include "ecs/Registry.hpp"
#include "editor/EditorModeState.hpp"
#include "renderer/VulkanRenderer.hpp"

/**
 * @class PlayerControllerSystem
 * @brief System that processes player inputs to drive movement, jumps, and interaction physics.
 */
class PlayerControllerSystem : public System {
public:
    PlayerControllerSystem(Registry& reg, VulkanRenderer& renderer, EditorModeState& editorMode);

    void update(float dt) override;

private:
    Registry& registry;
    VulkanRenderer& renderer;
    EditorModeState& editorMode;
};
