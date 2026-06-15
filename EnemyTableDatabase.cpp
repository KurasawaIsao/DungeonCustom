#include "EnemyTableDatabase.h"
#include "EnemyTableIO.h"
#include "MessageLog.h"
#include <filesystem>

namespace fs = std::filesystem;

std::unordered_map<
    std::string,
    EnemySpawnTable> EnemyTableDatabase::m_Tables;

void EnemyTableDatabase::Init()
{
    LoadAll("DungeonData\\EnemyTables\\");
}

const EnemySpawnTable*
EnemyTableDatabase::Get(const std::string& id)
{
    auto it = m_Tables.find(id);
    if (it == m_Tables.end())
        return nullptr;

    return &it->second;
}

bool EnemyTableDatabase::Exists(const std::string& id)
{
    return m_Tables.find(id) != m_Tables.end();
}

std::vector<std::string>
EnemyTableDatabase::GetAllIds()
{
    std::vector<std::string> ids;

    for (const auto& [id, _] : m_Tables)
        ids.push_back(id);

    return ids;
}

void EnemyTableDatabase::LoadAll(
    const std::string& dir)
{
    m_Tables.clear();

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
            if (m_Tables.find(table.tableId) != m_Tables.end())
            {
                MessageLog::Instance().AddMessage("[Data] Duplicate enemy tableId: " + table.tableId);
                continue;
            }
            m_Tables.emplace(
                table.tableId,
                std::move(table));
        }
    }
}
