#pragma once
#include <glm/glm.hpp>

struct InstanceData {
    glm::mat4 modelMatrix;
    uint32_t materialID;
    uint32_t meshID;
};