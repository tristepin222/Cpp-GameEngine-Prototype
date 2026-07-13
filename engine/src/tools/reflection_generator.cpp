#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <algorithm>

namespace fs = std::filesystem;

struct ReflectedField {
    std::string type;
    std::string name;
};

struct ReflectedComponent {
    std::string name;
    std::string headerName;
    std::vector<ReflectedField> fields;
};

struct ReflectedSystem {
    std::string name;
    std::string headerName;
};

// Trim whitespace from string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string getFieldTypeEnum(const std::string& type) {
    if (type == "float" || type == "double") return "Engine::FieldType::Float";
    if (type == "bool") return "Engine::FieldType::Bool";
    if (type == "glm::vec3" || type == "vec3") return "Engine::FieldType::Vec3";
    if (type == "RigidBodyType") return "Engine::FieldType::RigidBodyType";
    if (type == "Entity") return "Engine::FieldType::Entity";
    return "";
}

// Calculate the correct relative include path for both user scripts and engine public headers
std::string getIncludePath(const fs::path& filePath, const fs::path& inputDir) {
    std::string pathStr = filePath.generic_string();
    size_t ecsPos = pathStr.find("ecs/components");
    if (ecsPos != std::string::npos) {
        return pathStr.substr(ecsPos);
    }
    try {
        fs::path rel = fs::relative(filePath, inputDir);
        return rel.generic_string();
    } catch (...) {
        return filePath.filename().string();
    }
}

