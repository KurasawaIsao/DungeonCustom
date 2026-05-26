#include "MapLoader.h"
#include "JsonIO.h"
#include "Item.h"
#include "ItemDataBase.h"
#include "ItemData.h"
#include "ItemInstance.h"
#include "MapManager.h"
#include "Trap.h"
#include "TrapDataBase.h"
#include "Shrine.h"
#include <fstream>
#include "external/json.hpp"
using json = nlohmann::json;
std::unique_ptr<MapData> MapLoader::LoadFromFile(const std::string& path, Scene* scene)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) return nullptr;

    json root;
    ifs >> root;

    const json& jmap = root["map"];
    int w = jmap["width"];
    int h = jmap["height"];

    auto map = std::make_unique<MapData>(w, h);

    //部屋
    if (root.contains("rooms")) {
        std::vector<Room> loadedRooms;
        for (const auto& rj : root["rooms"]) {
            Room room({ rj["x"], rj["y"] }, { rj["w"], rj["h"] });
            room.isFixed = rj.value("isFixed", false);

            if (rj.contains("subRects")) {
                for (const auto& srj : rj["subRects"]) {
                    room.AddSubRect({ srj["x"], srj["y"] }, { 1, 1 });
                }
            }
            loadedRooms.push_back(room);
        }
        map->AddRooms(loadedRooms);
    }

    // マス
    const json& tiles = jmap["tiles"];
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            map->SetTile(x, y,
                (TileType)tiles[y][x].get<int>());

   // アイテム
    if (root.contains("items"))
    {
        for (const auto& j : root["items"])
        {
            std::string id = j["id"].get<std::string>();

            Vector2Int pos{
                j["x"].get<int>(),
                j["y"].get<int>()
            };

            auto* item = scene->AddGameObject<Item>(1);

            const ItemData* data = ItemDatabase::Get(id);
            if (!data) continue;

            ItemInstance inst(data);

            if (data->type == ItemType::Pot)
            {
                int cap = j.value("potCapacity", 4);
                inst.CreatePot(cap, true);
            }
            else if (data->type == ItemType::Staff) 
            {
                int charges = j.value("staffCharges", 5);
                inst.SetCharge(charges);
            }
            if (data->type == ItemType::Arrow || data->type == ItemType::Stone)
            {
                inst.SetStackCount(j.value("stackCount", 3));
            }
            inst.InitIdentify(MapManager::Instance() ? MapManager::Instance()->GetDungeonData().IsBlessOrCurseEnabled() : true);
            item->SetInstance(std::move(inst));
            item->SetGridPos(pos);
            item->SetPosition({ pos.x * 2.0f, 0.01f, pos.y * 2.0f });

            map->AddMapObject(item, pos.x, pos.y);
        }
    }
    if (root.contains("traps")) {
        for (const auto& j : root["traps"]) {
            std::string id = j["id"].get<std::string>();
            Vector2Int pos{ j["x"].get<int>(), j["y"].get<int>() };

            auto* trap = scene->AddGameObject<Trap>(1);
            const TrapData* data = TrapDatabase::Get(id);
            if (!data) continue;
            if (data) {
                trap->Init();     
                trap->Setup(data);
            }
            trap->SetGridPos(pos);
            trap->SetPosition({ pos.x * 2.0f, 0.01f, pos.y * 2.0f });

            map->AddMapObject(trap, pos.x, pos.y);
        }
    }

    if (root.contains("shrines")) {
        for (const auto& j : root["shrines"]) {
            Vector2Int pos{ j["x"].get<int>(), j["y"].get<int>() };
            auto* shrine = scene->AddGameObject<Shrine>(1);
            shrine->Init();
            shrine->Setup(pos); 
            map->AddMapObject(shrine, pos.x, pos.y);
        }
    }


    return map;
}

std::unique_ptr<MapData> MapLoader::LoadDataOnly(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open())
        return nullptr;

    json root;
    ifs >> root;

    const json& jmap = root.at("map");

    int w = jmap.at("width").get<int>();
    int h = jmap.at("height").get<int>();

    auto map = std::make_unique<MapData>(w, h);

    const json& tiles = jmap.at("tiles");
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            map->SetTile(
                x, y,
                static_cast<TileType>(tiles[y][x].get<int>())
            );
        }
    }

    return map;
}

void MapLoader::InsertFixedRoom(MapData* targetMap, const std::string& path, Vector2Int offset, Scene* scene) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return;

    nlohmann::json root;
    ifs >> root;

    const auto& jmap = root["map"];
    int w = jmap["width"];
    int h = jmap["height"];
    const auto& tiles = jmap["tiles"];

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            TileType type = (TileType)tiles[y][x].get<int>();
            targetMap->SetTile(offset.x + x, offset.y + y, type);
        }
    }

    if (root.contains("items")) {
        for (const auto& j : root["items"]) {
            std::string id = j["id"].get<std::string>();
            Vector2Int localPos{ j["x"].get<int>(), j["y"].get<int>() };
            Vector2Int worldGridPos = offset + localPos;

            auto* item = scene->AddGameObject<Item>(1);
            const ItemData* data = ItemDatabase::Get(id);
            if (!data) continue;

            ItemInstance inst(data);
            // 壺・杖などの個別設定
            if (data->type == ItemType::Pot) inst.CreatePot(j.value("potCapacity", 4), true);
            else if (data->type == ItemType::Staff) inst.SetCharge(j.value("staffCharges", 5));
            if (data->type == ItemType::Arrow || data->type == ItemType::Stone) inst.SetStackCount(j.value("stackCount", 3));

            inst.InitIdentify(MapManager::Instance() ? MapManager::Instance()->GetDungeonData().IsBlessOrCurseEnabled() : true);
            item->SetInstance(std::move(inst));
            item->SetGridPos(worldGridPos);

            item->SetPosition({ worldGridPos.x * 2.0f, 0.01f, worldGridPos.y * 2.0f });

            targetMap->AddMapObject(item, worldGridPos.x, worldGridPos.y);
        }
    }
    if (root.contains("traps")) {
        for (const auto& j : root["traps"]) {
            std::string id = j["id"].get<std::string>();
            Vector2Int worldGridPos = offset + Vector2Int{ j["x"].get<int>(), j["y"].get<int>() };

            auto* trap = scene->AddGameObject<Trap>(1);
            const TrapData* data = TrapDatabase::Get(id);
            if (!data) continue;

            if (data) {
                trap->Init();      
                trap->Setup(data); 
            }
            trap->SetGridPos(worldGridPos);
            trap->SetPosition({ worldGridPos.x * 2.0f, 0.01f, worldGridPos.y * 2.0f });
            targetMap->AddMapObject(trap, worldGridPos.x, worldGridPos.y);
        }
    }

    if (root.contains("shrines")) {
        for (const auto& j : root["shrines"]) {
            Vector2Int worldGridPos = offset + Vector2Int{ j["x"].get<int>(), j["y"].get<int>() };
            auto* shrine = scene->AddGameObject<Shrine>(1);
            shrine->Init();
            shrine->Setup(worldGridPos);
            targetMap->AddMapObject(shrine, worldGridPos.x, worldGridPos.y);
        }
    }

 
}