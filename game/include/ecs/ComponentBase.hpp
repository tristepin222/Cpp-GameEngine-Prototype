#pragma once
#include "Entity.hpp"

class Registry; // forward declaration

struct ComponentBase {
    Entity entity; // back-pointer to the owning entity
    Registry* registry;

    template<typename T>
    T& getComponent() {
        return entity.getComponent<T>();
    }

    template<typename T>
    T* tryGetComponent() {
        return entity.tryGetComponent<T>();
    }

    template<typename T>
    bool hasComponent() {
        return entity.hasComponent<T>();
    }
};
