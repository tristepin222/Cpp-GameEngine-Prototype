#pragma once
#include "ecs/System.hpp"
#include "ecs/Registry.hpp"
#include "editor/EditorModeState.hpp"
#include "renderer/VulkanRenderer.hpp"
#include <vector>
#include <glm/glm.hpp>

#include "meta/ComponentReflection.hpp"

// [ReflectClass]
struct AStarAgent {
    // [ReflectField]
    int targetX = 0;
    // [ReflectField]
    int targetY = 0;
    // [ReflectField]
    float speed = 2.0f;
    // [ReflectField]
    bool allowDiagonal = true;
    // [ReflectField]
    bool showDebugPath = true;

    // Cached computed path
    std::vector<glm::ivec2> path;

    // Cached values to avoid recalculating the path every frame
    glm::ivec2 lastStart = glm::ivec2(-9999);
    glm::ivec2 lastTarget = glm::ivec2(-9999);
};
REGISTER_COMPONENT(AStarAgent, "AI/AStar Agent");

namespace Engine {
    struct TilemapComponent;
}

namespace AStar {
    /**
     * @brief High-level A* pathfinding calculation on a Tilemap Component.
     */
    std::vector<glm::ivec2> findPath(
        const Engine::TilemapComponent& tilemap,
        const glm::ivec2& start,
        const glm::ivec2& end,
        const std::vector<int>& blockedTileIds = {},
        bool allowDiagonal = true
    );
}

// [ReflectClass]
class AStarSystem : public System {
public:
    AStarSystem(Registry& reg, VulkanRenderer& renderer, EditorModeState& editorMode);
    ~AStarSystem() = default;

    void update(float dt) override;
    void renderDebugUI() override;

private:
    Registry& registry;
    VulkanRenderer& renderer;
    EditorModeState& editorMode;
};
