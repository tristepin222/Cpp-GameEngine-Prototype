#pragma once
#include "ecs/Entity.hpp"
#include "ecs/System.hpp"
#include <glm/glm.hpp>

class Registry;
class VulkanRenderer;
struct EditorModeState;

// [ReflectClass]
struct PhysgunScript {
    // [ReflectField]
    float Kp = 450.0f;
    // [ReflectField]
    float Kd = 25.0f;
    // [ReflectField]
    float holdDistance = 5.0f;

    // Diagnostic fields exposed in the inspector
    // [ReflectField]
    bool isHolding = false;
    // [ReflectField]
    float currentHoldDistance = 0.0f;
    // [ReflectField]
    bool debugShowRay = false;
    // [ReflectField]
    Entity originEntity;

    glm::vec3 rayOrigin{ 0.0f };
    glm::vec3 rayDirection{ 0.0f, 0.0f, -1.0f };

    int updateCount = 0;

    // Internal non-reflected state
    Entity heldEntity;
};

// [ReflectClass]
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

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(PhysgunScript, "Player Interaction");