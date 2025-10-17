#pragma once

#include <glm/glm.hpp>


struct PushConstants {
    glm::mat4 model;
    glm::vec4 color;
    glm::mat4 viewProj;
    glm::vec3 camPos;
    float scale;
    float fade;
};