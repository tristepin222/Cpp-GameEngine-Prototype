#include "meta/ComponentReflection.hpp"
#include "ecs/components/RigidBody.hpp"
#include "ecs/components/PlayerControllerComponent.hpp"
#include <cstddef>

namespace Engine {

    ComponentReflectionRegistry& ComponentReflectionRegistry::getInstance() {
        static ComponentReflectionRegistry instance;
        static bool initialized = false;
        if (!initialized) {
            initialized = true;

            // 1. RigidBodyComponent Reflection
            ComponentReflection rbRefl;
            rbRefl.name = "RigidBody";
            rbRefl.fields = {
                { "rbType", FieldType::RigidBodyType, offsetof(RigidBodyComponent, type) },
                { "rbMass", FieldType::Float, offsetof(RigidBodyComponent, mass) },
                { "rbVelX", FieldType::Float, offsetof(RigidBodyComponent, velocity) + offsetof(glm::vec3, x) },
                { "rbVelY", FieldType::Float, offsetof(RigidBodyComponent, velocity) + offsetof(glm::vec3, y) },
                { "rbVelZ", FieldType::Float, offsetof(RigidBodyComponent, velocity) + offsetof(glm::vec3, z) },
                { "rbGravityScale", FieldType::Float, offsetof(RigidBodyComponent, gravityScale) },
                { "rbRestitution", FieldType::Float, offsetof(RigidBodyComponent, restitution) },
                { "rbFriction", FieldType::Float, offsetof(RigidBodyComponent, friction) },
                { "rbLinearDrag", FieldType::Float, offsetof(RigidBodyComponent, linearDrag) },
                { "rbAngularDrag", FieldType::Float, offsetof(RigidBodyComponent, angularDrag) },
                { "rbFreezePX", FieldType::Bool, offsetof(RigidBodyComponent, freezePositionX) },
                { "rbFreezePY", FieldType::Bool, offsetof(RigidBodyComponent, freezePositionY) },
                { "rbFreezePZ", FieldType::Bool, offsetof(RigidBodyComponent, freezePositionZ) },
                { "rbFreezeRX", FieldType::Bool, offsetof(RigidBodyComponent, freezeRotationX) },
                { "rbFreezeRY", FieldType::Bool, offsetof(RigidBodyComponent, freezeRotationY) },
                { "rbFreezeRZ", FieldType::Bool, offsetof(RigidBodyComponent, freezeRotationZ) }
            };
            rbRefl.add = [](Registry& reg, Entity e) { reg.emplace<RigidBodyComponent>(e, RigidBodyComponent{}); };
            rbRefl.has = [](Registry& reg, Entity e) { return reg.has<RigidBodyComponent>(e); };
            rbRefl.remove = [](Registry& reg, Entity e) { reg.remove<RigidBodyComponent>(e); };
            rbRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<RigidBodyComponent>(e)); };
            instance.registerComponent(rbRefl);

            // 2. PlayerControllerComponent Reflection
            ComponentReflection pcRefl;
            pcRefl.name = "PlayerController";
            pcRefl.fields = {
                { "playerSpeed", FieldType::Float, offsetof(PlayerControllerComponent, speed) },
                { "playerJumpForce", FieldType::Float, offsetof(PlayerControllerComponent, jumpForce) },
                { "playerInteractRange", FieldType::Float, offsetof(PlayerControllerComponent, interactRange) }
            };
            pcRefl.add = [](Registry& reg, Entity e) { reg.emplace<PlayerControllerComponent>(e, PlayerControllerComponent{}); };
            pcRefl.has = [](Registry& reg, Entity e) { return reg.has<PlayerControllerComponent>(e); };
            pcRefl.remove = [](Registry& reg, Entity e) { reg.remove<PlayerControllerComponent>(e); };
            pcRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<PlayerControllerComponent>(e)); };
            instance.registerComponent(pcRefl);
        }
        return instance;
    }

    void ComponentReflectionRegistry::registerComponent(const ComponentReflection& refl) {
        reflections.push_back(refl);
    }

    const std::vector<ComponentReflection>& ComponentReflectionRegistry::getReflections() const {
        return reflections;
    }

} // namespace Engine
