#include "GameRandom.h"
#include "MapGenerator.h"
#include "Ally.h"
#include "CorridorGenarator.h"
#include "DungeonData.h"
#include "Enemy.h"
#include "EnemyDatabase.h"
#include "GeneraterPlacer.h"
#include "manager.h"
#include "MapManager.h"
#include "Player.h"
#include "RoomGenerator.h"
#include "scene.h"
#include "SpecialRoomGenerater.h"
#include "TurnManager.h"
#include "UnitManager.h"
#include <cstdlib>
#include <memory>
#include <vector>

// MapGenerator は「フロアを作る手順」を並べる薄い司令塔。
// 実際の部屋生成は RoomGenerator、通路は CorridorGenerator、特殊部屋は SpecialRoomGenerator、
// 敵・アイテム・罠の配置は GeneraterPlacer に分け、どこを調整すべきか追いやすくしている。

namespace
{
    bool IsInPlayerRoom(MapData* map, Player* player, const Vector2Int& pos)
    {
        if (!map || !player) return false;

        const Room* playerRoom = map->GetRoomAt(player->GetGridPos());
        return playerRoom && playerRoom == map->GetRoomAt(pos);
    }

    bool FindFreeFloorOutsidePlayerRoom(MapData* map, Player* player, Vector2Int& outPos)
    {
        if (!map) return false;

        std::vector<Vector2Int> candidates;
        for (int y = 0; y < map->GetHeight(); ++y)
        {
            for (int x = 0; x < map->GetWidth(); ++x)
            {
                const Vector2Int pos(x, y);
                if (!map->IsTileFree(pos)) continue;
                if (player && player->GetGridPos() == pos) continue;
                if (IsInPlayerRoom(map, player, pos)) continue;
                candidates.push_back(pos);
            }
        }

        if (candidates.empty()) return false;

        outPos = candidates[GameRandom::Index(candidates.size())];
        return true;
    }

    bool RoomHasUnit(MapData* map, const Room& room)
    {
        if (!map) return false;

        const Vector2Int pos = room.GetPosition();
        const Vector2Int size = room.GetSize();
        for (int y = pos.y; y < pos.y + size.y; ++y)
        {
            for (int x = pos.x; x < pos.x + size.x; ++x)
            {
                const Vector2Int tile(x, y);
                if (!room.Contains(tile)) continue;
                if (map->GetUnitAt(x, y)) return true;
            }
        }

        return false;
    }
}

std::unique_ptr<MapData> MapGenerator::Generate(int width, int height, Scene* scene)
{
    auto map = std::make_unique<MapData>(width, height);
    const FloorData floor = MapManager::Instance()->GetCurrentFloorData();

    // MapGenerator はフロア生成の手順だけを管理する。
    // 部屋生成・特殊部屋・配置処理を別 Generator に分けることで、各処理の調整箇所を追いやすくする。
    const MapLayoutType layoutType = RoomGenerator::GenerateRooms(map.get(), floor, scene);

    auto& rooms = map->GetRooms();
    // 大部屋は1部屋だけで完結するため、通路生成を走らせず床面を保つ。
    if (layoutType != MapLayoutType::LargeRoom)
    {
        const auto mst = CorridorGenerator::BuildMST(rooms);
        CorridorGenerator corridorGen;
        corridorGen.ApplyCorridors(map.get(), mst, true, floor.corridorComplexity);
    }

    // 通路のあとで入口を調べる。店番や巡回AIが「どこが入口か」を地形生成の詳細なしで使えるため。
    RoomGenerator::ScanRoomEntrances(map.get());

    // 特殊部屋は通常部屋とは別ルールで配置物を置くため、部屋形状と通路が確定してから処理する。
    SpecialRoomGenerator::AssignSpecialRooms(map.get(), floor);
    SpecialRoomGenerator::PlaceSpecialRoomObjects(map.get(), scene, floor);

    std::vector<Room*> normalRooms;
    for (Room& room : rooms)
    {
        if (room.specialType == RoomSpecialType::Normal)
            normalRooms.push_back(&room);
    }

    if (!normalRooms.empty())
    {
        // 通常部屋だけにランダム罠を追加し、店やモンスターハウスの演出ルールと混ざらないようにする。
        for (int i = 0; i < floor.maxTrapCount; ++i)
        {
            Room& room = *normalRooms[GameRandom::Index(normalRooms.size())];
            auto tiles = GeneraterPlacer::CollectRoomTiles(room, map.get(), false);
            if (tiles.empty()) break;

            GeneraterPlacer::PlaceTrapAt(map.get(), scene, tiles[GameRandom::Index(tiles.size())], floor.trapTableId);
        }
    }

    if (!normalRooms.empty())
    {
        Room& room = *normalRooms[GameRandom::Index(normalRooms.size())];
        auto tiles = GeneraterPlacer::CollectRoomTiles(room, map.get(), false);
        if (!tiles.empty())
        {
            const Vector2Int pos = tiles[GameRandom::Index(tiles.size())];
            map->SetTile(pos.x, pos.y, TileType::Stair);
        }
    }

    return map;
}

