#include "core/Application.hpp"
#include <iostream>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Entry point for the standalone game runtime (no editor UI).
 *
 * Usage:
 *   game_runtime.exe                  -> runs project from current working directory
 *   game_runtime.exe <project_path>   -> runs project from the specified path
 *   game_runtime.exe --debug          -> runs project with attached debug console window
 *
 * This is the executable that gets packaged as game.exe when you click Build in the editor.
 */
int main(int argc, char* argv[]) {
    bool debugConsole = false;
    std::filesystem::path projectPathArg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-debug" || arg == "/debug") {
            debugConsole = true;
        } else if (projectPathArg.empty()) {
            projectPathArg = std::filesystem::absolute(arg);
        }
    }

#ifdef _WIN32
    if (!debugConsole) {
        FreeConsole();
    }
#endif

    try {
        Engine::ApplicationConfig config;
        config.title        = "Game";
        config.enableEditor = false;

        // Capture the exe directory BEFORE any CWD change
        config.exeDir = std::filesystem::weakly_canonical(
            std::filesystem::path(argv[0]).parent_path()
        ).string();

        // Resolve project path to absolute BEFORE constructing Application
        std::filesystem::path projectPath;
        if (!projectPathArg.empty()) {
            projectPath = projectPathArg;
        } else {
            projectPath = std::filesystem::current_path();
        }
        config.projectPath = projectPath.string();

        std::cout << "[GameRuntime] Starting project at: " << config.projectPath << std::endl;

        Engine::Application app(config);
        app.run();


    } catch (const std::exception& e) {
        std::cerr << "[GameRuntime] Fatal exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
