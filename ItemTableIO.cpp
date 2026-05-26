#include "ItemTableIO.h"
#include "MessageLog.h"
#include <fstream>
#include "external/json.hpp"
#include "JsonIO.h"

using json = nlohmann::json;

bool ItemTableIO::LoadFromFile(
    const std::string& path,
    ItemSpawnTable& outTable)
{
    json root;
    if (!JsonIO::LoadJson(path, root))
    {
        MessageLog::Instance().AddMessage("[Data] Item table load failed: " + path);
        return false;
    }

    try
    {
        if (!root.is_object())
        {
            MessageLog::Instance().AddMessage("[Data] Item table root is not object: " + path);
            return false;
        }

        if (!root.contains("tableId") || !root["tableId"].is_string() || root["tableId"].get<std::string>().empty())
        {
            MessageLog::Instance().AddMessage("[Data] Item table has invalid tableId: " + path);
            return false;
        }

        if (!root.contains("entries") || !root["entries"].is_array())
        {
            MessageLog::Instance().AddMessage("[Data] Item table has invalid entries: " + path);
            return false;
        }

        outTable.tableId = root["tableId"].get<std::string>();
        outTable.entries.clear();

        for (const auto& e : root["entries"])
        {
            if (!e.is_object() || !e.contains("id") || !e["id"].is_string() ||
                !e.contains("weight") || !e["weight"].is_number_integer())
            {
                MessageLog::Instance().AddMessage("[Data] Invalid item table entry skipped: " + outTable.tableId);
                continue;
            }

            std::string id = e["id"].get<std::string>();
            int weight = e["weight"].get<int>();
            if (id.empty() || weight < 0)
            {
                MessageLog::Instance().AddMessage("[Data] Invalid item id/weight skipped: " + outTable.tableId);
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
        MessageLog::Instance().AddMessage("[Data] Item table parse error: " + path);
        return false;
    }

    return true;
}

bool ItemTableIO::SaveToFile(
    const std::string& path,
    const ItemSpawnTable& table)
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
