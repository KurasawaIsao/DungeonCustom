#include "SpawnTableEditor.h"
#include "imgui.h"
#include "ItemTableIO.h"
#include "EnemyTableIO.h"
#include "ItemTableDataBase.h"
#include "EnemyTableDatabase.h"
#include "TrapTableDatabase.h"
#include "ItemDataBase.h"
#include "EnemyDatabase.h"
#include "TrapDataBase.h"
#include "SpawnTableIO.h"
#include <filesystem>

namespace
{
    std::string GetTableDirectory(TableCategory category)
    {
        if (category == TableCategory::Item) return "DungeonData\\ItemTables\\";
        if (category == TableCategory::Enemy) return "DungeonData\\EnemyTables\\";
        return "DungeonData\\TrapTables\\";
    }

    std::string GetDefaultEntryId(TableCategory category)
    {
        if (category == TableCategory::Item) return "Herb_Heal20";
        if (category == TableCategory::Enemy) return "Kingyo";
        return "Trap_Sleep";
    }
}

void SpawnTableEditor::EnsureTablesLoaded()
{
    if (m_TablesLoaded) return;
    ReloadTableCache();
}

void SpawnTableEditor::ReloadTableCache()
{
    ItemTableDatabase::LoadAll("DungeonData\\ItemTables\\");
    EnemyTableDatabase::LoadAll("DungeonData\\EnemyTables\\");
    TrapTableDatabase::LoadAll("DungeonData\\TrapTables\\");

    m_ItemTables = ItemTableDatabase::GetAll();
    m_EnemyTables = EnemyTableDatabase::GetAll();
    m_TrapTables = TrapTableDatabase::GetAll();
    m_TablesLoaded = true;
}

void SpawnTableEditor::DrawTableEditorTab()
{
    EnsureTablesLoaded();

    ImGui::RadioButton("Item", (int*)&m_CurrentCategory, (int)TableCategory::Item); ImGui::SameLine();
    ImGui::RadioButton("Enemy", (int*)&m_CurrentCategory, (int)TableCategory::Enemy); ImGui::SameLine();
    ImGui::RadioButton("Trap", (int*)&m_CurrentCategory, (int)TableCategory::Trap);
    if (ImGui::Button("Reload Existing Tables", ImVec2(-1, 0)))
    {
        ReloadTableCache();
    }
    ImGui::Separator();

    switch (m_CurrentCategory)
    {
		/// それぞれのカテゴリに対して、テーブルの選択、編集、保存、削除を行うUIを描画する関数を呼び出す
    case TableCategory::Item:  DrawTableEditorContents(m_ItemTables, TableCategory::Item); break;
    case TableCategory::Enemy: DrawTableEditorContents(m_EnemyTables, TableCategory::Enemy); break;
    case TableCategory::Trap:  DrawTableEditorContents(m_TrapTables, TableCategory::Trap); break;
    }
}

