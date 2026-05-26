#include "DungeonStructureEditor.h"
#include "imgui.h"
#include "DungeonDataIO.h"
#include "EnemyTableDatabase.h"
#include "ItemTableDatabase.h"
#include "TrapTableDatabase.h"
#include <algorithm>

namespace
{
    constexpr MapLayoutType kLayoutTypes[] =
    {
        MapLayoutType::Random,
        MapLayoutType::CrossFive,
        MapLayoutType::SmallRoomChain,
        MapLayoutType::TreasureRoomChain,
        MapLayoutType::TShapeFour,
        MapLayoutType::SoilEight,
        MapLayoutType::LShape,
        MapLayoutType::LargeRoom,
    };

    constexpr const char* kLayoutNames[] =
    {
        "Normal Random",
        "Cross 5 Rooms",
        "Small Chain",
        "Treasure Chain",
        "To Shape 4 Rooms",
        "Tsuchi Shape 8 Rooms",
        "L Shape",
        "Large Room",
    };

    int FindLayoutWeightIndex(const FloorData& floor, MapLayoutType type)
    {
        for (int i = 0; i < (int)floor.layoutWeights.size(); ++i)
        {
            if (floor.layoutWeights[i].type == type)
                return i;
        }
        return -1;
    }

    int& GetLayoutWeight(FloorData& floor, MapLayoutType type)
    {
        const int index = FindLayoutWeightIndex(floor, type);
        if (index >= 0)
            return floor.layoutWeights[index].weight;

        floor.layoutWeights.push_back({ type, 0 });
        return floor.layoutWeights.back().weight;
    }
}

void DungeonStructureEditor::EnsureBulkTargetSize(int floorCount)
{
    if (floorCount < 0)
        floorCount = 0;

    m_BulkTargetFloors.resize((size_t)floorCount, 0);
}

int DungeonStructureEditor::GetBulkTargetCount(int floorCount, int sourceIndex) const
{
    int count = 0;
    const int targetCount = (std::min)(floorCount, (int)m_BulkTargetFloors.size());
    for (int i = 0; i < targetCount; ++i)
    {
        if (i != sourceIndex && m_BulkTargetFloors[i] != 0)
            ++count;
    }
    return count;
}
bool DungeonStructureEditor::HasSelectedBulkApplyFields() const
{
    return m_BulkApplyMapSource ||
        m_BulkApplyMapSize ||
        m_BulkApplyMinRoomCount ||
        m_BulkApplyMaxRoomCount ||
        m_BulkApplyCorridorComplexity ||
        m_BulkApplyGenerationSeedOffset ||
        m_BulkApplyLayoutWeights ||
        m_BulkApplyFixedRooms ||
        m_BulkApplyViewDistance ||
        m_BulkApplyPlayerVisionClear ||
        m_BulkApplyMaxEnemyCount ||
        m_BulkApplyMaxItemCount ||
        m_BulkApplyMaxTrapCount ||
        m_BulkApplyMonsterHouseRate ||
        m_BulkApplyShopRate ||
        m_BulkApplyShopTrapDensity ||
        m_BulkApplyEnemyTable ||
        m_BulkApplyItemTable ||
        m_BulkApplyTrapTable ||
        m_BulkApplyShopItemTable;
}

