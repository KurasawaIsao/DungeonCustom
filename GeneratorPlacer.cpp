#include "GameRandom.h"
#include "GeneraterPlacer.h"
#include "DungeonData.h"
#include "Enemy.h"
#include "EnemyDatabase.h"
#include "Item.h"
#include "ItemData.h"
#include "ItemDataBase.h"
#include "MapData.h"
#include "MapManager.h"
#include "Room.h"
#include "scene.h"
#include "Trap.h"
#include "TrapDataBase.h"
#include "UnitManager.h"
#include <algorithm>
#include <cstdlib>

namespace
{
    Player* GetPlayerOnMap(MapData* map)
    {
        MapManager* mapManager = MapManager::Instance();
        if (!map || !mapManager || mapManager->GetCurrentMap() != map) return nullptr;

        return UnitManager::Instance()->GetPlayer();
    }

    bool IsPlayerGridPos(MapData* map, const Vector2Int& pos)
    {
        Player* player = GetPlayerOnMap(map);
        return player && player->GetGridPos() == pos;
    }

    bool IsInPlayerRoom(MapData* map, const Vector2Int& pos)
    {
        Player* player = GetPlayerOnMap(map);
        if (!map || !player) return false;

        const Room* playerRoom = map->GetRoomAt(player->GetGridPos());
        return playerRoom && playerRoom == map->GetRoomAt(pos);
    }
}

int GeneraterPlacer::ClampPercent(int value)
{
    return (std::max)(0, (std::min)(100, value));
}

int GeneraterPlacer::GetRoomArea(const Room& room, const MapData* map)
{
    if (!map) return 0;

    int area = 0;
    const Vector2Int pos = room.GetPosition();
    const Vector2Int size = room.GetSize();
    for (int y = pos.y; y < pos.y + size.y; ++y)
    {
        for (int x = pos.x; x < pos.x + size.x; ++x)
        {
            if (room.Contains({ x, y }) && map->GetTile(x, y) == TileType::Floor)
                ++area;
        }
    }
    return area;
}

int GeneraterPlacer::CalculateDensityCount(int area, int density, int maxCount)
{
    if (area <= 0 || density <= 0 || maxCount <= 0) return 0;
    return (std::min)(maxCount, (area * density + 99) / 100);
}

std::vector<Vector2Int> GeneraterPlacer::CollectRoomTiles(const Room& room, MapData* map, bool borderOnly)
{
    std::vector<Vector2Int> tiles;
    if (!map) return tiles;

    const Vector2Int pos = room.GetPosition();
    const Vector2Int size = room.GetSize();
    for (int y = pos.y; y < pos.y + size.y; ++y)
    {
        for (int x = pos.x; x < pos.x + size.x; ++x)
        {
            if (!room.Contains({ x, y })) continue;
            if (map->GetTile(x, y) != TileType::Floor) continue;
            if (!map->IsTileFree({ x, y })) continue;
            if (IsPlayerGridPos(map, { x, y })) continue;
            if (borderOnly && !map->IsRoomBorderTile(x, y)) continue;
            tiles.push_back({ x, y });
        }
    }
    return tiles;
}

bool GeneraterPlacer::PlaceTrapAt(MapData* map, Scene* scene, const Vector2Int& pos, const std::string& tableId)
{
    if (!map || !scene || !map->IsTileFree(pos)) return false;
    if (IsPlayerGridPos(map, pos)) return false;

    const TrapData* data = tableId.empty()
        ? TrapDatabase::GetRandom()
        : TrapDatabase::DrawFromTable(tableId);
    if (!data) return false;

    auto* trap = scene->AddGameObject<Trap>(1);
    trap->Init();
    trap->Setup(data);
    trap->SetGridPos(pos);
    trap->SetPosition(Vector3(pos.x * 2.0f, 0.01f, pos.y * 2.0f));
    map->AddMapObject(trap, pos.x, pos.y);
    return true;
}

bool GeneraterPlacer::PlaceItemAt(MapData* map, Scene* scene, const Vector2Int& pos, const std::string& tableId, bool shopItem)
{
    if (!map || !scene || !map->IsTileFree(pos)) return false;
    if (IsPlayerGridPos(map, pos)) return false;

    const ItemData* data = ItemDatabase::DrawFromTable(tableId);
    if (!data) return false;

    auto* item = scene->AddGameObject<Item>(1);
    ItemInstance inst(data);

    if (data->type == ItemType::Weapon || data->type == ItemType::Shield)
    {
        inst.SetPlusValue(GameRandom::Range(0, 3));
    }
    if (data->type == ItemType::Pot)
    {
        inst.CreatePot(GameRandom::Range(4, 6), true);
    }
    if (data->type == ItemType::Staff)
    {
        inst.SetCharge(GameRandom::Range(4, 6));
    }
    if (data->type == ItemType::Arrow || data->type == ItemType::Stone)
    {
        inst.SetStackCount(GameRandom::Range(3, 8));
    }

    const bool blessOrCurseEnabled = MapManager::Instance()
        ? MapManager::Instance()->GetDungeonData().IsBlessOrCurseEnabled()
        : true;
    inst.InitIdentify(blessOrCurseEnabled);
    if (MapManager::Instance()->GetDungeonData().GetUnidentifiedMode() == UnidentifiedMode::None)
    {
        inst.RevealOption();
    }
    item->SetInstance(std::move(inst));
    if (shopItem) item->SetShopItem(true);
    item->SetGridPos(pos);
    item->SetPosition(Vector3(pos.x * 2.0f, 0.01f, pos.y * 2.0f));
    map->AddMapObject(item, pos.x, pos.y);
    return true;
}

bool GeneraterPlacer::PlaceEnemyAt(MapData* map, Scene* scene, const Vector2Int& pos, const FloorData& floor, bool nap)
{
    if (!map || !scene || !map->IsTileFree(pos)) return false;
    if (IsPlayerGridPos(map, pos) || IsInPlayerRoom(map, pos)) return false;

    const EnemyData* data = EnemyDatabase::DrawFromTable(floor.enemyTableId);
    if (!data) return false;

    Enemy* enemy = scene->AddGameObject<Enemy>(1);
    enemy->ApplyData(*data);
    // モンスターハウス生成時は仮眠状態を付けるが、敵データ側ですでに仮眠になっている場合は二重付与しない。
    if (nap && enemy->GetStatus() != Status::Nap)
        enemy->SetStatus(Status::Nap, -1);
    // マップ生成中は MapManager の currentMap がまだ新マップを指していないため、初期配置用の座標設定を使う。
    enemy->SetInitGridPos(pos);
    UnitManager::Instance()->RegisterEnemy(enemy);
    return true;
}

bool GeneraterPlacer::PlaceShopKeeperAt(MapData* map, Scene* scene, const Vector2Int& pos, bool hostile)
{
    if (!map || !scene || !map->IsTileFree(pos)) return false;

    const EnemyData* data = EnemyDatabase::Get("ShopKeeper");
    if (!data) data = EnemyDatabase::DrawFromTable("ShopKeeper");
    if (!data) return false;

    Enemy* enemy = scene->AddGameObject<Enemy>(1);
    enemy->ApplyData(*data);
    enemy->SetShopKeeperMode(hostile);
    // 店番はマップ生成中に置くので、旧マップの座標判定に引っかからない初期配置用APIを使う。
    enemy->SetInitGridPos(pos);
    UnitManager::Instance()->RegisterEnemy(enemy);
    return true;
}
