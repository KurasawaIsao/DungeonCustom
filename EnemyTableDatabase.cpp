#include "EnemyTableDatabase.h"
#include "EnemyTableIO.h"
#include "MessageLog.h"
#include <filesystem>

namespace fs = std::filesystem;

std::unordered_map<
    std::string,
    EnemySpawnTable> EnemyTableDatabase::tables;

void EnemyTableDatabase::Init()
{
    LoadAll("DungeonData\\EnemyTables\\");
}

const EnemySpawnTable*
EnemyTableDatabase::Get(const std::string& id)
{
    auto it = tables.find(id);
    if (it == tables.end())
        return nullptr;

    return &it->second;
}

bool EnemyTableDatabase::Exists(const std::string& id)
{
    return tables.find(id) != tables.end();
}

std::vector<std::string>
EnemyTableDatabase::GetAllIds()
{
    std::vector<std::string> ids;

    for (const auto& [id, _] : tables)
        ids.push_back(id);

    return ids;
}

void EnemyTableDatabase::LoadAll(
    const std::string& dir)
{
    tables.clear();

    if (!fs::exists(dir))
    {
        MessageLog::Instance().AddMessage("[Data] Enemy table dir not found: " + dir);
        return;
    }

    for (const auto& entry :
        fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".json")
            continue;

        EnemySpawnTable table;

        if (EnemyTableIO::LoadFromFile(
            entry.path().string(), table))
        {
            if (tables.find(table.tableId) != tables.end())
            {
                MessageLog::Instance().AddMessage("[Data] Duplicate enemy tableId: " + table.tableId);
                continue;
            }
            tables.emplace(
                table.tableId,
                std::move(table));
        }
    }
}
