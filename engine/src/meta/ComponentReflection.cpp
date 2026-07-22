#include "meta/ComponentReflection.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/RigidBody.hpp"
#include "ecs/components/PlayerControllerComponent.hpp"
#include "ecs/components/AudioSource.hpp"
#include "ecs/components/AudioListener.hpp"
#include "ecs/components/Tilemap.hpp"
#include "ecs/components/UIComponents.hpp"
#include "ecs/components/LightComponent.hpp"
#include <cstddef>

namespace Engine {

    ComponentReflectionRegistry& ComponentReflectionRegistry::getInstance() {
        static ComponentReflectionRegistry instance;
        static bool initialized = false;
        if (!initialized) {
            initialized = true;

            // 0. Transform Reflection
            ComponentReflection transRefl;
            transRefl.name = "Transform";
            transRefl.fields = {
                { "position", FieldType::Vec3, offsetof(Transform, position) },
                { "rotation", FieldType::Vec3, offsetof(Transform, rotation) },
                { "scale", FieldType::Vec3, offsetof(Transform, scale) }
            };
            transRefl.add = [](Registry& reg, Entity e) { reg.emplace<Transform>(e, Transform{}); };
            transRefl.has = [](Registry& reg, Entity e) { return reg.has<Transform>(e); };
            transRefl.remove = [](Registry& reg, Entity e) { reg.remove<Transform>(e); };
            transRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<Transform>(e)); };
            instance.registerComponent(transRefl);

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

