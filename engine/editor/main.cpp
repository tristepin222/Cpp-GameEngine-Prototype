#include "core/Application.hpp"
#include <iostream>
#include <string>
#include <filesystem>

/**
 * @brief Entry point for the engine editor.
 *
 * Usage:
 *   editor.exe                  -> opens project from current working directory
 *   editor.exe <project_path>   -> opens project from the specified path
 */
int main(int argc, char* argv[]) {
    try {
        Engine::ApplicationConfig config;
        config.title        = "Engine Editor";
        config.enableEditor = true;

        // Capture the exe directory BEFORE any CWD change so PluginManager can
        // find engine plugins in sdk/bin/plugins/ regardless of project location.
        config.exeDir = std::filesystem::weakly_canonical(
            std::filesystem::path(argv[0]).parent_path()
        ).string();

        // Resolve project path to absolute BEFORE constructing Application
        // (Application will change the CWD to the project directory)
        std::filesystem::path projectPath;
        if (argc >= 2) {
            projectPath = std::filesystem::absolute(argv[1]);
        } else {
            projectPath = std::filesystem::current_path(); // CWD = project dir
        }
        config.projectPath = projectPath.string();

        std::cout << "[Editor] Opening project at: " << config.projectPath << std::endl;

        Engine::Application app(config);
        app.run();

    } catch (const std::exception& e) {
        std::cerr << "[Editor] Fatal exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
