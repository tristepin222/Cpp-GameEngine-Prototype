#pragma once
#include <vector>
#include <stack>
#include <array>
#include <algorithm>
#include "Entity.hpp"
#include "EntityHash.hpp"

/**
 * @class EntityManager
 * @brief Manages entity allocation, deallocation, component masks, and lifetime tracking.
 */
class EntityManager {
public:
    /** @brief Maximum allowed active entities. */
    static constexpr std::size_t MAX_ENTITIES = 10000;

    /**
     * @brief Construct a new Entity Manager object and initializes the free identifier pool.
     */
    EntityManager() {
        for (Entity::IdType i = 0; i < MAX_ENTITIES; ++i)
            freeIds.push(i);
    }

    /**
     * @brief Creates a new Entity, populating it from the free ID pool.
     * @return The spawned Entity, or an invalid Entity if limit is reached.
     */
    Entity create() {
        if (freeIds.empty()) return Entity(Entity::INVALID_ENTITY);
        Entity::IdType id = freeIds.top();
        freeIds.pop();
        alive.push_back(id);
        masks[id].reset();
        return Entity(id);
    }

    /**
     * @brief Destroys an entity and recycles its identifier.
     * @param e Entity to destroy.
     */
    void destroy(Entity e) {
        if (e.getId() == Entity::INVALID_ENTITY) return;
        masks[e.getId()].reset();
        freeIds.push(e.getId());
        alive.erase(std::remove(alive.begin(), alive.end(), e.getId()), alive.end());
    }

    /**
     * @brief Retrieves the component mask of an entity.
     * @param e Entity to check.
     * @return Reference to component mask.
     */
    ComponentMask& getMask(Entity e) {
        return masks[e.getId()];
    }

    /**
     * @brief Retrieves list of currently active entity identifiers.
     * @return List of active entity IDs.
     */
    const std::vector<Entity::IdType>& getAlive() const {
        return alive;
    }

private:
    /** @brief Tracking vector of active entity IDs. */
    std::vector<Entity::IdType> alive;
    /** @brief Stack of available identifiers. */
    std::stack<Entity::IdType> freeIds;
    /** @brief Array of component masks indexable by entity ID. */
    std::array<ComponentMask, MAX_ENTITIES> masks;
};
