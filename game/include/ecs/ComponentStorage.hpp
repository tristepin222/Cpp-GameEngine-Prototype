#pragma once
#include "Entity.hpp"
#include <unordered_map>
#include <vector>
#include <optional>

// Base class for all component storages
struct IStorage {
    virtual ~IStorage() = default;

    virtual size_t size() const = 0;
};


template<typename T>
class ComponentStorage : public IStorage {
public:
    void add(Entity e, T comp) {
        data[e] = std::move(comp);
    }
    bool has(Entity e) const {
        return data.find(e) != data.end();
    }
    void remove(Entity e) {
        data.erase(e);
    }
    std::optional<T*> get(Entity e) {
        auto it = data.find(e);
        if (it == data.end()) return std::nullopt;
        return &it->second;
    }

    size_t size() const override { return data.size(); }

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
private:
    std::unordered_map<Entity, T> data;
};
