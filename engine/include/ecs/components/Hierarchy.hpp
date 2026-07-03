#pragma once
#include "../Entity.hpp"

/**
 * @struct HierarchyComponent
 * @brief Represents a parent-child relationship between ECS entities.
 */
struct HierarchyComponent {
    Entity parent;
};
