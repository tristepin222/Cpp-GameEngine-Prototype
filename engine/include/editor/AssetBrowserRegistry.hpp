#pragma once
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include "core/EngineAPI.hpp"

namespace Engine {

    /**
     * @class AssetBrowserRegistry
     * @brief Registry for adding custom options and submenu items to the Asset Browser's right-click context menu.
     */
    class ENGINE_API AssetBrowserRegistry {
    public:
        using ContextMenuCallback = std::function<void(const std::filesystem::path&)>;

        struct Option {
            std::string labelPath; // e.g. "Create/Custom Component"
            ContextMenuCallback callback;
        };

        /**
         * @brief Registers a custom context menu item.
         * @param labelPath Menu hierarchy path (e.g. "Create/Script" or "Actions/Validate").
         * @param callback Function called when the menu item is selected, receiving the folder path.
         */
        static void registerOption(const std::string& labelPath, ContextMenuCallback callback) {
            getOptions().push_back({ labelPath, callback });
        }

        /**
         * @brief Retrieves all registered custom menu options.
         */
        static std::vector<Option>& getOptions() {
            static std::vector<Option> s_options;
            return s_options;
        }
    };

}
