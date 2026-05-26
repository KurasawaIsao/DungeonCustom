#include "SpecialRoomGenerater.h"
#include "GameRandom.h"
#include "DungeonData.h"
#include "GeneraterPlacer.h"
#include "MapData.h"
#include "Room.h"
#include "scene.h"
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace
{
    constexpr int kMaxMonsterHouseTraps = 20;
    constexpr int kMaxMonsterHouseEnemies = 20;
    constexpr int kMaxMonsterHouseItems = 10;
    constexpr int kMaxTreasureRoomItems = 12;
    constexpr int kMonsterHouseTrapMinDensity = 40;
    constexpr int kMonsterHouseTrapMaxDensity = 50;
    constexpr int kMonsterHouseEnemyMinDensity = 80;
    constexpr int kMonsterHouseEnemyMaxDensity = 90;
    constexpr int kMonsterHouseItemMinDensity = 60;
    constexpr int kMonsterHouseItemMaxDensity = 70;

    int RandomDensity(int minDensity, int maxDensity)
    {
        if (maxDensity < minDensity) return minDensity;
        return GameRandom::Range(minDensity, maxDensity);
    }

    int CalculateOccupancyCount(int area, int minDensity, int maxDensity, int maxCount)
    {
        if (area <= 0 || maxCount <= 0) return 0;
        const int density = RandomDensity(minDensity, maxDensity);
        return (std::min)(maxCount, (area * density + 99) / 100);
    }

    bool GetShopFloorSquare(const Room& room, Vector2Int& topLeft, int& side)
    {
        const Vector2Int pos = room.GetPosition();
        const Vector2Int size = room.GetSize();
        const int innerW = (std::max)(0, size.x - 2);
        const int innerH = (std::max)(0, size.y - 2);
        side = (std::min)(innerW, innerH);
        if (side <= 0) return false;

        topLeft.x = pos.x + 1 + (innerW - side) / 2;
        topLeft.y = pos.y + 1 + (innerH - side) / 2;
        return true;
    }

    bool IsInsideShopFloorSquare(const Vector2Int& pos, const Vector2Int& topLeft, int side)
    {
        return pos.x >= topLeft.x && pos.x < topLeft.x + side
            && pos.y >= topLeft.y && pos.y < topLeft.y + side;
    }

    void PlaceMonsterHouseObjects(MapData* map, Scene* scene, Room& room, const FloorData& floor)
    {
        const int area = GeneraterPlacer::GetRoomArea(room, map);
        const int enemyCount = CalculateOccupancyCount(area, kMonsterHouseEnemyMinDensity, kMonsterHouseEnemyMaxDensity, kMaxMonsterHouseEnemies);
        const int itemCount = CalculateOccupancyCount(area, kMonsterHouseItemMinDensity, kMonsterHouseItemMaxDensity, kMaxMonsterHouseItems);
        const int trapCount = CalculateOccupancyCount(area, kMonsterHouseTrapMinDensity, kMonsterHouseTrapMaxDensity, kMaxMonsterHouseTraps);

        for (int i = 0; i < enemyCount; ++i)
        {
            auto tiles = GeneraterPlacer::CollectRoomTiles(room, map, false);
            if (tiles.empty()) break;
            GeneraterPlacer::PlaceEnemyAt(map, scene, tiles[GameRandom::Index(tiles.size())], floor, true);
        }

        for (int i = 0; i < itemCount; ++i)
        {
            auto tiles = GeneraterPlacer::CollectRoomTiles(room, map, false);
            if (tiles.empty()) break;
            GeneraterPlacer::PlaceItemAt(map, scene, tiles[GameRandom::Index(tiles.size())], room.specialItemTableId);
        }

        for (int i = 0; i < trapCount; ++i)
        {
            auto tiles = GeneraterPlacer::CollectRoomTiles(room, map, false);
            if (tiles.empty()) break;
            GeneraterPlacer::PlaceTrapAt(map, scene, tiles[GameRandom::Index(tiles.size())], floor.trapTableId);
        }
    }

    void PlaceTreasureRoomObjects(MapData* map, Scene* scene, Room& room, const FloorData& floor)
    {
        const int area = GeneraterPlacer::GetRoomArea(room, map);
        const int itemCount = GeneraterPlacer::CalculateDensityCount(area, 35, kMaxTreasureRoomItems);

        for (int i = 0; i < itemCount; ++i)
        {
            auto tiles = GeneraterPlacer::CollectRoomTiles(room, map, false);
            if (tiles.empty()) break;
            GeneraterPlacer::PlaceItemAt(map, scene, tiles[GameRandom::Index(tiles.size())], floor.itemTableId);
        }
    }

    void PlaceShopObjects(MapData* map, Scene* scene, Room& room, const FloorData& floor)
    {
        for (const Vector2Int& pos : GeneraterPlacer::CollectRoomTiles(room, map, true))
        {
            if (GameRandom::Percent(room.specialTrapDensity))
                GeneraterPlacer::PlaceTrapAt(map, scene, pos, floor.trapTableId);
        }

        Vector2Int shopTopLeft;
        int shopSide = 0;
        if (!GetShopFloorSquare(room, shopTopLeft, shopSide)) return;

        const Vector2Int center(shopTopLeft.x + shopSide / 2, shopTopLeft.y + shopSide / 2);
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                Vector2Int pos(center.x + dx, center.y + dy);
                if (!IsInsideShopFloorSquare(pos, shopTopLeft, shopSide)) continue;
                if (!room.Contains(pos)) continue;
                if (map->GetTile(pos.x, pos.y) != TileType::Floor) continue;
                GeneraterPlacer::PlaceItemAt(map, scene, pos, room.specialItemTableId, true);
            }
        }

        std::vector<Vector2Int> keeperTiles;
        for (const Vector2Int& ent : room.entrances)
        {
            static const Vector2Int dirs[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
            for (const Vector2Int& d : dirs)
            {
                Vector2Int p = ent + d;
                if (room.Contains(p) && map->IsTileFree(p))
                    keeperTiles.push_back(p);
            }
        }
        if (keeperTiles.empty())
            keeperTiles = GeneraterPlacer::CollectRoomTiles(room, map, false);

        if (!keeperTiles.empty())
            GeneraterPlacer::PlaceShopKeeperAt(map, scene, keeperTiles[GameRandom::Index(keeperTiles.size())], false);
    }
}

