#include "ItemTableDataBase.h"
#include"ItemTableIO.h"
#include "MessageLog.h"
#include <filesystem>
namespace fs = std::filesystem;

std::unordered_map<std::string, ItemSpawnTable> ItemTableDatabase::tables;
void ItemTableDatabase::Init()
{
    LoadAll("DungeonData\\ItemTables");
}

const ItemSpawnTable* ItemTableDatabase::Get(const std::string& id)
{
    auto it = tables.find(id);
    if (it == tables.end())
        return nullptr;
    return &it->second;
}

bool ItemTableDatabase::Exists(const std::string& id)
{
    return tables.find(id) != tables.end();
}

std::vector<std::string> ItemTableDatabase::GetAllIds()
{
    std::vector<std::string> ids;
    ids.reserve(tables.size());

    for (const auto& [id, table] : tables)
    {
        ids.push_back(id);
    }
    return ids;
}
void ItemTableDatabase::LoadAll(const std::string& dir)
{
    tables.clear();

    if (!fs::exists(dir))
    {
        MessageLog::Instance().AddMessage("[Data] Item table dir not found: " + dir);
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".json")
            continue;

        ItemSpawnTable table;
        if (ItemTableIO::LoadFromFile(entry.path().string(), table))
        {
            if (tables.find(table.tableId) != tables.end())
            {
                MessageLog::Instance().AddMessage("[Data] Duplicate item tableId: " + table.tableId);
                continue;
            }
            tables.emplace(table.tableId, std::move(table));
        }
    }
}
