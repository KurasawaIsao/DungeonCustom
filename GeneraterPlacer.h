#pragma once
#include "Vector2Int.h"
#include <string>
#include <vector>

class MapData;
class Room;
class Scene;
struct FloorData;

class GeneraterPlacer
{
public:
    static int ClampPercent(int value);
    static int GetRoomArea(const Room& room, const MapData* map);
    static int CalculateDensityCount(int area, int density, int maxCount);

    static std::vector<Vector2Int> CollectRoomTiles(const Room& room, MapData* map, bool borderOnly);
    static bool PlaceTrapAt(MapData* map, Scene* scene, const Vector2Int& pos, const std::string& tableId = "");
    static bool PlaceItemAt(MapData* map, Scene* scene, const Vector2Int& pos, const std::string& tableId, bool shopItem = false);
    static bool PlaceEnemyAt(MapData* map, Scene* scene, const Vector2Int& pos, const FloorData& floor, bool nap);
    static bool PlaceShopKeeperAt(MapData* map, Scene* scene, const Vector2Int& pos, bool hostile);
};
