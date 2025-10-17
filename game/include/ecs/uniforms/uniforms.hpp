// Renderer.h (or a separate Uniforms.h)
#pragma once
#include <glm/glm.hpp>

struct CameraUBO {
    glm::mat4 view;
    glm::mat4 proj;
};
