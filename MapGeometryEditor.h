#pragma once
#include "MapData.h"
#include <vector>

class MapGeometryEditor {
private:
    // 地形編集の状態
    TileType m_SelectedTileType = TileType::Wall;

    // リサイズ用入力バッファ
    int m_NewWidth = 50;
    int m_NewHeight = 50;

    TileType m_TargetReplaceType = TileType::Floor;

public:
    // UI描画（MapEditorから呼ばれる）

    bool DrawGeometryTab(MapData* map, class MapObjectPlacer& placer);

    // 地形操作ロジック
    void ApplyTile(MapData* map, Vector2Int pos);
    void ResizeMap(MapData** mapPtr, class MapObjectPlacer& placer);  // mapのポインタ自体を差し替える可能性があるためポインタのポインタ

    void FillPerimeter(MapData* map);

    // 部屋情報の同期（タイルからRoomRectsを再構築するロジックの移動）
    void UpdateRoomDataFromTiles(MapData* map);

    // ゲッター
    TileType GetSelectedTileType() const { return m_SelectedTileType; }
};