void parseHeader(const fs::path& filePath, const fs::path& inputDir, std::vector<ReflectedComponent>& outComponents, std::vector<ReflectedSystem>& outSystems) {
    std::ifstream file(filePath);
    if (!file.is_open()) return;

    std::string line;
    bool inComponent = false;
    ReflectedComponent currentComp;
    bool nextLineIsReflected = false;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        // Check if this line is a reflection marker (only skip if we are not already parsing a component's fields)
        if (trimmed.find("@reflect") != std::string::npos && !inComponent) {
            nextLineIsReflected = true;
            continue;
        }

        if (nextLineIsReflected) {
            nextLineIsReflected = false;

            // Check if it's a system class (inherits from System)
            if (trimmed.find("class") != std::string::npos && trimmed.find(": public System") != std::string::npos) {
                std::regex sysRegex("class\\s+(\\w+)");
                std::smatch match;
                if (std::regex_search(trimmed, match, sysRegex)) {
                    ReflectedSystem sys;
                    sys.name = match[1].str();
                    sys.headerName = getIncludePath(filePath, inputDir);
                    outSystems.push_back(sys);
                }
                continue;
            }

            // Check if it's a component struct/class
            if (trimmed.find("struct") != std::string::npos || trimmed.find("class") != std::string::npos) {
                std::regex compRegex("(?:struct|class)\\s+(\\w+)");
                std::smatch match;
                if (std::regex_search(trimmed, match, compRegex)) {
                    inComponent = true;
                    currentComp = ReflectedComponent();
                    currentComp.name = match[1].str();
                    currentComp.headerName = getIncludePath(filePath, inputDir);
                }
                continue;
            }
        }

        if (inComponent) {
            // Check for end of struct/class
            if (trimmed.rfind("};", 0) == 0 || trimmed == "};") {
                outComponents.push_back(currentComp);
                inComponent = false;
                continue;
            }

            // Check if the field is annotated
            bool isReflectedField = (trimmed.find("@reflect") != std::string::npos);

            if (isReflectedField) {
                // Strip comments
                size_t commentPos = trimmed.find("//");
                if (commentPos != std::string::npos) {
                    trimmed = trimmed.substr(0, commentPos);
                }
                trimmed = trim(trimmed);

                // Strip trailing semicolon
                if (!trimmed.empty() && trimmed.back() == ';') {
                    trimmed.pop_back();
                }
                trimmed = trim(trimmed);

                // Split at '=' or '{'
                size_t eqPos = trimmed.find('=');
                if (eqPos == std::string::npos) {
                    eqPos = trimmed.find('{');
                }
                if (eqPos != std::string::npos) {
                    trimmed = trimmed.substr(0, eqPos);
                }
                trimmed = trim(trimmed);

                // Split type and name
                size_t lastSpace = trimmed.find_last_of(" \t");
                if (lastSpace != std::string::npos) {
                    ReflectedField field;
                    field.type = trim(trimmed.substr(0, lastSpace));
                    field.name = trim(trimmed.substr(lastSpace + 1));
                    currentComp.fields.push_back(field);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_directory> <output_file>" << std::endl;
        return 1;
    }

    std::string inputDir = argv[1];
    std::string outputFile = argv[2];

    std::vector<ReflectedComponent> components;
    std::vector<ReflectedSystem> systems;

    // Scan input directory recursively for headers
    try {
        if (fs::exists(inputDir)) {
            for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".hpp" || ext == ".h") {
                        parseHeader(entry.path(), inputDir, components, systems);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
        return 1;
    }

    // Also scan the engine public include directory for PhysgunScript.hpp if we are running from sandbox_game
    try {
        fs::path engineInclude = fs::path(inputDir) / "../../engine/include/ecs/components";
        if (fs::exists(engineInclude)) {
            for (const auto& entry : fs::directory_iterator(engineInclude)) {
                if (entry.is_regular_file() && entry.path().filename() == "PhysgunScript.hpp") {
                    parseHeader(entry.path(), inputDir, components, systems);
                }
            }
        }
    } catch (...) {}

    // Open output file
    std::ofstream out(outputFile);
    if (!out.is_open()) {
        std::cerr << "Failed to open output file: " << outputFile << std::endl;
        return 1;
    }

    // Generate output header
    out << "// =========================================================================\n";
    out << "//  GENERATED REFLECTION FILE — DO NOT EDIT MANUALLY\n";
    out << "//  This file is automatically generated by reflection_generator\n";
    out << "// =========================================================================\n\n";

    out << "#include \"ScriptAPI.hpp\"\n";
    
    // Generate includes for each parsed script header
    std::vector<std::string> includedHeaders;
    for (const auto& comp : components) {
        if (std::find(includedHeaders.begin(), includedHeaders.end(), comp.headerName) == includedHeaders.end()) {
            out << "#include \"" << comp.headerName << "\"\n";
            includedHeaders.push_back(comp.headerName);
        }
    }
    for (const auto& sys : systems) {
        if (std::find(includedHeaders.begin(), includedHeaders.end(), sys.headerName) == includedHeaders.end()) {
            out << "#include \"" << sys.headerName << "\"\n";
            includedHeaders.push_back(sys.headerName);
        }
    }
    out << "\n";

    // Generate DLL Entry Points
    out << "#ifdef _WIN32\n";
    out << "    #define PLUGIN_API extern \"C\" __declspec(dllexport)\n";
    out << "#else\n";
    out << "    #define PLUGIN_API extern \"C\"\n";
    out << "#endif\n\n";

    out << "PLUGIN_API void initPlugin(PluginContext* context) {\n";
    out << "    ImGui::SetCurrentContext(context->imguiContext);\n\n";

    // Register reflected components
    for (const auto& comp : components) {
        out << "    // Register " << comp.name << "\n";
        out << "    {\n";
        out << "        Engine::ComponentReflection refl;\n";
        out << "        refl.name = \"" << comp.name << "\";\n";
        out << "        refl.fields = {\n";
        for (size_t i = 0; i < comp.fields.size(); ++i) {
            const auto& field = comp.fields[i];
            std::string enumStr = getFieldTypeEnum(field.type);
            if (!enumStr.empty()) {
                out << "            { \"" << field.name << "\", " << enumStr << ", offsetof(" << comp.name << ", " << field.name << ") }";
                if (i < comp.fields.size() - 1) out << ",";
                out << "\n";
            }
        }
        out << "        };\n";
        out << "        refl.add = [](Registry& reg, Entity e) { reg.emplace<" << comp.name << ">(e, " << comp.name << "{}); };\n";
        out << "        refl.has = [](Registry& reg, Entity e) { return reg.has<" << comp.name << ">(e); };\n";
        out << "        refl.remove = [](Registry& reg, Entity e) { reg.remove<" << comp.name << ">(e); };\n";
        out << "        refl.get = [](Registry& reg, Entity e) { return static_cast<void*>(reg.get<" << comp.name << ">(e)); };\n";
        out << "        Engine::ComponentReflectionRegistry::getInstance().registerComponent(refl);\n";
        out << "    }\n\n";
    }

    // Register reflected systems
    for (const auto& sys : systems) {
        out << "    // Register " << sys.name << "\n";
        out << "    {\n";
        out << "        context->systemManager->addSystem(std::make_shared<" << sys.name << ">(*context->registry, *context->renderer, *context->editorMode));\n";
        out << "    }\n\n";
    }

    out << "}\n\n";

    out << "PLUGIN_API void shutdownPlugin(PluginContext* context) {\n";
    out << "    // Cleanup logic\n";
    out << "}\n";

    std::cout << "[Reflection Generator] Successfully generated: " << outputFile << " (" 
              << components.size() << " components, " << systems.size() << " systems)" << std::endl;

    return 0;
}
