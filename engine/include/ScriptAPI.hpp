#pragma once

// Core ECS
#include "ecs/Registry.hpp"
#include "ecs/Entity.hpp"
#include "ecs/System.hpp"

// Components
#include "ecs/components/Transform.hpp"
#include "ecs/components/Rigidbody.hpp"
#include "ecs/components/Collider.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/EditorCamera.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "ecs/components/PlayerControllerComponent.hpp"
#include "ecs/components/CinemachineComponent.hpp"
#include "ecs/components/AudioSource.hpp"

// Math & Utility
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>

// Input & UI
#include <imgui.h>
#include <GLFW/glfw3.h>

// Renderer & Reflection
#include "renderer/VulkanRenderer.hpp"
#include "meta/ComponentReflection.hpp"
#include "core/Plugin.hpp"

// Expose the Engine namespace types directly to script files
using namespace Engine;
