#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace JSONUtils {
    std::string indent(int level);
    std::string quote(const std::string& value);
    std::string vec3ToJson(const glm::vec3& v);
    std::string vec4ToJson(const glm::vec4& v);
    
    std::string extractStringValue(const std::string& source, const std::string& key);
    bool extractFloatArray(const std::string& source, const std::string& key, float* values, size_t count);
    bool extractFloatValue(const std::string& source, const std::string& key, float& value);
    std::vector<std::string> extractEntityObjects(const std::string& source);
}