template<typename T>
void SpawnTableEditor::DrawTableEditorContents(std::unordered_map<std::string, T>& tables, TableCategory category)
{
    std::string& selectedId = m_SelectedIds[category];

    if (selectedId != m_LastSelectedId)
    {
        strcpy_s(m_RenameBuf, selectedId.c_str());
        m_LastSelectedId = selectedId;
    }

    if (ImGui::Button("+ Create New Table", ImVec2(-1, 40)))
    {
        int index = (int)tables.size();
        std::string newId = "NewTable_" + std::to_string(index);
        while (tables.find(newId) != tables.end())
            newId = "NewTable_" + std::to_string(++index);
        tables[newId].tableId = newId;
        selectedId = newId;
    }

    ImGui::Separator();

    ImGui::Text("Current Table:");
    if (ImGui::BeginCombo("##TableSelect", selectedId.empty() ? "Select a table..." : selectedId.c_str()))
    {
        for (auto& [id, table] : tables)
        {
            if (ImGui::Selectable(id.c_str(), id == selectedId))
                selectedId = id;
        }
        ImGui::EndCombo();
    }

    if (selectedId.empty() || tables.find(selectedId) == tables.end())
    {
        ImGui::TextDisabled("No table selected.");
        return;
    }

    auto& table = tables[selectedId];
    ImGui::Text("Rename ID:");
    ImGui::SameLine();
    if (ImGui::InputText("##RenameField", m_RenameBuf, sizeof(m_RenameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        std::string newId = m_RenameBuf;
        if (!newId.empty() && newId != selectedId && tables.find(newId) == tables.end())
        {
            auto node = tables.extract(selectedId);
            node.key() = newId;
            tables.insert(std::move(node));
            tables[newId].tableId = newId;
            selectedId = newId;
            m_LastSelectedId = newId;
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Save", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 30)))
    {
        std::string newIdFromBuf = m_RenameBuf;
        if (!newIdFromBuf.empty() && newIdFromBuf != selectedId && tables.find(newIdFromBuf) == tables.end())
        {
            auto node = tables.extract(selectedId);
            node.key() = newIdFromBuf;
            tables.insert(std::move(node));
            tables[newIdFromBuf].tableId = newIdFromBuf;
            selectedId = newIdFromBuf;
            m_LastSelectedId = newIdFromBuf;
        }

        auto& currentTable = tables[selectedId];
        std::filesystem::create_directories(GetTableDirectory(category));
        std::string path = GetTableDirectory(category) + currentTable.tableId + ".json";

        SpawnTableIO::Save(path, currentTable);

        if (category == TableCategory::Item) ItemTableDatabase::LoadAll("DungeonData\\ItemTables\\");
        else if (category == TableCategory::Enemy) EnemyTableDatabase::LoadAll("DungeonData\\EnemyTables\\");
        else if (category == TableCategory::Trap) TrapTableDatabase::LoadAll("DungeonData\\TrapTables\\");
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Delete", ImVec2(ImGui::GetContentRegionAvail().x, 30)))
    {
        tables.erase(selectedId);
        selectedId = "";
        ImGui::PopStyleColor();
        return;
    }
    ImGui::PopStyleColor();

    if (selectedId != m_LastSelectedId)
    {
        strcpy_s(m_RenameBuf, selectedId.c_str());
        m_LastSelectedId = selectedId;
    }

    ImGui::Separator();

    if (ImGui::Button("Add New Entry", ImVec2(-1, 35)))
        table.entries.push_back({ GetDefaultEntryId(category), 1 });

    ImGui::BeginChild("EntryList", ImVec2(0, 0), true);
    for (int i = 0; i < (int)table.entries.size(); ++i)
    {
        ImGui::PushID(i);
        ImGui::BeginChildFrame(ImGui::GetID("entry_frame"), ImVec2(0, 85), ImGuiWindowFlags_NoScrollbar);

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Target:");
        ImGui::SameLine();
        DrawIDCombo(category, table.entries[i].id);

        ImGui::Text("Weight:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::DragInt("##Weight", &table.entries[i].weight, 1, 0, 1000);

        ImGui::SameLine(ImGui::GetWindowWidth() - 45);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));

        if (ImGui::Button("X", ImVec2(35, 35)))
        {
            table.entries.erase(table.entries.begin() + i);
            ImGui::PopStyleColor(3);
            ImGui::EndChildFrame();
            ImGui::PopID();
            break;
        }
        ImGui::PopStyleColor(3);

        ImGui::EndChildFrame();
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void SpawnTableEditor::DrawIDCombo(TableCategory category, std::string& ioId)
{
    std::string previewName = "Unknown";
    if (category == TableCategory::Item)
    {
        if (auto* data = ItemDatabase::Get(ioId)) previewName = data->name;
    }
    else if (category == TableCategory::Enemy)
    {
        auto& enemies = EnemyDatabase::GetAll();
        if (enemies.count(ioId)) previewName = enemies.at(ioId).name;
    }
    else if (category == TableCategory::Trap)
    {
        if (auto* data = TrapDatabase::Get(ioId)) previewName = data->name;
    }

    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("##IDCombo", previewName.c_str()))
    {
        if (category == TableCategory::Item)
        {
            for (const auto& item : ItemDatabase::GetAllData())
            {
                if (ImGui::Selectable(item.name.c_str(), ioId == item.id))
                    ioId = item.id;
            }
        }
        else if (category == TableCategory::Enemy)
        {
            for (const auto& [id, data] : EnemyDatabase::GetAll())
            {
                if (ImGui::Selectable(data.name.c_str(), ioId == id))
                    ioId = id;
            }
        }
        else if (category == TableCategory::Trap)
        {
            for (const auto& id : TrapDatabase::GetAllIDs())
            {
                const TrapData* data = TrapDatabase::Get(id);
                if (!data) continue;
                if (ImGui::Selectable(data->name.c_str(), ioId == id))
                    ioId = id;
            }
        }
        ImGui::EndCombo();
    }
}
