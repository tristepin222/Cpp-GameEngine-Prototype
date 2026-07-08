#pragma once
#include "ecs/Registry.hpp"
#include "ecs/SystemManager.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "editor/EditorModeState.hpp"
#include "core/EngineAPI.hpp"
#include <imgui.h>

#ifdef _WIN32
    #define PLUGIN_API extern "C" __declspec(dllexport)
#else
    #define PLUGIN_API extern "C"
#endif

/**
 * @struct PluginContext
 * @brief State container passed to dynamic libraries upon initialization.
 */
struct PluginContext {
    Registry* registry;
    SystemManager* systemManager;
    VulkanRenderer* renderer;
    EditorModeState* editorMode;
    ImGuiContext* imguiContext;
};

typedef void (*PluginInitFunc)(PluginContext*);
typedef void (*PluginShutdownFunc)(PluginContext*);
