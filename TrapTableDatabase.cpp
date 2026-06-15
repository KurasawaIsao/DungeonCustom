#include "TrapTableDatabase.h"
#include "MessageLog.h"
#include "SpawnTableIO.h"
#include <filesystem>

namespace fs = std::filesystem;

std::unordered_map<std::string, TrapSpawnTable> TrapTableDatabase::m_Tables;

void TrapTableDatabase::Init()
{
    LoadAll("DungeonData\\TrapTables\\");
}

const TrapSpawnTable* TrapTableDatabase::Get(const std::string& id)
{
    auto it = m_Tables.find(id);
    if (it == m_Tables.end())
        return nullptr;
    return &it->second;
}

bool TrapTableDatabase::Exists(const std::string& id)
{
    return m_Tables.find(id) != m_Tables.end();
}

std::vector<std::string> TrapTableDatabase::GetAllIds()
{
    std::vector<std::string> ids;
    ids.reserve(m_Tables.size());

    for (const auto& [id, table] : m_Tables)
        ids.push_back(id);

    return ids;
}

void TrapTableDatabase::LoadAll(const std::string& dir)
{
    m_Tables.clear();

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
            if (m_Tables.find(table.tableId) != m_Tables.end())
            {
                MessageLog::Instance().AddMessage("[Data] Duplicate trap tableId: " + table.tableId);
                continue;
            }
            m_Tables.emplace(table.tableId, std::move(table));
        }
    }
}
