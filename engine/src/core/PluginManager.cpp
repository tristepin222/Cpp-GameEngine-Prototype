#include "core/PluginManager.hpp"
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

PluginManager::PluginManager(Registry& reg, SystemManager& sys, VulkanRenderer& rend, EditorModeState& mode)
    : registry(reg), systemManager(sys), renderer(rend), editorMode(mode) {}

PluginManager::~PluginManager() {
    unloadPlugins();
}

void PluginManager::loadPlugins() {
    std::string pluginsDir = "./plugins";
    if (!std::filesystem::exists(pluginsDir)) {
        std::filesystem::create_directories(pluginsDir);
        std::cout << "[PluginManager] Created plugins folder." << std::endl;
        return; // No plugins to load yet
    }

    std::cout << "[PluginManager] Scanning plugins directory: " << pluginsDir << std::endl;

    for (const auto& entry : std::filesystem::directory_iterator(pluginsDir)) {
        if (entry.path().extension() == ".dll") {
            std::string pathStr = entry.path().string();
            std::cout << "[PluginManager] Found dynamic library: " << pathStr << std::endl;

#ifdef _WIN32
            HMODULE handle = LoadLibraryA(pathStr.c_str());
            if (!handle) {
                DWORD err = GetLastError();
                std::cerr << "[PluginManager] Failed to load DLL: " << pathStr << " (Error code: " << err << ")" << std::endl;
                continue;
            }

            auto initFunc = (PluginInitFunc)GetProcAddress(handle, "initPlugin");
            auto shutdownFunc = (PluginShutdownFunc)GetProcAddress(handle, "shutdownPlugin");

            if (!initFunc) {
                std::cerr << "[PluginManager] Entry point 'initPlugin' not found in: " << pathStr << std::endl;
                FreeLibrary(handle);
                continue;
            }
#else
            HMODULE handle = dlopen(pathStr.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!handle) {
                std::cerr << "[PluginManager] Failed to load shared object: " << pathStr << " (" << dlerror() << ")" << std::endl;
                continue;
            }

            auto initFunc = (PluginInitFunc)dlsym(handle, "initPlugin");
            auto shutdownFunc = (PluginShutdownFunc)dlsym(handle, "shutdownPlugin");

            if (!initFunc) {
                std::cerr << "[PluginManager] Entry point 'initPlugin' not found in: " << pathStr << std::endl;
                dlclose(handle);
                continue;
            }
#endif

            // Initialize plugin
            PluginContext context;
            context.registry = &registry;
            context.systemManager = &systemManager;
            context.renderer = &renderer;
            context.editorMode = &editorMode;
            context.imguiContext = ImGui::GetCurrentContext();

            std::cout << "[PluginManager] Initializing plugin: " << pathStr << std::endl;
            initFunc(&context);

            LoadedPlugin plugin;
            plugin.path = pathStr;
            plugin.handle = handle;
            plugin.shutdownFunc = shutdownFunc;
            loadedPlugins.push_back(plugin);

            std::cout << "[PluginManager] Plugin successfully loaded: " << pathStr << std::endl;
        }
    }
}

void PluginManager::unloadPlugins() {
    if (loadedPlugins.empty()) return;

    std::cout << "[PluginManager] Unloading all active plugins..." << std::endl;

    for (auto& plugin : loadedPlugins) {
        if (plugin.shutdownFunc) {
            PluginContext context;
            context.registry = &registry;
            context.systemManager = &systemManager;
            context.renderer = &renderer;
            context.editorMode = &editorMode;
            context.imguiContext = ImGui::GetCurrentContext();
            plugin.shutdownFunc(&context);
        }

#ifdef _WIN32
        FreeLibrary(plugin.handle);
#else
        dlclose(plugin.handle);
#endif
    }
    loadedPlugins.clear();
    std::cout << "[PluginManager] All plugins successfully unloaded." << std::endl;
}