void MapGenerator::SetAllies()
{
    // 階層移動後、仲間はプレイヤー周辺の空きマスへ再配置する。
    // 周囲が埋まっている場合だけ、保険として任意の歩行可能マスを探す。
    auto allies = Manager::GetScene()->GetGameObjects<Ally>();
    if (allies.empty()) return;

    Player* player = Manager::GetScene()->GetGameObject<Player>();
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!player || !map) return;

    Vector2Int pPos = player->GetGridPos();

    Vector2Int offsets[] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    int offsetIndex = 0;

    for (auto& ally : allies)
    {
        if (!ally) continue;
        ally->ResetTurnSpeedToBase();

        bool placed = false;
        while (offsetIndex < 8)
        {
            Vector2Int checkPos = pPos + offsets[offsetIndex];
            ++offsetIndex;

            if (map->IsInBounds(checkPos) && map->IsWalkable(checkPos) && map->IsTileFree(checkPos))
            {
                ally->SetGridPos(checkPos);
                ally->SetPosition(Vector3(checkPos.x * (float)TILE_DISTANCE, 0.0f, checkPos.y * (float)TILE_DISTANCE));

                placed = true;
                break;
            }
        }

        if (!placed)
        {
            int x, y;
            if (map->FindAnyWalkableTile(map, x, y))
            {
                Vector2Int backupPos(x, y);
                ally->SetGridPos(backupPos);
                ally->SetPosition(Vector3(x * 2.0f, 0.0f, y * 2.0f));
            }
        }
    }
}

void MapGenerator::SpawnPlayerInRoom()
{
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return;

    int x = 0, y = 0;
    bool found = false;

    std::vector<const Room*> startRooms;
    for (const Room& room : map->GetRooms())
    {
        if (!RoomHasUnit(map, room))
            startRooms.push_back(&room);
    }

    if (!startRooms.empty())
    {
        for (int i = 0; i < 100; ++i)
        {
            const Room* room = startRooms[GameRandom::Index(startRooms.size())];
            if (room->GetSize().x <= 2 || room->GetSize().y <= 2) continue;

            x = GameRandom::Range(room->GetPosition().x + 1, room->GetPosition().x + room->GetSize().x - 2);
            y = GameRandom::Range(room->GetPosition().y + 1, room->GetPosition().y + room->GetSize().y - 2);

            if (map->GetTile(x, y) == TileType::Floor && map->IsTileFree({ x, y }))
            {
                found = true;
                break;
            }
        }
    }

    if (!found)
        found = map->FindAnyWalkableTile(map, x, y);

    Player* player = Manager::GetScene()->GetGameObject<Player>();
    if (player && found)
        player->ResetPosition(Vector2Int(x, y));
}

