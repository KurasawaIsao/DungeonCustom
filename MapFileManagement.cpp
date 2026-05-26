#include "MapFileManagement.h"
#include <fstream>

bool MapFileManagement::ExportMap(const std::string& path, MapData* map, const std::vector<EditorPlacedObject>& placedObjects) {
    if (!map) return false;

    nlohmann::json root;

    // --- 1. 地形データ ---
    root["map"]["width"] = map->GetWidth();
    root["map"]["height"] = map->GetHeight();
    nlohmann::json tiles = nlohmann::json::array();
    for (int y = 0; y < map->GetHeight(); y++) {
        nlohmann::json row = nlohmann::json::array();
        for (int x = 0; x < map->GetWidth(); x++) {
            row.push_back((int)map->GetTile(x, y));
        }
        tiles.push_back(row);
    }
    root["map"]["tiles"] = tiles;

    // --- 2. 部屋情報 ---
    nlohmann::json roomsJson = nlohmann::json::array();
    for (auto& room : map->GetRooms()) {
        nlohmann::json r;
        r["x"] = room.GetPosition().x;
        r["y"] = room.GetPosition().y;
        r["w"] = room.GetSize().x;
        r["h"] = room.GetSize().y;
        r["isFixed"] = room.isFixed;

        nlohmann::json subRectsJson = nlohmann::json::array();
        for (const auto& rect : room.GetSubRects()) {
            subRectsJson.push_back({ {"x", rect.pos.x}, {"y", rect.pos.y} });
        }
        r["subRects"] = subRectsJson;
        roomsJson.push_back(r);
    }
    root["rooms"] = roomsJson;

    // --- 3. オブジェクトの振り分け ---
    nlohmann::json items = nlohmann::json::array();
    nlohmann::json traps = nlohmann::json::array();
    nlohmann::json shrines = nlohmann::json::array();

    for (const auto& obj : placedObjects) {
        nlohmann::json data;
        data["x"] = obj.pos.x;
        data["y"] = obj.pos.y;

        switch (obj.type) {
        case EditorPlacedObject::Type::Item:
            data["id"] = obj.id;
            if (obj.staffCharges >= 0) data["staffCharges"] = obj.staffCharges;
            if (obj.potCapacity >= 0) data["potCapacity"] = obj.potCapacity;
            if (obj.stackCount >= 0) data["stackCount"] = obj.stackCount;
            items.push_back(data);
            break;
        case EditorPlacedObject::Type::Trap:
            data["id"] = obj.id;
            traps.push_back(data);
            break;
        case EditorPlacedObject::Type::Shrine:
            shrines.push_back(data);
            break;
        }
    }
    root["items"] = items;
    root["traps"] = traps;
    root["shrines"] = shrines;

    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << root.dump(4);
    return true;
}

bool MapFileManagement::ImportMap(const std::string& path, MapData* map, MapObjectPlacer& placer) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    nlohmann::json root;
    ifs >> root;

    // --- 1. 地形復元 ---
    int w = root["map"]["width"];
    int h = root["map"]["height"];
    map->Reset(w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            map->SetTile(x, y, (TileType)root["map"]["tiles"][y][x]);
        }
    }

    // --- 2. 部屋復元 ---
    map->GetRooms().clear();
    if (root.contains("rooms")) {
        for (const auto& rj : root["rooms"]) {
            Room room({ rj["x"], rj["y"] }, { rj["w"], rj["h"] });
            room.isFixed = rj.value("isFixed", false);
            if (rj.contains("subRects")) {
                for (const auto& srj : rj["subRects"]) {
                    room.AddSubRect({ srj["x"], srj["y"] }, { 1, 1 });
                }
            }
            map->AddRoom(room);
        }
    }

    // --- 3. オブジェクト復元 ---
    placer.ClearAllObjects(map);

    if (root.contains("items")) {
        for (const auto& i : root["items"]) {
            int count = i.value("stackCount", i.value("staffCharges", i.value("potCapacity", 0)));
            placer.PlaceItemByID(map, i["id"], i["x"], i["y"], count);
        }
    }
    if (root.contains("traps")) {
        for (const auto& t : root["traps"]) {
            placer.PlaceTrapByID(map, t["id"], t["x"], t["y"]);
        }
    }
    if (root.contains("shrines")) {
        for (const auto& s : root["shrines"]) {
            placer.PlaceShrine(map, s["x"].get<int>(), s["y"].get<int>());
        }
    }

    return true;
}