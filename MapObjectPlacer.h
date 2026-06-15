#pragma once
#include <vector>
#include <string>
#include "Vector2Int.h"
#include "EditorMode.h"
class MapData;
struct EditorPlacedObject {
    enum class Type { Item, Trap, Shrine, Enemy };
    Type type;
    std::string id;
    Vector2Int pos;
    class MapObject* view = nullptr;
    class Enemy* enemyView = nullptr;

    int potCapacity = -1;
    int staffCharges = -1;
    int stackCount = -1;
};

class MapObjectPlacer {
private:
    std::vector<EditorPlacedObject> m_PlacedObjects;

    // UI用の一時変数
    std::string m_SelectedItemId = "Food_Bread";
    std::string m_SelectedTrapId = "Trap_Sleep";
    std::string m_SelectedEnemyId = "Kingyo";
    int m_SelectedItemCount = 3;

public:
    void DrawPlacerTab(MapData* map,EditorPlaceMode mode);

    void InitializeSelection();

    // 配置ロジック
    void PlaceItem(MapData* map, int x, int y);
    void PlaceTrap(MapData* map, int x, int y);
    void PlaceShrine(MapData* map, int x, int y);
    void PlaceEnemy(MapData* map, int x, int y);
    void RemoveObjectAt(MapData* map, int x, int y);

    void PlaceItemByID(MapData* map, std::string id, int x, int y, int count);
    void PlaceTrapByID(MapData* map, std::string id, int x, int y);
    void PlaceEnemyByID(MapData* map, std::string id, int x, int y);
    // 外部（シリアライザ等）からのアクセス用
    const std::vector<EditorPlacedObject>& GetPlacedObjects() const { return m_PlacedObjects; }
    void ClearAllObjects(MapData* map);
};