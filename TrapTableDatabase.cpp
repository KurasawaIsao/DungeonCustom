#include "TrapTableDatabase.h"
#include "MessageLog.h"
#include "SpawnTableIO.h"
#include <filesystem>

namespace fs = std::filesystem;

std::unordered_map<std::string, TrapSpawnTable> TrapTableDatabase::tables;

void TrapTableDatabase::Init()
{
    LoadAll("DungeonData\\TrapTables\\");
}

const TrapSpawnTable* TrapTableDatabase::Get(const std::string& id)
{
    auto it = tables.find(id);
    if (it == tables.end())
        return nullptr;
    return &it->second;
}

bool TrapTableDatabase::Exists(const std::string& id)
{
    return tables.find(id) != tables.end();
}

std::vector<std::string> TrapTableDatabase::GetAllIds()
{
    std::vector<std::string> ids;
    ids.reserve(tables.size());

    for (const auto& [id, table] : tables)
        ids.push_back(id);

    return ids;
}

void TrapTableDatabase::LoadAll(const std::string& dir)
{
    tables.clear();

    std::error_code ec;
    if (!fs::exists(dir, ec) || ec)
    {
        MessageLog::Instance().AddMessage("[Data] Trap table dir not found: " + dir);
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir, ec))
    {
        if (ec)
        {
            MessageLog::Instance().AddMessage("[Data] Trap table dir read failed: " + dir);
            return;
        }
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".json")
            continue;

        TrapSpawnTable table;
        if (SpawnTableIO::Load(entry.path().string(), table))
        {
            if (tables.find(table.tableId) != tables.end())
            {
                MessageLog::Instance().AddMessage("[Data] Duplicate trap tableId: " + table.tableId);
                continue;
            }
            tables.emplace(table.tableId, std::move(table));
        }
    }
}
