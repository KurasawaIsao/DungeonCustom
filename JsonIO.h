#pragma once
#include <string>
#include "external/json.hpp"

namespace JsonIO
{
    bool LoadJson(const std::string& path, nlohmann::json& outJson);
    bool SaveJson(const std::string& path, const nlohmann::json& jsonData);
}
