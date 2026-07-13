#pragma once
#include "ecs/Entity.hpp"
#include "ecs/System.hpp"
#include <glm/glm.hpp>

class Registry;
class VulkanRenderer;
struct EditorModeState;

// @reflect
struct PhysgunScript {
    float Kp = 450.0f; // @reflect
    float Kd = 25.0f; // @reflect
    float holdDistance = 5.0f; // @reflect

    // Diagnostic fields exposed in the inspector
    bool isHolding = false; // @reflect
    float currentHoldDistance = 0.0f; // @reflect
    bool debugShowRay = false; // @reflect
    Entity originEntity; // @reflect

    glm::vec3 rayOrigin{ 0.0f };
    glm::vec3 rayDirection{ 0.0f, 0.0f, -1.0f };

    int updateCount = 0;

    // Internal non-reflected state
    Entity heldEntity;
};

// @reflect
class PhysgunSystem : public System {
private:
    Registry& registry;
    VulkanRenderer& renderer;
    EditorModeState& editorMode;
    bool fKeyPressed = false;
    bool rKeyPressed = false;

public:
    PhysgunSystem(Registry& reg, VulkanRenderer& rend, EditorModeState& mode);
    void update(float dt) override;
};
