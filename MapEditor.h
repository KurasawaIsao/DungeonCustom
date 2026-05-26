#pragma once
#include "GameObject.h"
#include "MapData.h"
#include "DungeonData.h"
#include "ImguiObject.h"
#include "SpawnTableEditor.h"
#include "DungeonStructureEditor.h"
#include "EditorMode.h"
#include <vector>
#include <string>
#include "MapObjectPlacer.h"
#include "MapGeometryEditor.h"
#include "MapFileManagement.h"


class MapEditor : public GameObject, public ImGuiObject {
private:
    bool m_Enabled = true;
    // --- マップ設計用データ ---
    MapData* m_Map = nullptr;
    int m_NewWidth, m_NewHeight;
    char m_MapFileName[128] = "map01.json";
    char m_RoomPartFileName[128] = "room01.json";
    EditCategory m_EditCategory = EditCategory::Geometry;
    EditorPlaceMode m_ObjectMode = EditorPlaceMode::Item;
    TileType m_SelectedTileType = TileType::Wall;

    std::vector<EditorPlacedObject> m_PlacedObjects;

  
 

    Vector2Int m_HoverTile = { -1, -1 };

    // --- ダンジョン設計用データ ---
    DungeonData m_Dungeon;
    int m_SelectedFloorIndex = -1;
    char m_DungeonID[64] = "NewDungeon";
    char m_TestDungeonID[64] = "";
    int m_SelectedTestDungeonIndex = 0;
    bool m_RandomizeTestPlaySeed = true;

    // --- ファイルリスト ---
    std::vector<std::string> m_MapFileList;
    std::vector<std::string> m_RoomPartFileList;
    std::vector<std::string> m_DungeonFileList;

    // --- サブエディタ ---
    MapGeometryEditor    m_GeometryEditor;
    MapObjectPlacer      m_ObjectPlacer;
    DungeonStructureEditor m_DungeonEditor;
	MapFileManagement	m_FileManager;
	SpawnTableEditor m_SpawnTableEditor;


public:
    void Init() override;
    void Update() override;
    void Draw() override;
    void Uninit() override;
    void DrawImGui() override;

private:
    void DrawMapEditorWindow();      // ウィンドウ1：マップ・アイテム配置
    void DrawDungeonEditorWindow();   // ウィンドウ2：ダンジョン・階層設計

    // 内部ロジック
    void SyncRenderer();
    void UpdateHover();
    void HandleMouse();
    void DrawRoomPreviewUI();
    std::string GetMapPath(const std::string& name, ExportDirectory directory) const;
    std::string GetDungeonPath() const;
    std::string GetSelectedTestDungeonPath() const;
    std::string GetTestMapPath() const;
    void SyncTestDungeonSelection(bool force);
    void DrawTestPlayControls();
    void StartTestPlay();
    void RefreshFileList();

    void ExportMap(ExportDirectory directory);
    void ImportMap(const std::string& path);
};