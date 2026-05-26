#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include "ItemData.h"
#include "EnemyData.h"
#include "TrapData.h"

enum class TableCategory { Item, Enemy, Trap };

class SpawnTableEditor {
public:
    void DrawTableEditorTab();

private:
    TableCategory m_CurrentCategory = TableCategory::Item;
    char m_RenameBuf[64] = "";
    std::string m_LastSelectedId = "";

    std::unordered_map<TableCategory, std::string> m_SelectedIds;

    std::unordered_map<std::string, ItemSpawnTable> m_ItemTables;
    std::unordered_map<std::string, EnemySpawnTable> m_EnemyTables;
    std::unordered_map<std::string, TrapSpawnTable> m_TrapTables;
    bool m_TablesLoaded = false;

    template<typename T>
    void DrawTableEditorContents(std::unordered_map<std::string, T>& tables, TableCategory category);

    void EnsureTablesLoaded();
    void ReloadTableCache();
    void DrawIDCombo(TableCategory category, std::string& ioId);
};
