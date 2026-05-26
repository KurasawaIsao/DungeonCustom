#include "JsonIO.h"
#include <fstream>

namespace JsonIO
{
    bool LoadJson(const std::string& path, nlohmann::json& outJson)
    {
        std::ifstream ifs(path);
        if (!ifs.is_open())
            return false;

        try
        {
            ifs >> outJson;
        }
        catch (...)
        {
            return false;
        }

        return true;
    }

    bool SaveJson(const std::string& path, const nlohmann::json& jsonData)
    {
        std::ofstream ofs(path);
        if (!ofs.is_open())
            return false;

        try
        {
            ofs << jsonData.dump(4);
        }
        catch (...)
        {
            return false;
        }

        return true;
    }
}
