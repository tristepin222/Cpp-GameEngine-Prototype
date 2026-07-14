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
        String
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
