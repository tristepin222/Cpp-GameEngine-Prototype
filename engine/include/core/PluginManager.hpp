#pragma once
#include "Plugin.hpp"
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
using HMODULE = void*;
#endif

struct LoadedPlugin {
    std::string path;
    HMODULE handle = nullptr;
    PluginShutdownFunc shutdownFunc = nullptr;
    bool isScript = false;
};

class PluginManager {
public:
    PluginManager(Registry& reg, SystemManager& sys, VulkanRenderer& rend, EditorModeState& mode);
    ~PluginManager();

    void loadPlugins();
    void loadScripts(const std::string& projectPath);
    void setExeDirectory(const std::string& dir);
    void unloadPlugins();
    void unloadScripts();

private:
    void scanDirectory(const std::string& dir, bool isScript = false);
    Registry& registry;
    SystemManager& systemManager;
    VulkanRenderer& renderer;
    EditorModeState& editorMode;
    std::string exeDir;

    std::vector<LoadedPlugin> loadedPlugins;
};
