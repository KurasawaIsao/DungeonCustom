#include "ItemTableDataBase.h"
#include"ItemTableIO.h"
#include "MessageLog.h"
#include <filesystem>
namespace fs = std::filesystem;

std::unordered_map<std::string, ItemSpawnTable> ItemTableDatabase::m_Tables;
void ItemTableDatabase::Init()
{
    LoadAll("DungeonData\\ItemTables");
}

const ItemSpawnTable* ItemTableDatabase::Get(const std::string& id)
{
    auto it = m_Tables.find(id);
    if (it == m_Tables.end())
        return nullptr;
    return &it->second;
}

bool ItemTableDatabase::Exists(const std::string& id)
{
    return m_Tables.find(id) != m_Tables.end();
}

std::vector<std::string> ItemTableDatabase::GetAllIds()
{
    std::vector<std::string> ids;
    ids.reserve(m_Tables.size());

    for (const auto& [id, table] : m_Tables)
    {
        ids.push_back(id);
    }
    return ids;
}
void ItemTableDatabase::LoadAll(const std::string& dir)
{
    m_Tables.clear();

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
            if (m_Tables.find(table.tableId) != m_Tables.end())
            {
                MessageLog::Instance().AddMessage("[Data] Duplicate item tableId: " + table.tableId);
                continue;
            }
            m_Tables.emplace(table.tableId, std::move(table));
        }
    }
}
