#pragma once
#include <vector>
#include <stack>
#include <array>
#include <algorithm>
#include "Entity.hpp"
#include "EntityHash.hpp"

class EntityManager {
public:
    static constexpr std::size_t MAX_ENTITIES = 10000;

    EntityManager() {
        for (Entity::IdType i = 0; i < MAX_ENTITIES; ++i)
            freeIds.push(i);
    }

    Entity create() {
        if (freeIds.empty()) return Entity(Entity::INVALID_ENTITY);
        Entity::IdType id = freeIds.top();
        freeIds.pop();
        alive.push_back(id);
        masks[id].reset();
        return Entity(id);
    }

    void destroy(Entity e) {
        if (e.getId() == Entity::INVALID_ENTITY) return;
        masks[e.getId()].reset();
        freeIds.push(e.getId());
        alive.erase(std::remove(alive.begin(), alive.end(), e.getId()), alive.end());
    }

    ComponentMask& getMask(Entity e) {
        return masks[e.getId()];
    }

    const std::vector<Entity::IdType>& getAlive() const {
        return alive;
    }

private:
    std::vector<Entity::IdType> alive;
    std::stack<Entity::IdType> freeIds;
    std::array<ComponentMask, MAX_ENTITIES> masks;
};
