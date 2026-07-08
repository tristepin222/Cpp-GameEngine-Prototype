#include "ecs/Registry.hpp"

std::unordered_map<std::type_index, std::size_t>& g_componentTypeMap() {
    static std::unordered_map<std::type_index, std::size_t> map;
    return map;
}

std::size_t registerComponentType(std::type_index idx) {
    auto &map = g_componentTypeMap();
    auto it = map.find(idx);
    if (it == map.end()) {
        std::size_t id = map.size();
        map[idx] = id;
        return id;
    }
    return it->second;
}
