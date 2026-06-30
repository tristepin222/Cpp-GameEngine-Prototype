#pragma once
#include "Entity.hpp"
#include "EntityManager.hpp"
#include <functional>
#include <vector>
#include <algorithm>

class System {
public:
    virtual ~System() = default;
    virtual void update(float dt) = 0;
    void addEntity(Entity e) { entities.push_back(e); }
    void removeEntity(Entity e) {
        entities.erase(std::remove(entities.begin(), entities.end(), e), entities.end());
    }
protected:
    std::vector<Entity> entities;
};
