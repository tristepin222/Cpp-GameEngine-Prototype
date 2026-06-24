#pragma once
#include "Entity.hpp"
#include <vector>
#include <unordered_map>
#include <stdexcept>

// Base interface
struct IStorage {
    virtual ~IStorage() = default;
    virtual size_t size() const = 0;
    virtual void removeEntity(Entity e) = 0;

    // Return all entities in this storage
    virtual const std::vector<Entity>& getEntities() const = 0;
};
template<typename T>
class ComponentStorage : public IStorage {
public:
    void add(Entity e, T comp) {
        entities.push_back(e);
        data.push_back(std::move(comp));
        entityToIndex[e] = data.size() - 1;
    }

    bool has(Entity e) const {
        return entityToIndex.find(e) != entityToIndex.end();
    }

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

    void removeEntity(Entity e) override {
        remove(e);
    }

    T& get(Entity e) {
        auto it = entityToIndex.find(e);
        if (it == entityToIndex.end()) throw std::runtime_error("Component not found");
        return data[it->second];
    }

    const std::vector<Entity>& getEntities() const override {
        return entities;
    }

    size_t size() const override { return data.size(); }

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto entityBegin() { return entities.begin(); }
    auto entityEnd() { return entities.end(); }

private:
    std::vector<T> data;                // contiguous component data
    std::vector<Entity> entities;       // entities aligned with data
    std::unordered_map<Entity, size_t> entityToIndex; // fast lookup
};