void MapGenerator::SetEnemies(const FloorData& floor)
{
    // フロア開始時の初期敵配置。
    // 店の中とプレイヤーのいる部屋には通常敵を置かず、特殊部屋は SpecialRoomGenerator 側に任せる。
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return;

    Player* player = UnitManager::Instance()->GetPlayer();
    if (!player) return;

    Room* playerRoom = map->GetRoomAt(player->GetGridPos());

    std::vector<Room*> enemyRooms;
    for (Room& room : map->GetRooms())
    {
        if (&room != playerRoom && room.specialType != RoomSpecialType::Shop)
            enemyRooms.push_back(&room);
    }

    Scene* scene = Manager::GetScene();

    for (int i = 0; i < floor.maxEnemyCount; ++i)
    {
        Vector2Int pos;
        bool found = false;

        if (!enemyRooms.empty())
        {
            Room& room = *enemyRooms[GameRandom::Index(enemyRooms.size())];
            auto tiles = GeneraterPlacer::CollectRoomTiles(room, map, false);
            if (!tiles.empty())
            {
                pos = tiles[GameRandom::Index(tiles.size())];
                found = true;
            }
        }

        if (!found)
            found = FindFreeFloorOutsidePlayerRoom(map, player, pos);

        if (found)
            GeneraterPlacer::PlaceEnemyAt(map, scene, pos, floor, false);
    }
}

void MapGenerator::SetEnemiesOne(const FloorData& floor)
{
    // TurnManager の自然湧き用。プレイヤーのいる部屋と店は避け、1体だけ追加する。
    // 泥棒状態では通常敵ではなく敵対店番を湧かせる。
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return;

    Vector2Int pos;
    bool found = false;

    Player* player = UnitManager::Instance()->GetPlayer();
    if (!player) return;

    Room* playerRoom = map->GetRoomAt(player->GetGridPos());

    std::vector<Room*> candidateRooms;
    for (Room& room : map->GetRooms())
    {
        if (&room != playerRoom && room.specialType != RoomSpecialType::Shop)
            candidateRooms.push_back(&room);
    }

    if (!candidateRooms.empty())
    {
        Room& room = *candidateRooms[GameRandom::Index(candidateRooms.size())];
        auto tiles = GeneraterPlacer::CollectRoomTiles(room, map, false);
        if (!tiles.empty())
        {
            pos = tiles[GameRandom::Index(tiles.size())];
            found = true;
        }
    }

    if (!found)
        found = FindFreeFloorOutsidePlayerRoom(map, player, pos);

    if (!found) return;

    const bool theftMode = TurnManager::Instance()->IsShopTheftMode();
    if (!theftMode)
    {
        GeneraterPlacer::PlaceEnemyAt(map, Manager::GetScene(), pos, floor, false);
        return;
    }

    GeneraterPlacer::PlaceShopKeeperAt(map, Manager::GetScene(), pos, true);
}

void MapGenerator::SetItems(const FloorData& floor)
{
    // フロア開始時の通常アイテム配置。
    // 商品やモンスターハウス報酬は特殊部屋側で置くため、ここでは通常部屋だけを対象にする。
    MapData* currentMap = MapManager::Instance()->GetCurrentMap();
    if (!currentMap) return;

    std::vector<Room*> candidateRooms;
    for (Room& room : currentMap->GetRooms())
    {
        if (!room.isFixed && room.specialType == RoomSpecialType::Normal)
            candidateRooms.push_back(&room);
    }

    if (candidateRooms.empty()) return;

    Scene* scene = Manager::GetScene();
    for (int i = 0; i < floor.maxItemCount; ++i)
    {
        Room& room = *candidateRooms[GameRandom::Index(candidateRooms.size())];
        auto tiles = GeneraterPlacer::CollectRoomTiles(room, currentMap, false);
        if (tiles.empty()) continue;

        GeneraterPlacer::PlaceItemAt(currentMap, scene, tiles[GameRandom::Index(tiles.size())], floor.itemTableId);
    }
}
