#pragma once
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

class VulkanRenderer;

namespace EntityCloner {
    Entity clone(Registry& registry, VulkanRenderer& renderer, Entity source);
}
