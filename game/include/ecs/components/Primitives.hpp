#pragma once
#include "Mesh.hpp"
#include <vector>

namespace Primitives {

    inline std::vector<Vertex> triangleVertices() {
        return {
            {{-0.5f, -0.5f, 0.f}, {0.f,0.f,1.f}, {0.f,0.f}},
            {{ 0.5f, -0.5f, 0.f}, {0.f,0.f,1.f}, {1.f,0.f}},
            {{ 0.0f,  0.5f, 0.f}, {0.f,0.f,1.f}, {0.5f,1.f}}
        };
    }

    inline std::vector<uint32_t> triangleIndices() {
        return { 0, 1, 2 };
    }

    inline std::vector<Vertex> quadVertices() {
        return {
            {{-0.5f, -0.5f, 0.f}, {0.f,0.f,1.f}, {0.f,0.f}},
            {{ 0.5f, -0.5f, 0.f}, {0.f,0.f,1.f}, {1.f,0.f}},
            {{ 0.5f,  0.5f, 0.f}, {0.f,0.f,1.f}, {1.f,1.f}},
            {{-0.5f,  0.5f, 0.f}, {0.f,0.f,1.f}, {0.f,1.f}},
        };
    }

    inline std::vector<uint32_t> quadIndices() {
        return { 0, 1, 2, 2, 3, 0 };
    }

    inline std::vector<Vertex> cubeVertices() {
        return {
            // front
            {{-0.5f,-0.5f, 0.5f},{0,0,1},{0,0}},
            {{ 0.5f,-0.5f, 0.5f},{0,0,1},{1,0}},
            {{ 0.5f, 0.5f, 0.5f},{0,0,1},{1,1}},
            {{-0.5f, 0.5f, 0.5f},{0,0,1},{0,1}},
            // back
            {{-0.5f,-0.5f,-0.5f},{0,0,-1},{1,0}},
            {{ 0.5f,-0.5f,-0.5f},{0,0,-1},{0,0}},
            {{ 0.5f, 0.5f,-0.5f},{0,0,-1},{0,1}},
            {{-0.5f, 0.5f,-0.5f},{0,0,-1},{1,1}},
        };
    }

    inline std::vector<uint32_t> cubeIndices() {
        return {
            0,1,2, 2,3,0,  // front
            1,5,6, 6,2,1,  // right
            5,4,7, 7,6,5,  // back
            4,0,3, 3,7,4,  // left
            3,2,6, 6,7,3,  // top
            4,5,1, 1,0,4   // bottom
        };
    }

    // Optionally add more primitives here:
    inline std::vector<Vertex> pyramidVertices() {
        return {
            {{ 0.0f, 0.5f, 0.0f},{0,1,0},{0.5f,1.0f}}, // top
            {{-0.5f,-0.5f, 0.5f},{0,-1,0},{0,0}},
            {{ 0.5f,-0.5f, 0.5f},{0,-1,0},{1,0}},
            {{ 0.5f,-0.5f,-0.5f},{0,-1,0},{1,1}},
            {{-0.5f,-0.5f,-0.5f},{0,-1,0},{0,1}},
        };
    }

    inline std::vector<uint32_t> pyramidIndices() {
        return {
            0,1,2, 0,2,3, 0,3,4, 0,4,1,  // sides
            1,2,3, 3,4,1                    // base
        };
    }

    // Sphere / cylinder / torus can be added later as procedural functions
}
