#pragma once
#include "DungeonData.h"
#include <string>
#include <vector>

class DungeonStructureEditor {
public:
    // UIï`âÊÅFDungeonDataÇÃéQè∆ÇéÛÇØéÊÇ¡Çƒï“èWÇ∑ÇÈ
    void DrawDungeonEditorWindow(
        DungeonData& dungeon,
        bool& enabled,
        char* dungeonIDBuffer,
        const std::vector<std::string>& dungeonFileList,
        const std::vector<std::string>& mapFileList,
        const std::vector<std::string>& roomPartFileList
    );

private:
    int m_SelectedFloorIndex = -1;
    std::vector<unsigned char> m_BulkTargetFloors;
    bool m_MirrorEditsToBulkTargets = false;
    bool m_BulkApplyAllSettings = true;
    bool m_BulkApplyTheme = false;
    bool m_BulkApplyMapSource = false;
    bool m_BulkApplyMapSize = false;
    bool m_BulkApplyMinRoomCount = false;
    bool m_BulkApplyMaxRoomCount = false;
    bool m_BulkApplyCorridorComplexity = false;
    bool m_BulkApplyGenerationSeedOffset = false;
    bool m_BulkApplyLayoutWeights = false;
    bool m_BulkApplyFixedRooms = false;
    bool m_BulkApplyViewDistance = false;
    bool m_BulkApplyPlayerVisionClear = false;
    bool m_BulkApplyWindTurnLimit = false;
    bool m_BulkApplyMaxEnemyCount = false;
    bool m_BulkApplyMaxItemCount = false;
    bool m_BulkApplyMaxTrapCount = false;
    bool m_BulkApplyMonsterHouseRate = false;
    bool m_BulkApplyShopRate = false;
    bool m_BulkApplyShopTrapDensity = false;
    bool m_BulkApplyEnemyTable = false;
    bool m_BulkApplyItemTable = false;
    bool m_BulkApplyTrapTable = false;
    bool m_BulkApplyShopItemTable = false;
    void RefreshFileList(); 
    void EnsureBulkTargetSize(int floorCount);
    int GetBulkTargetCount(int floorCount, int sourceIndex) const;
    bool HasSelectedBulkApplyFields() const;
    void ApplySelectedFloorSettings(const FloorData& source, FloorData& target) const;
    void ApplySourceFloorToTargets(DungeonData& dungeon, bool applyToAllFloors) const;
};