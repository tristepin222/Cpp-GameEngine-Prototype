#pragma once
#include <glm/glm.hpp>

struct InputComponent {
    glm::vec3 movement{ 0 }; // forward/back, left/right, up/down
    glm::vec2 look{ 0 };     // mouse delta
};
