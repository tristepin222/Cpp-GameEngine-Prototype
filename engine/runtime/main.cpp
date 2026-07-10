#include "core/Application.hpp"
#include <iostream>
#include <string>
#include <filesystem>

/**
 * @brief Entry point for the standalone game runtime (no editor UI).
 *
 * Usage:
 *   game_runtime.exe                  -> runs project from current working directory
 *   game_runtime.exe <project_path>   -> runs project from the specified path
 *
 * This is the executable that gets packaged as game.exe when you click Build in the editor.
 */
int main(int argc, char* argv[]) {
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
        if (argc >= 2) {
            projectPath = std::filesystem::absolute(argv[1]);
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
