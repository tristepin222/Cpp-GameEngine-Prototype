#pragma once
#include "Entity.hpp"
#include <vector>
#include <unordered_map>
#include <stdexcept>

// Base interface
/**
 * @struct IStorage
 * @brief Polymorphic base interface for all component storage types, allowing untyped manipulation of components.
 */
struct IStorage {
    /**
     * @brief Virtual destructor.
     */
    virtual ~IStorage() = default;
    /**
     * @brief Returns the number of component instances stored.
     * @return Size of the storage.
     */
    virtual size_t size() const = 0;
    /**
     * @brief Removes the component associated with the given entity.
     * @param e The entity to remove.
     */
    virtual void removeEntity(Entity e) = 0;

    /**
     * @brief Returns list of entities mapped to components.
     * @return Reference to vector of entities.
     */
    virtual const std::vector<Entity>& getEntities() const = 0;
};

/**
 * @class ComponentStorage
 * @brief Template class storing components contiguously in memory for optimal cache usage.
 * @tparam T Type of component being stored.
 */
template<typename T>
class ComponentStorage : public IStorage {
public:
    /**
     * @brief Adds a component to an entity.
     * @param e The target entity.
     * @param comp The component instance.
     */
    void add(Entity e, T comp) {
        entities.push_back(e);
        data.push_back(std::move(comp));
        entityToIndex[e] = data.size() - 1;
    }

    /**
     * @brief Checks if an entity possesses this component.
     * @param e The entity to check.
     * @return True if the component exists, false otherwise.
     */
    bool has(Entity e) const {
        return entityToIndex.find(e) != entityToIndex.end();
    }

    /**
     * @brief Removes the component from an entity.
     * @param e The entity to remove.
     */
    void remove(Entity e) {
        auto it = entityToIndex.find(e);
        if (it == entityToIndex.end()) return;

        size_t index = it->second;
        size_t last = data.size() - 1;

        // swap-remove
        data[index] = data[last];
        entities[index] = entities[last];
        entityToIndex[entities[index]] = index;

        data.pop_back();
        entities.pop_back();
        entityToIndex.erase(e);
    }

    /**
     * @brief Polymorphic interface to remove entity component.
     * @param e The entity to remove.
     */
    void removeEntity(Entity e) override {
        remove(e);
    }

    /**
     * @brief Retrieves the component associated with an entity.
     * @param e The target entity.
     * @return Reference to the component instance.
     */
    T& get(Entity e) {
        auto it = entityToIndex.find(e);
        if (it == entityToIndex.end()) throw std::runtime_error("Component not found");
        return data[it->second];
    }

    /**
     * @brief Retrieves all entities with this component.
     * @return Vector of entities.
     */
    const std::vector<Entity>& getEntities() const override {
        return entities;
    }

    /**
     * @brief Returns the size of the storage.
     * @return Element count.
     */
    size_t size() const override { return data.size(); }

    /**
     * @brief Gets start iterator of stored component data.
     * @return Iterator.
     */
    auto begin() { return data.begin(); }
    /**
     * @brief Gets end iterator of stored component data.
     * @return Iterator.
     */
    auto end() { return data.end(); }
    /**
     * @brief Gets start iterator of stored entity list.
     * @return Iterator.
     */
    auto entityBegin() { return entities.begin(); }
    /**
     * @brief Gets end iterator of stored entity list.
     * @return Iterator.
     */
    auto entityEnd() { return entities.end(); }

private:
    /** @brief Contiguous array of component instances. */
    std::vector<T> data;                // contiguous component data
    /** @brief Entity mapping matching data array order. */
    std::vector<Entity> entities;       // entities aligned with data
    /** @brief Mapping for quick entity component lookup. */
    std::unordered_map<Entity, size_t> entityToIndex; // fast lookup
};