            // 3. AudioSourceComponent Reflection
            ComponentReflection audioSrcRefl;
            audioSrcRefl.name = "AudioSource";
            audioSrcRefl.fields = {
                { "clipPath", FieldType::String, offsetof(AudioSourceComponent, clipPath) },
                { "volume", FieldType::Float, offsetof(AudioSourceComponent, volume) },
                { "pitch", FieldType::Float, offsetof(AudioSourceComponent, pitch) },
                { "loop", FieldType::Bool, offsetof(AudioSourceComponent, loop) },
                { "playOnAwake", FieldType::Bool, offsetof(AudioSourceComponent, playOnAwake) },
                { "spatialized", FieldType::Bool, offsetof(AudioSourceComponent, spatialized) },
                { "minDistance", FieldType::Float, offsetof(AudioSourceComponent, minDistance) },
                { "maxDistance", FieldType::Float, offsetof(AudioSourceComponent, maxDistance) }
            };
            audioSrcRefl.add = [](Registry& reg, Entity e) { reg.emplace<AudioSourceComponent>(e, AudioSourceComponent{}); };
            audioSrcRefl.has = [](Registry& reg, Entity e) { return reg.has<AudioSourceComponent>(e); };
            audioSrcRefl.remove = [](Registry& reg, Entity e) { reg.remove<AudioSourceComponent>(e); };
            audioSrcRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<AudioSourceComponent>(e)); };
            instance.registerComponent(audioSrcRefl);

            // 4. AudioListenerComponent Reflection
            ComponentReflection audioListRefl;
            audioListRefl.name = "AudioListener";
            audioListRefl.fields = {
                { "active", FieldType::Bool, offsetof(AudioListenerComponent, active) }
            };
            audioListRefl.add = [](Registry& reg, Entity e) { reg.emplace<AudioListenerComponent>(e, AudioListenerComponent{}); };
            audioListRefl.has = [](Registry& reg, Entity e) { return reg.has<AudioListenerComponent>(e); };
            audioListRefl.remove = [](Registry& reg, Entity e) { reg.remove<AudioListenerComponent>(e); };
            audioListRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<AudioListenerComponent>(e)); };
            instance.registerComponent(audioListRefl);
            // 5. TilemapComponent Reflection
            ComponentReflection tmRefl;
            tmRefl.name = "Tilemap";
            tmRefl.fields = {
                { "tileSize", FieldType::Float, offsetof(TilemapComponent, tileSize) },
                { "tilesetPath", FieldType::String, offsetof(TilemapComponent, tilesetPath) }
            };
            tmRefl.add = [](Registry& reg, Entity e) { reg.emplace<TilemapComponent>(e, TilemapComponent{}); };
            tmRefl.has = [](Registry& reg, Entity e) { return reg.has<TilemapComponent>(e); };
            tmRefl.remove = [](Registry& reg, Entity e) { reg.remove<TilemapComponent>(e); };
            tmRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<TilemapComponent>(e)); };
            instance.registerComponent(tmRefl);
 
            // 6. CanvasComponent Reflection
            ComponentReflection canvasRefl;
            canvasRefl.name = "Canvas";
            canvasRefl.fields = {
                { "isScreenSpace", FieldType::Bool, offsetof(CanvasComponent, isScreenSpace) }
            };
            canvasRefl.add = [](Registry& reg, Entity e) { reg.emplace<CanvasComponent>(e, CanvasComponent{}); };
            canvasRefl.has = [](Registry& reg, Entity e) { return reg.has<CanvasComponent>(e); };
            canvasRefl.remove = [](Registry& reg, Entity e) { reg.remove<CanvasComponent>(e); };
            canvasRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<CanvasComponent>(e)); };
            instance.registerComponent(canvasRefl);

            // 7. RectTransform Reflection
            ComponentReflection rectRefl;
            rectRefl.name = "RectTransform";
            rectRefl.fields = {
                { "anchorMin", FieldType::Vec2, offsetof(RectTransform, anchorMin) },
                { "anchorMax", FieldType::Vec2, offsetof(RectTransform, anchorMax) },
                { "anchoredPosition", FieldType::Vec2, offsetof(RectTransform, anchoredPosition) },
                { "sizeDelta", FieldType::Vec2, offsetof(RectTransform, sizeDelta) },
                { "pivot", FieldType::Vec2, offsetof(RectTransform, pivot) }
            };
            rectRefl.add = [](Registry& reg, Entity e) { reg.emplace<RectTransform>(e, RectTransform{}); };
            rectRefl.has = [](Registry& reg, Entity e) { return reg.has<RectTransform>(e); };
            rectRefl.remove = [](Registry& reg, Entity e) { reg.remove<RectTransform>(e); };
            rectRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<RectTransform>(e)); };
            instance.registerComponent(rectRefl);

            // 8. UIPanelComponent Reflection
            ComponentReflection panelRefl;
            panelRefl.name = "UIPanel";
            panelRefl.fields = {
                { "color", FieldType::Vec4, offsetof(UIPanelComponent, color) },
                { "borderRadius", FieldType::Float, offsetof(UIPanelComponent, borderRadius) }
            };
            panelRefl.add = [](Registry& reg, Entity e) { reg.emplace<UIPanelComponent>(e, UIPanelComponent{}); };
            panelRefl.has = [](Registry& reg, Entity e) { return reg.has<UIPanelComponent>(e); };
            panelRefl.remove = [](Registry& reg, Entity e) { reg.remove<UIPanelComponent>(e); };
            panelRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<UIPanelComponent>(e)); };
            instance.registerComponent(panelRefl);

            // 9. UIImageComponent Reflection
            ComponentReflection imgRefl;
            imgRefl.name = "UIImage";
            imgRefl.fields = {
                { "texturePath", FieldType::String, offsetof(UIImageComponent, texturePath) },
                { "tintColor", FieldType::Vec4, offsetof(UIImageComponent, tintColor) }
            };
            imgRefl.add = [](Registry& reg, Entity e) { reg.emplace<UIImageComponent>(e, UIImageComponent{}); };
            imgRefl.has = [](Registry& reg, Entity e) { return reg.has<UIImageComponent>(e); };
            imgRefl.remove = [](Registry& reg, Entity e) { reg.remove<UIImageComponent>(e); };
            imgRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<UIImageComponent>(e)); };
            instance.registerComponent(imgRefl);

            // 10. UITextComponent Reflection
            ComponentReflection txtRefl;
            txtRefl.name = "UIText";
            txtRefl.fields = {
                { "text", FieldType::String, offsetof(UITextComponent, text) },
                { "color", FieldType::Vec4, offsetof(UITextComponent, color) },
                { "fontSize", FieldType::Float, offsetof(UITextComponent, fontSize) },
                { "alignCenter", FieldType::Bool, offsetof(UITextComponent, alignCenter) }
            };
            txtRefl.add = [](Registry& reg, Entity e) { reg.emplace<UITextComponent>(e, UITextComponent{}); };
            txtRefl.has = [](Registry& reg, Entity e) { return reg.has<UITextComponent>(e); };
            txtRefl.remove = [](Registry& reg, Entity e) { reg.remove<UITextComponent>(e); };
            txtRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<UITextComponent>(e)); };
            instance.registerComponent(txtRefl);

            // 11. UIButtonComponent Reflection
            ComponentReflection btnRefl;
            btnRefl.name = "UIButton";
            btnRefl.fields = {
                { "label", FieldType::String, offsetof(UIButtonComponent, label) },
                { "normalColor", FieldType::Vec4, offsetof(UIButtonComponent, normalColor) },
                { "hoverColor", FieldType::Vec4, offsetof(UIButtonComponent, hoverColor) },
                { "pressedColor", FieldType::Vec4, offsetof(UIButtonComponent, pressedColor) },
                { "textColor", FieldType::Vec4, offsetof(UIButtonComponent, textColor) },
                { "clickEventName", FieldType::String, offsetof(UIButtonComponent, clickEventName) }
            };
            btnRefl.add = [](Registry& reg, Entity e) { reg.emplace<UIButtonComponent>(e, UIButtonComponent{}); };
            btnRefl.has = [](Registry& reg, Entity e) { return reg.has<UIButtonComponent>(e); };
            btnRefl.remove = [](Registry& reg, Entity e) { reg.remove<UIButtonComponent>(e); };
            btnRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<UIButtonComponent>(e)); };
            instance.registerComponent(btnRefl);

            // 12. LightComponent Reflection
            ComponentReflection lightRefl;
            lightRefl.name = "Light";
            lightRefl.fields = {
                { "color", FieldType::Vec3, offsetof(LightComponent, color) },
                { "intensity", FieldType::Float, offsetof(LightComponent, intensity) },
                { "range", FieldType::Float, offsetof(LightComponent, range) }
            };
            lightRefl.add = [](Registry& reg, Entity e) { reg.emplace<LightComponent>(e, LightComponent{}); };
            lightRefl.has = [](Registry& reg, Entity e) { return reg.has<LightComponent>(e); };
            lightRefl.remove = [](Registry& reg, Entity e) { reg.remove<LightComponent>(e); };
            lightRefl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<LightComponent>(e)); };
            instance.registerComponent(lightRefl);
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
