#pragma once
#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "core/EngineAPI.hpp"

namespace Engine {

    /**
     * @enum FieldType
     * @brief Supported field types for component reflection.
     */
    enum class FieldType {
        Float,
        Bool,
        Vec3,
        RigidBodyType,
        Entity,
        String,
        Vec2,
        Vec4
    };

    /**
     * @struct ComponentField
     * @brief Reflection metadata for a single field inside a component.
     */
    struct ComponentField {
        std::string name;
        FieldType type;
        size_t offset;
    };

    /**
     * @struct ComponentReflection
     * @brief Reflection metadata and ECS callbacks for a component.
     */
    struct ComponentReflection {
        std::string name;
        std::string category = "General";
        std::vector<ComponentField> fields;


        // Lifecycle callbacks
        std::function<void(Registry&, Entity)> add;
        std::function<bool(Registry&, Entity)> has;
        std::function<void(Registry&, Entity)> remove;
        std::function<void*(Registry&, Entity)> get;
    };

    /**
     * @class ComponentReflectionRegistry
     * @brief Central singleton managing the registered component reflections.
     */
    class ENGINE_API ComponentReflectionRegistry {
    public:
        static ComponentReflectionRegistry& getInstance();

        void registerComponent(const ComponentReflection& refl);
        const std::vector<ComponentReflection>& getReflections() const;

    private:
        ComponentReflectionRegistry() = default;
        std::vector<ComponentReflection> reflections;
    };

} // namespace Engine

// Forward declarations for type deduction specializations
enum class RigidBodyType;
struct RotationField;

namespace Engine {

    template<typename T> struct DeduceFieldType;
    template<> struct DeduceFieldType<float> { static constexpr FieldType value = FieldType::Float; };
    template<> struct DeduceFieldType<double> { static constexpr FieldType value = FieldType::Float; };
    template<> struct DeduceFieldType<int> { static constexpr FieldType value = FieldType::Float; };
    template<> struct DeduceFieldType<bool> { static constexpr FieldType value = FieldType::Bool; };
    template<> struct DeduceFieldType<glm::vec2> { static constexpr FieldType value = FieldType::Vec2; };
    template<> struct DeduceFieldType<glm::vec3> { static constexpr FieldType value = FieldType::Vec3; };
    template<> struct DeduceFieldType<RotationField> { static constexpr FieldType value = FieldType::Vec3; };
    template<> struct DeduceFieldType<glm::vec4> { static constexpr FieldType value = FieldType::Vec4; };
    template<> struct DeduceFieldType<std::string> { static constexpr FieldType value = FieldType::String; };
    template<> struct DeduceFieldType<Entity> { static constexpr FieldType value = FieldType::Entity; };
    template<> struct DeduceFieldType<RigidBodyType> { static constexpr FieldType value = FieldType::RigidBodyType; };


} // namespace Engine

#define REFLECT_FIELD(Type, FieldName) \
    refl.fields.push_back({ \
        #FieldName, \
        Engine::DeduceFieldType<std::decay_t<decltype(std::declval<Type>().FieldName)>>::value, \
        offsetof(Type, FieldName) \
    });

/**
 * @brief Unity-style 1-line macro for auto-registering custom components into the engine.
 * Automatically adds the component to "+ Add Component" menus, Inspector window, and JSON save/load.
 * Example:
 *   REGISTER_COMPONENT(HealthComponent, "Gameplay");
 */
#define REGISTER_COMPONENT_EXPAND(Type, CategoryName, Counter) \
    namespace { \
        struct AutoRegisterComp_##Counter { \
            AutoRegisterComp_##Counter() { \
                Engine::ComponentReflection refl; \
                refl.name = #Type; \
                size_t scopePos = refl.name.rfind("::"); \
                if (scopePos != std::string::npos) refl.name = refl.name.substr(scopePos + 2); \
                if (refl.name.size() > 9 && refl.name.rfind("Component") == refl.name.size() - 9) { \
                    refl.name = refl.name.substr(0, refl.name.size() - 9); \
                } \
                refl.category = CategoryName; \
                refl.add = [](Registry& reg, Entity e) { reg.emplace<Type>(e, Type{}); }; \
                refl.has = [](Registry& reg, Entity e) { return reg.has<Type>(e); }; \
                refl.remove = [](Registry& reg, Entity e) { reg.remove<Type>(e); }; \
                refl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<Type>(e)); }; \
                Engine::ComponentReflectionRegistry::getInstance().registerComponent(refl); \
            } \
        }; \
        static AutoRegisterComp_##Counter global_autoRegComp_##Counter; \
    }

#define REGISTER_COMPONENT_CONCAT(Type, CategoryName, Counter) REGISTER_COMPONENT_EXPAND(Type, CategoryName, Counter)
#define REGISTER_COMPONENT(Type, CategoryName) REGISTER_COMPONENT_CONCAT(Type, CategoryName, __COUNTER__)

#define REFLECT_COMPONENT_EXPAND(Type, CategoryName, Counter, InitFunc) \
    namespace { \
        struct AutoRegisterComp_##Counter { \
            AutoRegisterComp_##Counter() { \
                Engine::ComponentReflection refl; \
                refl.name = #Type; \
                size_t scopePos = refl.name.rfind("::"); \
                if (scopePos != std::string::npos) refl.name = refl.name.substr(scopePos + 2); \
                if (refl.name.size() > 9 && refl.name.rfind("Component") == refl.name.size() - 9) { \
                    refl.name = refl.name.substr(0, refl.name.size() - 9); \
                } \
                refl.category = CategoryName; \
                refl.add = [](Registry& reg, Entity e) { reg.emplace<Type>(e, Type{}); }; \
                refl.has = [](Registry& reg, Entity e) { return reg.has<Type>(e); }; \
                refl.remove = [](Registry& reg, Entity e) { reg.remove<Type>(e); }; \
                refl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<Type>(e)); }; \
                auto init = InitFunc; \
                init(refl); \
                Engine::ComponentReflectionRegistry::getInstance().registerComponent(refl); \
            } \
        }; \
        static AutoRegisterComp_##Counter global_autoRegComp_##Counter; \
    }

#define REFLECT_COMPONENT_CONCAT(Type, CategoryName, Counter, InitFunc) REFLECT_COMPONENT_EXPAND(Type, CategoryName, Counter, InitFunc)
#define REFLECT_COMPONENT(Type, CategoryName, InitFunc) REFLECT_COMPONENT_CONCAT(Type, CategoryName, __COUNTER__, InitFunc)