void DungeonStructureEditor::ApplySelectedFloorSettings(const FloorData& source, FloorData& target) const
{
    // 選択式の一括反映では、チェックした項目だけを現在階層から対象階層へコピーする。
    if (m_BulkApplyMapSource)
    {
        target.mapSource = source.mapSource;
        target.mapFilePath = source.mapFilePath;
    }
    if (m_BulkApplyMapSize)
    {
        target.width = source.width;
        target.height = source.height;
        target.useRandomMapSize = source.useRandomMapSize;
        target.useFullyRandomMapSize = source.useFullyRandomMapSize;
        target.minWidth = source.minWidth;
        target.maxWidth = source.maxWidth;
        target.minHeight = source.minHeight;
        target.maxHeight = source.maxHeight;
    }
    if (m_BulkApplyMinRoomCount)
    {
        target.minRoomCount = source.minRoomCount;
    }
    if (m_BulkApplyMaxRoomCount)
    {
        target.maxRoomCount = source.maxRoomCount;
    }
    if (m_BulkApplyCorridorComplexity)
    {
        target.corridorComplexity = source.corridorComplexity;
    }
    if (m_BulkApplyGenerationSeedOffset)
    {
        target.generationSeedOffset = source.generationSeedOffset;
    }
    if (m_BulkApplyLayoutWeights)
    {
        target.layoutWeights = source.layoutWeights;
    }
    if (m_BulkApplyFixedRooms)
    {
        target.useFixedRoom = source.useFixedRoom;
        target.spawnAllFixedRooms = source.spawnAllFixedRooms;
        target.fixedRoomPaths = source.fixedRoomPaths;
    }
    if (m_BulkApplyViewDistance)
    {
        target.viewDistance = source.viewDistance;
    }
    if (m_BulkApplyPlayerVisionClear)
    {
        target.playerVisionClear = source.playerVisionClear;
    }
    if (m_BulkApplyMaxEnemyCount)
    {
        target.maxEnemyCount = source.maxEnemyCount;
    }
    if (m_BulkApplyMaxItemCount)
    {
        target.maxItemCount = source.maxItemCount;
    }
    if (m_BulkApplyMaxTrapCount)
    {
        target.maxTrapCount = source.maxTrapCount;
    }
    if (m_BulkApplyMonsterHouseRate)
    {
        target.monsterHouseAppearanceRate = source.monsterHouseAppearanceRate;
    }
    if (m_BulkApplyShopRate)
    {
        target.shopAppearanceRate = source.shopAppearanceRate;
    }
    if (m_BulkApplyShopTrapDensity)
    {
        target.shopTrapDensity = source.shopTrapDensity;
    }
    if (m_BulkApplyEnemyTable)
    {
        target.enemyTableId = source.enemyTableId;
    }
    if (m_BulkApplyItemTable)
    {
        target.itemTableId = source.itemTableId;
    }
    if (m_BulkApplyTrapTable)
    {
        target.trapTableId = source.trapTableId;
    }
    if (m_BulkApplyShopItemTable)
    {
        target.shopItemTableId = source.shopItemTableId;
    }
}
void DungeonStructureEditor::ApplySourceFloorToTargets(DungeonData& dungeon, bool applyToAllFloors) const
{
    const int floorCount = dungeon.GetFloorCount();
    if (m_SelectedFloorIndex < 0 || m_SelectedFloorIndex >= floorCount)
        return;

    const FloorData sourceFloor = dungeon.GetFloor(m_SelectedFloorIndex);
    const int targetCount = applyToAllFloors ? floorCount : (std::min)(floorCount, (int)m_BulkTargetFloors.size());
    for (int i = 0; i < targetCount; ++i)
    {
        if (i == m_SelectedFloorIndex)
            continue;
        if (!applyToAllFloors && m_BulkTargetFloors[i] == 0)
            continue;

        if (m_BulkApplyAllSettings)
            dungeon.GetFloor(i) = sourceFloor;
        else
            ApplySelectedFloorSettings(sourceFloor, dungeon.GetFloor(i));
    }
}

