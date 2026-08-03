#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#endif

#include <filesystem>
#include <string>
#include <cstdlib>

void openInExplorer(const std::filesystem::path& path) {
#ifdef _WIN32
    std::filesystem::path absPath = std::filesystem::absolute(path);
    std::string pathStr = absPath.string();
    std::replace(pathStr.begin(), pathStr.end(), '/', '\\');
    
    if (std::filesystem::is_directory(absPath)) {
        ShellExecuteA(NULL, "open", pathStr.c_str(), NULL, NULL, SW_SHOWNORMAL);
    } else {
        std::string params = "/select,\"" + pathStr + "\"";
        ShellExecuteA(NULL, "open", "explorer.exe", params.c_str(), NULL, SW_SHOWNORMAL);
    }
#elif __APPLE__
    std::string cmd = "open \"" + path.string() + "\"";
    std::system(cmd.c_str());
#else
    std::string cmd = "xdg-open \"" + path.string() + "\"";
    std::system(cmd.c_str());
#endif
}