void SpecialRoomGenerator::AssignSpecialRooms(MapData* map, const FloorData& floor)
{
    if (!map) return;

    std::vector<Room*> candidates;
    for (Room& room : map->GetRooms())
    {
        const bool isTreasureRoom = (room.specialType == RoomSpecialType::Treasure);
        if (!isTreasureRoom)
            room.specialType = RoomSpecialType::Normal;
        room.specialTrapDensity = 0;
        room.specialMessageShown = false;
        room.specialItemTableId = isTreasureRoom ? floor.itemTableId : "";

        if (!room.isFixed && !isTreasureRoom)
            candidates.push_back(&room);
    }

    if (candidates.empty()) return;

    if (GameRandom::Percent(GeneraterPlacer::ClampPercent(floor.shopAppearanceRate)))
    {
        const int index = GameRandom::Index(candidates.size());
        Room* room = candidates[index];
        room->specialType = RoomSpecialType::Shop;
        room->specialTrapDensity = GeneraterPlacer::ClampPercent(floor.shopTrapDensity);
        room->specialItemTableId = floor.shopItemTableId.empty() ? floor.itemTableId : floor.shopItemTableId;
        candidates.erase(candidates.begin() + index);
    }

    if (!candidates.empty() && GameRandom::Percent(GeneraterPlacer::ClampPercent(floor.monsterHouseAppearanceRate)))
    {
        Room* room = candidates[GameRandom::Index(candidates.size())];
        room->specialType = RoomSpecialType::MonsterHouse;
        room->specialItemTableId = floor.itemTableId;
    }
}

void SpecialRoomGenerator::PlaceSpecialRoomObjects(MapData* map, Scene* scene, const FloorData& floor)
{
    if (!map || !scene) return;

    for (Room& room : map->GetRooms())
    {
        if (room.specialType == RoomSpecialType::MonsterHouse)
            PlaceMonsterHouseObjects(map, scene, room, floor);
        else if (room.specialType == RoomSpecialType::Shop)
            PlaceShopObjects(map, scene, room, floor);
        else if (room.specialType == RoomSpecialType::Treasure)
            PlaceTreasureRoomObjects(map, scene, room, floor);
    }
}


