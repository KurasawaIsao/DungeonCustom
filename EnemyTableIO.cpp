#include "EnemyTableIO.h"
#include "JsonIO.h"
#include "MessageLog.h"
#include "external/json.hpp"

using json = nlohmann::json;

bool EnemyTableIO::LoadFromFile(
    const std::string& path,
    EnemySpawnTable& outTable)
{
    json root;
    if (!JsonIO::LoadJson(path, root))
    {
        MessageLog::Instance().AddMessage("[Data] Enemy table load failed: " + path);
        return false;
    }

    try
    {
        if (!root.is_object())
        {
            MessageLog::Instance().AddMessage("[Data] Enemy table root is not object: " + path);
            return false;
        }

        if (!root.contains("tableId") || !root["tableId"].is_string() || root["tableId"].get<std::string>().empty())
        {
            MessageLog::Instance().AddMessage("[Data] Enemy table has invalid tableId: " + path);
            return false;
        }

        if (!root.contains("entries") || !root["entries"].is_array())
        {
            MessageLog::Instance().AddMessage("[Data] Enemy table has invalid entries: " + path);
            return false;
        }

        outTable.tableId = root["tableId"].get<std::string>();
        outTable.entries.clear();

        for (const auto& e : root["entries"])
        {
            if (!e.is_object() || !e.contains("id") || !e["id"].is_string() ||
                !e.contains("weight") || !e["weight"].is_number_integer())
            {
                MessageLog::Instance().AddMessage("[Data] Invalid enemy table entry skipped: " + outTable.tableId);
                continue;
            }

            std::string id = e["id"].get<std::string>();
            int weight = e["weight"].get<int>();
            if (id.empty() || weight < 0)
            {
                MessageLog::Instance().AddMessage("[Data] Invalid enemy id/weight skipped: " + outTable.tableId);
                continue;
            }

            outTable.entries.push_back({
                id,
                weight
                });
        }
    }
    catch (const std::exception&)
    {
        MessageLog::Instance().AddMessage("[Data] Enemy table parse error: " + path);
        return false;
    }

    return true;
}

bool EnemyTableIO::SaveToFile(
    const std::string& path,
    const EnemySpawnTable& table)
{
    json root;
    json arr = json::array();

    for (const auto& e : table.entries)
    {
        arr.push_back({
            { "id", e.id },
            { "weight", e.weight }
            });
    }

    root["tableId"] = table.tableId;
    root["entries"] = arr;

    return JsonIO::SaveJson(path, root);
}