void DungeonStructureEditor::DrawDungeonEditorWindow(
    DungeonData& dungeon,
    bool& enabled,
    char* dungeonIDBuffer,
    const std::vector<std::string>& dungeonFileList,
    const std::vector<std::string>& mapFileList,
    const std::vector<std::string>& roomPartFileList
) {
    ImGui::SetNextWindowPos(ImVec2(50, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 750), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Dungeon Hierarchy Editor", &enabled)) {
        EnsureBulkTargetSize(dungeon.GetFloorCount());
        auto mirrorToBulkTargets = [&](auto apply) {
            if (!m_MirrorEditsToBulkTargets)
                return;
            const int floorCount = dungeon.GetFloorCount();
            if (m_SelectedFloorIndex < 0 || m_SelectedFloorIndex >= floorCount)
                return;

            EnsureBulkTargetSize(floorCount);
            const int targetCount = (std::min)(floorCount, (int)m_BulkTargetFloors.size());
            for (int i = 0; i < targetCount; ++i) {
                if (i == m_SelectedFloorIndex || m_BulkTargetFloors[i] == 0)
                    continue;
                apply(dungeon.GetFloor(i));
            }
        };
        auto clearBulkApplyFields = [&]() {
            m_BulkApplyMapSource = false;
            m_BulkApplyMapSize = false;
            m_BulkApplyMinRoomCount = false;
            m_BulkApplyMaxRoomCount = false;
            m_BulkApplyCorridorComplexity = false;
            m_BulkApplyGenerationSeedOffset = false;
            m_BulkApplyLayoutWeights = false;
            m_BulkApplyFixedRooms = false;
            m_BulkApplyViewDistance = false;
            m_BulkApplyPlayerVisionClear = false;
            m_BulkApplyMaxEnemyCount = false;
            m_BulkApplyMaxItemCount = false;
            m_BulkApplyMaxTrapCount = false;
            m_BulkApplyMonsterHouseRate = false;
            m_BulkApplyShopRate = false;
            m_BulkApplyShopTrapDensity = false;
            m_BulkApplyEnemyTable = false;
            m_BulkApplyItemTable = false;
            m_BulkApplyTrapTable = false;
            m_BulkApplyShopItemTable = false;
        };

        ImGui::Separator();
        ImGui::Spacing();

        // --- 1. ファイル保存・管理 ---
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ File Management ]");
        ImGui::InputText("Dungeon ID", dungeonIDBuffer, 64);

        const char* unidentifiedModeNames[] = { "None", "Half Assign", "All Assign" };
        int unidentifiedMode = (int)dungeon.GetUnidentifiedMode();
        if (ImGui::Combo("Unidentified Mode", &unidentifiedMode, unidentifiedModeNames, 3)) {
            dungeon.SetUnidentifiedMode((UnidentifiedMode)unidentifiedMode);
        }
        bool blessOrCurseEnabled = dungeon.IsBlessOrCurseEnabled();
        if (ImGui::Checkbox("Enable Bless/Curse", &blessOrCurseEnabled)) {
            dungeon.SetBlessOrCurseEnabled(blessOrCurseEnabled);
        }

        if (ImGui::Button("Export Dungeon", ImVec2(150, 0))) {
            dungeon.SetDungeonId(dungeonIDBuffer);
            DungeonDataIO::SaveToFile("DungeonData\\DungeonContext\\" + std::string(dungeonIDBuffer) + ".json", dungeon);
        }
        ImGui::SameLine();

        // --- 2. ダンジョンデータの読み込み・挿入 ---
        ImGui::Separator();
        ImGui::Text("Load / Insert Dungeon:");
        static std::string selectedDungeonFile = "";
        if (ImGui::BeginCombo("Select Dungeon File", selectedDungeonFile.empty() ? "Choose..." : selectedDungeonFile.c_str())) {
            for (auto& file : dungeonFileList) {
                if (ImGui::Selectable(file.c_str())) selectedDungeonFile = file;
            }
            ImGui::EndCombo();
        }

        if (!selectedDungeonFile.empty()) {
            if (ImGui::Button("Overwrite (Load)")) {
                dungeon = DungeonData();
                DungeonDataIO::LoadFromFile("DungeonData\\DungeonContext\\" + selectedDungeonFile, dungeon);
                m_SelectedFloorIndex = -1;
                m_BulkTargetFloors.clear();
                EnsureBulkTargetSize(dungeon.GetFloorCount());
                std::string idName = selectedDungeonFile.substr(0, selectedDungeonFile.find_last_of("."));
                strncpy_s(dungeonIDBuffer, 64, idName.c_str(), 64);
            }
            ImGui::SameLine();
            if (ImGui::Button("Append (Insert)")) {
                DungeonData temp;
                DungeonDataIO::LoadFromFile("DungeonData\\DungeonContext\\" + selectedDungeonFile, temp);
                for (int i = 0; i < temp.GetFloorCount(); ++i) dungeon.AddFloor(temp.GetFloor(i));
                EnsureBulkTargetSize(dungeon.GetFloorCount());
            }
        }

        ImGui::Separator();

        bool useGenerationSeed = dungeon.UseGenerationSeed();
        if (ImGui::Checkbox("Use Generation Seed", &useGenerationSeed)) {
            dungeon.SetUseGenerationSeed(useGenerationSeed);
        }
        int generationSeed = dungeon.GetGenerationSeed();
        if (ImGui::InputInt("Generation Seed", &generationSeed)) {
            dungeon.SetGenerationSeed(generationSeed);
        }

        // --- 3. 階層リスト ---
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[ Floor Hierarchy ]");
        if (ImGui::Button("Add New Floor")) {
            FloorData f{};
            f.width = 50; f.height = 50;
            dungeon.AddFloor(f);
            EnsureBulkTargetSize(dungeon.GetFloorCount());
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove This Floor")) {
            if (m_SelectedFloorIndex >= 0 && m_SelectedFloorIndex < dungeon.GetFloorCount()) {
                dungeon.RemoveFloor(m_SelectedFloorIndex);
                if (m_SelectedFloorIndex < (int)m_BulkTargetFloors.size()) {
                    m_BulkTargetFloors.erase(m_BulkTargetFloors.begin() + m_SelectedFloorIndex);
                }
            }
            m_SelectedFloorIndex = -1;
            EnsureBulkTargetSize(dungeon.GetFloorCount());
        }

        EnsureBulkTargetSize(dungeon.GetFloorCount());
        ImGui::BeginChild("FloorList", ImVec2(0, 150), true);
        for (int i = 0; i < dungeon.GetFloorCount(); ++i) {
            auto& floor = dungeon.GetFloor(i);
            std::string label = "Floor " + std::to_string(i);
            if (floor.mapSource == MapSourceType::FixedMap) label += " (Fixed)";

            ImGui::PushID(i);
            bool targetChecked = (i < (int)m_BulkTargetFloors.size() && m_BulkTargetFloors[i] != 0);
            if (ImGui::Checkbox("##BulkTarget", &targetChecked)) {
                m_BulkTargetFloors[i] = targetChecked ? 1 : 0;
            }
            ImGui::SameLine();
            if (ImGui::Selectable(label.c_str(), m_SelectedFloorIndex == i)) m_SelectedFloorIndex = i;
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::SeparatorText("Bulk Settings");
        const int bulkTargetCount = GetBulkTargetCount(dungeon.GetFloorCount(), m_SelectedFloorIndex);
        ImGui::Text("Checked target floors: %d", bulkTargetCount);
        if (ImGui::Button("Check All Targets")) {
            EnsureBulkTargetSize(dungeon.GetFloorCount());
            for (int i = 0; i < (int)m_BulkTargetFloors.size(); ++i) m_BulkTargetFloors[i] = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Targets")) {
            for (int i = 0; i < (int)m_BulkTargetFloors.size(); ++i) m_BulkTargetFloors[i] = 0;
        }

        const bool canUseBulkSource = (m_SelectedFloorIndex >= 0 && m_SelectedFloorIndex < dungeon.GetFloorCount());
        if (!canUseBulkSource) ImGui::BeginDisabled();
        ImGui::Checkbox("Mirror Edits To Checked Floors", &m_MirrorEditsToBulkTargets);
        if (!canUseBulkSource) ImGui::EndDisabled();

        ImGui::SeparatorText("Bulk Apply Fields");
        ImGui::Checkbox("Apply All Settings", &m_BulkApplyAllSettings);
        if (!m_BulkApplyAllSettings)
        {
            if (ImGui::Button("Player Vision Only"))
            {
                clearBulkApplyFields();
                m_BulkApplyPlayerVisionClear = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Fields"))
            {
                clearBulkApplyFields();
            }

            // 必要な項目だけを選んで、選択中階層からチェック済み階層へ反映する。
            ImGui::Checkbox("Map Source / File##BulkApplyMapSource", &m_BulkApplyMapSource);
            ImGui::Checkbox("Map Size##BulkApplyMapSize", &m_BulkApplyMapSize);
            ImGui::Checkbox("Min Rooms##BulkApplyMinRoomCount", &m_BulkApplyMinRoomCount);
            ImGui::Checkbox("Max Rooms##BulkApplyMaxRoomCount", &m_BulkApplyMaxRoomCount);
            ImGui::Checkbox("Corridor Complexity##BulkApplyCorridorComplexity", &m_BulkApplyCorridorComplexity);
            ImGui::Checkbox("Generation Seed Offset##BulkApplyGenerationSeedOffset", &m_BulkApplyGenerationSeedOffset);
            ImGui::Checkbox("Floor Generation Type##BulkApplyLayoutWeights", &m_BulkApplyLayoutWeights);
            ImGui::Checkbox("Fixed Room Parts##BulkApplyFixedRooms", &m_BulkApplyFixedRooms);
            ImGui::Checkbox("View Distance##BulkApplyViewDistance", &m_BulkApplyViewDistance);
            ImGui::Checkbox("Player Vision Clear##BulkApplyPlayerVisionClear", &m_BulkApplyPlayerVisionClear);
            ImGui::Checkbox("Max Enemy Count##BulkApplyMaxEnemyCount", &m_BulkApplyMaxEnemyCount);
            ImGui::Checkbox("Max Item Count##BulkApplyMaxItemCount", &m_BulkApplyMaxItemCount);
            ImGui::Checkbox("Max Trap Count##BulkApplyMaxTrapCount", &m_BulkApplyMaxTrapCount);
            ImGui::Checkbox("Monster House Rate##BulkApplyMonsterHouseRate", &m_BulkApplyMonsterHouseRate);
            ImGui::Checkbox("Shop Rate##BulkApplyShopRate", &m_BulkApplyShopRate);
            ImGui::Checkbox("Shop Trap Density##BulkApplyShopTrapDensity", &m_BulkApplyShopTrapDensity);
            ImGui::Checkbox("Enemy Table##BulkApplyEnemyTable", &m_BulkApplyEnemyTable);
            ImGui::Checkbox("Item Table##BulkApplyItemTable", &m_BulkApplyItemTable);
            ImGui::Checkbox("Trap Table##BulkApplyTrapTable", &m_BulkApplyTrapTable);
            ImGui::Checkbox("Shop Item Table##BulkApplyShopItemTable", &m_BulkApplyShopItemTable);
            if (!HasSelectedBulkApplyFields())
            {
                ImGui::TextDisabled("Select at least one field to apply.");
            }
        }

        const bool canApplyBulkFields = m_BulkApplyAllSettings || HasSelectedBulkApplyFields();
        if (!canUseBulkSource || bulkTargetCount <= 0 || !canApplyBulkFields) ImGui::BeginDisabled();
        if (ImGui::Button("Apply Source To Checked")) {
            ApplySourceFloorToTargets(dungeon, false);
        }
        if (!canUseBulkSource || bulkTargetCount <= 0 || !canApplyBulkFields) ImGui::EndDisabled();
        ImGui::SameLine();
        if (!canUseBulkSource || dungeon.GetFloorCount() <= 1 || !canApplyBulkFields) ImGui::BeginDisabled();
        if (ImGui::Button("Apply Source To All")) {
            ApplySourceFloorToTargets(dungeon, true);
        }
        if (!canUseBulkSource || dungeon.GetFloorCount() <= 1 || !canApplyBulkFields) ImGui::EndDisabled();

        // --- 4. 階層ごとの詳細設定 ---
        if (m_SelectedFloorIndex >= 0 && m_SelectedFloorIndex < dungeon.GetFloorCount()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Detail: Floor %d", m_SelectedFloorIndex);

            FloorData& f = dungeon.GetFloor(m_SelectedFloorIndex);
            bool detailChanged = false;

            const char* sourceNames[] = { "Auto (Random)", "Fixed (Full Map File)" };
            int currentSource = (int)f.mapSource;
            if (ImGui::Combo("Map Source", &currentSource, sourceNames, 2)) {
                f.mapSource = (MapSourceType)currentSource;
                detailChanged = true;
            }

            bool isFixed = (f.mapSource == MapSourceType::FixedMap);
            if (isFixed) ImGui::BeginDisabled();
            if (ImGui::Checkbox("Fully Random Map Size", &f.useFullyRandomMapSize)) {
                if (f.useFullyRandomMapSize) f.useRandomMapSize = false;
                detailChanged = true;
            }
            if (!f.useFullyRandomMapSize) {
                if (ImGui::Checkbox("Random Map Size", &f.useRandomMapSize)) {
                    detailChanged = true;
                }
                if (f.useRandomMapSize) {
                    detailChanged |= ImGui::DragInt("Min Width", &f.minWidth, 1, 10, 200);
                    detailChanged |= ImGui::DragInt("Max Width", &f.maxWidth, 1, 10, 200);
                    detailChanged |= ImGui::DragInt("Min Height", &f.minHeight, 1, 10, 200);
                    detailChanged |= ImGui::DragInt("Max Height", &f.maxHeight, 1, 10, 200);
                }
                else {
                    detailChanged |= ImGui::DragInt("Width", &f.width, 1, 10, 200);
                    detailChanged |= ImGui::DragInt("Height", &f.height, 1, 10, 200);
                }
            }
            else {
                ImGui::TextDisabled("40-90 random width / height");
            }
            if (isFixed) {
                ImGui::EndDisabled();
                ImGui::TextDisabled(u8"※Fixedモードではファイル側のサイズが優先されます");
            }

            if (isFixed) {
                ImGui::Text("mapFilePath (Full Map):");
                if (ImGui::BeginCombo("##mapFilePath", f.mapFilePath.empty() ? "Select..." : f.mapFilePath.c_str())) {
                    for (auto& map : mapFileList) {
                        if (ImGui::Selectable(map.c_str())) {
                            f.mapFilePath = "DungeonData\\EdittedMapData\\" + map;
                            detailChanged = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else {
                detailChanged |= ImGui::SliderInt("Min Rooms", &f.minRoomCount, 1, 10);
                detailChanged |= ImGui::SliderInt("Max Rooms", &f.maxRoomCount, 1, 20);
                detailChanged |= ImGui::SliderInt("Corridor Complexity", &f.corridorComplexity, 0, 100);
                detailChanged |= ImGui::InputInt("Generation Seed Offset", &f.generationSeedOffset);
                ImGui::SeparatorText("Floor Generation Type");
                int totalLayoutWeight = 0;
                for (int i = 0; i < (int)(sizeof(kLayoutTypes) / sizeof(kLayoutTypes[0])); ++i) {
                    int& weight = GetLayoutWeight(f, kLayoutTypes[i]);
                    detailChanged |= ImGui::SliderInt(kLayoutNames[i], &weight, 0, 100);
                    totalLayoutWeight += (std::max)(0, weight);
                }
                ImGui::Text("Total Weight: %d", totalLayoutWeight);
                if (ImGui::Button("Normal Only")) {
                    for (int i = 0; i < (int)(sizeof(kLayoutTypes) / sizeof(kLayoutTypes[0])); ++i) {
                        GetLayoutWeight(f, kLayoutTypes[i]) = (kLayoutTypes[i] == MapLayoutType::Random) ? 100 : 0;
                    }
                    detailChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Large Room Only")) {
                    for (int i = 0; i < (int)(sizeof(kLayoutTypes) / sizeof(kLayoutTypes[0])); ++i) {
                        GetLayoutWeight(f, kLayoutTypes[i]) = (kLayoutTypes[i] == MapLayoutType::LargeRoom) ? 100 : 0;
                    }
                    detailChanged = true;
                }
                ImGui::Separator();
                detailChanged |= ImGui::Checkbox("useFixedRoom (Parts)", &f.useFixedRoom);
                if (f.useFixedRoom) {
                    ImGui::Indent();
                    detailChanged |= ImGui::Checkbox("spawnAllFixedRooms", &f.spawnAllFixedRooms);
                    for (size_t i = 0; i < f.fixedRoomPaths.size(); ++i) {
                        ImGui::PushID((int)i);
                        if (ImGui::Button("Delete")) {
                            f.fixedRoomPaths.erase(f.fixedRoomPaths.begin() + i);
                            detailChanged = true;
                            ImGui::PopID(); break;
                        }
                        ImGui::SameLine();
                        std::string fileName = f.fixedRoomPaths[i].path.substr(f.fixedRoomPaths[i].path.find_last_of("\\") + 1);
                        ImGui::Text("%s", fileName.c_str());
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(100);
                        detailChanged |= ImGui::SliderInt("Rate(%)", &f.fixedRoomPaths[i].appearanceRate, 0, 100);
                        ImGui::PopID();
                    }
                    if (ImGui::Button("+ Add Room Part")) ImGui::OpenPopup("AddPartPopup");
                    if (ImGui::BeginPopup("AddPartPopup")) {
                        for (auto& room : roomPartFileList) {
                            if (ImGui::Selectable(room.c_str())) {
                                FixedRoomSetting newSetting;
                                newSetting.path = "DungeonData\\EdittedRoomData\\" + room;
                                newSetting.appearanceRate = 100;
                                f.fixedRoomPaths.push_back(newSetting);
                                detailChanged = true;
                            }
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::Unindent();
                }
            }
            ImGui::Separator();
            detailChanged |= ImGui::SliderInt("View Distance", &f.viewDistance, 0, 20);
            detailChanged |= ImGui::Checkbox("Player Vision Clear", &f.playerVisionClear);
            ImGui::Separator();
            detailChanged |= ImGui::InputInt("maxEnemyCount", &f.maxEnemyCount);
            detailChanged |= ImGui::InputInt("maxItemCount", &f.maxItemCount);
            detailChanged |= ImGui::InputInt("maxTrapCount", &f.maxTrapCount);
            ImGui::Separator();
            detailChanged |= ImGui::SliderInt("monsterHouseAppearanceRate(%)", &f.monsterHouseAppearanceRate, 0, 100);
            detailChanged |= ImGui::SliderInt("shopAppearanceRate(%)", &f.shopAppearanceRate, 0, 100);
            detailChanged |= ImGui::SliderInt("shopTrapDensity(%)", &f.shopTrapDensity, 0, 100);
            ImGui::Separator();

            if (ImGui::BeginCombo("enemyTableId", f.enemyTableId.c_str())) {
                for (auto& id : EnemyTableDatabase::GetAllIds()) if (ImGui::Selectable(id.c_str())) { f.enemyTableId = id; detailChanged = true; }
                ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("itemTableId", f.itemTableId.c_str())) {
                for (auto& id : ItemTableDatabase::GetAllIds()) if (ImGui::Selectable(id.c_str())) { f.itemTableId = id; detailChanged = true; }
                ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("trapTableId", f.trapTableId.c_str())) {
                for (auto& id : TrapTableDatabase::GetAllIds()) if (ImGui::Selectable(id.c_str())) { f.trapTableId = id; detailChanged = true; }
                ImGui::EndCombo();
            }
            const char* shopTableLabel = f.shopItemTableId.empty() ? f.itemTableId.c_str() : f.shopItemTableId.c_str();
            if (ImGui::BeginCombo("shopItemTableId", shopTableLabel)) {
                for (auto& id : ItemTableDatabase::GetAllIds()) if (ImGui::Selectable(id.c_str())) { f.shopItemTableId = id; detailChanged = true; }
                ImGui::EndCombo();
            }

            if (detailChanged) {
                mirrorToBulkTargets([&](FloorData& target) { target = f; });
            }
        }
    }
    ImGui::End();
}

