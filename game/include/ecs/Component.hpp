#pragma once
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <memory>

// Simple runtime component type id registry
inline std::unordered_map<std::type_index, std::size_t>& g_componentTypeMap() {
    static std::unordered_map<std::type_index, std::size_t> map;
    return map;
}

inline std::size_t registerComponentType(std::type_index idx) {
    auto &map = g_componentTypeMap();
    if (map.find(idx) == map.end()) {
        std::size_t id = map.size();
        map[idx] = id;
        return id;
    }
    return map[idx];
}
