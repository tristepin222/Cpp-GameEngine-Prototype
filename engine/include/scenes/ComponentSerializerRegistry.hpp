#pragma once
#include <string>
#include <vector>
#include <functional>
#include <ostream>
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scenes/JSONUtils.hpp"

class VulkanRenderer;

class ComponentSerializerRegistry {
public:
    using SerializerCallback = std::function<void(Registry& registry, Entity entity, std::ostream& out, int indentLevel)>;
    using DeserializerCallback = std::function<void(Registry& registry, VulkanRenderer& renderer, Entity entity, const std::string& entityJson)>;

    struct Registration {
        std::string componentName;
        SerializerCallback serialize;
        DeserializerCallback deserialize;
    };

    static ComponentSerializerRegistry& getInstance() {
        static ComponentSerializerRegistry instance;
        return instance;
    }

    void registerComponent(const std::string& componentName, SerializerCallback serialize, DeserializerCallback deserialize) {
        registrations.push_back({ componentName, serialize, deserialize });
    }

    const std::vector<Registration>& getRegistrations() const {
        return registrations;
    }

private:
    ComponentSerializerRegistry() = default;
    std::vector<Registration> registrations;
};
