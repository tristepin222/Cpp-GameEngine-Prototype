#pragma once
#include <cstdint>
#include <bitset>
#include <limits>
#include <functional>

class Registry; // forward declaration

class Entity {
public:
    using IdType = std::uint32_t;
    static constexpr IdType INVALID_ENTITY = std::numeric_limits<IdType>::max();

    Entity() : registry(nullptr), id(INVALID_ENTITY) {}
    Entity(Registry* reg, IdType i) : registry(reg), id(i) {}

    IdType getId() const { return id; }

    template<typename T>
    T& getComponent();

    template<typename T>
    T* tryGetComponent();

    template<typename T>
    bool hasComponent();

    bool operator==(const Entity& other) const { return id == other.id; }
    bool operator!=(const Entity& other) const { return id != other.id; }

private:
    Registry* registry;
    IdType id;
};

constexpr std::size_t MAX_COMPONENTS = 64;
using ComponentMask = std::bitset<MAX_COMPONENTS>;

// Include Registry after Entity is fully defined
#include "Registry.hpp"

// ------------------- Template Implementations -------------------
template<typename T>
T& Entity::getComponent() {
    return registry->getRef<T>(*this);
}

template<typename T>
T* Entity::tryGetComponent() {
    return registry->has<T>(*this) ? &registry->getRef<T>(*this) : nullptr;
}

template<typename T>
bool Entity::hasComponent() {
    return registry->has<T>(*this);
}
