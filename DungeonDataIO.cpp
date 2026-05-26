#include "DungeonDataIO.h"
#include "external/json.hpp"
#include <fstream>
#include "JsonIO.h"
#include "MessageLog.h"
#include "ItemTableDataBase.h"
#include "EnemyTableDatabase.h"
#include "TrapTableDatabase.h"

using json = nlohmann::json;

namespace
{
    void LogDataWarning(const std::string& message)
    {
        MessageLog::Instance().AddMessage("[Data] " + message);
    }

    void ValidateFloorData(const FloorData& floor, int floorIndex)
    {
        const std::string label = "floor " + std::to_string(floorIndex + 1);
        if (floor.width <= 0 || floor.height <= 0)
            LogDataWarning("Invalid map size in " + label);
        if (floor.minRoomCount > floor.maxRoomCount)
            LogDataWarning("minRoomCount is greater than maxRoomCount in " + label);
        if (floor.maxEnemyCount < 0 || floor.maxItemCount < 0)
            LogDataWarning("Negative spawn count in " + label);
        if (!floor.itemTableId.empty() && !ItemTableDatabase::Exists(floor.itemTableId))
            LogDataWarning("Unknown itemTableId in " + label + ": " + floor.itemTableId);
        if (!floor.shopItemTableId.empty() && !ItemTableDatabase::Exists(floor.shopItemTableId))
            LogDataWarning("Unknown shopItemTableId in " + label + ": " + floor.shopItemTableId);
        if (!floor.enemyTableId.empty() && !EnemyTableDatabase::Exists(floor.enemyTableId))
            LogDataWarning("Unknown enemyTableId in " + label + ": " + floor.enemyTableId);
        if (!floor.trapTableId.empty() && !TrapTableDatabase::Exists(floor.trapTableId))
            LogDataWarning("Unknown trapTableId in " + label + ": " + floor.trapTableId);
    }
}

bool DungeonDataIO::LoadFromFile(const std::string& path, DungeonData& outData)
{
    json root;
    if (!JsonIO::LoadJson(path, root)) return false;

    outData.Clear();
    if (root.contains("dungeonId"))
        outData.SetDungeonId(root["dungeonId"].get<std::string>());

    outData.SetUseGenerationSeed(root.value("useGenerationSeed", false));
    outData.SetGenerationSeed(root.value("generationSeed", 0));

    if (root.contains("unidentifiedMode"))
    {
        outData.SetUnidentifiedMode((UnidentifiedMode)root.value("unidentifiedMode", (int)UnidentifiedMode::None));
    }
    else
    {
        const bool assignUnidentifiedNames = root.value("assignUnidentifiedNames", false);
        outData.SetUnidentifiedMode(assignUnidentifiedNames ? UnidentifiedMode::All : UnidentifiedMode::None);
    }
    outData.SetBlessOrCurseEnabled(root.value("blessOrCurseEnabled", true));

    if (!root.contains("floors") || !root["floors"].is_array()) return false;

    for (const auto& f : root["floors"])
    {
        FloorData floor{};
        floor.width = f.value("width", 50);
        floor.height = f.value("height", 50);
        floor.useRandomMapSize = f.value("useRandomMapSize", false);
        floor.useFullyRandomMapSize = f.value("useFullyRandomMapSize", false);
        floor.minWidth = f.value("minWidth", 40);
        floor.maxWidth = f.value("maxWidth", 60);
        floor.minHeight = f.value("minHeight", 40);
        floor.maxHeight = f.value("maxHeight", 60);

        floor.mapSource = (MapSourceType)f.value("mapSource", (int)MapSourceType::Auto);

        floor.itemTableId = f.value("itemTableId", "early");
        floor.enemyTableId = f.value("enemyTableId", "early");
        floor.trapTableId = f.value("trapTableId", "Basic");
        floor.shopItemTableId = f.value("shopItemTableId", floor.itemTableId);
        floor.viewDistance = f.value("viewDistance", 2);
        floor.playerVisionClear = f.value("playerVisionClear", true);
        floor.maxEnemyCount = f.value("maxEnemyCount", 10);
        floor.maxItemCount = f.value("maxItemCount", 10);
        floor.maxTrapCount = f.value("maxTrapCount", 10);
        floor.monsterHouseAppearanceRate = f.value("monsterHouseAppearanceRate", 0);
        floor.shopAppearanceRate = f.value("shopAppearanceRate", 0);
        floor.shopTrapDensity = f.value("shopTrapDensity", 35);
        floor.mapFilePath = f.value("mapFilePath", "");

        floor.useFixedRoom = f.value("useFixedRoom", false);
        floor.spawnAllFixedRooms = f.value("spawnAllFixedRooms", true);
        floor.minRoomCount = f.value("minRoomCount", 3);
        floor.maxRoomCount = f.value("maxRoomCount", 6);
        floor.corridorComplexity = f.value("corridorComplexity", 45);
        floor.generationSeedOffset = f.value("generationSeedOffset", 0);

        floor.layoutWeights.clear();
        if (f.contains("layoutWeights") && f["layoutWeights"].is_array())
        {
            for (const auto& layoutJson : f["layoutWeights"])
            {
                LayoutWeight layout;
                layout.type = (MapLayoutType)layoutJson.value("type", (int)MapLayoutType::Random);
                layout.weight = layoutJson.value("weight", 0);
                floor.layoutWeights.push_back(layout);
            }
        }
        if (floor.layoutWeights.empty())
            floor.layoutWeights.push_back({ MapLayoutType::Random, 100 });


        if (f.contains("fixedRoomPaths") && f["fixedRoomPaths"].is_array()) {
            for (const auto& settingJson : f["fixedRoomPaths"]) {
                FixedRoomSetting setting;
                setting.path = settingJson.value("path", "");
                setting.appearanceRate = settingJson.value("appearanceRate", 100);
                floor.fixedRoomPaths.push_back(setting);
            }
        }

        ValidateFloorData(floor, outData.GetFloorCount());
        outData.AddFloor(floor);
    }
    return true;
}

bool DungeonDataIO::SaveToFile(const std::string& path, const DungeonData& data)
{
    json root;
    root["dungeonId"] = data.GetDungeonId();
    root["useGenerationSeed"] = data.UseGenerationSeed();
    root["generationSeed"] = data.GetGenerationSeed();
    root["unidentifiedMode"] = (int)data.GetUnidentifiedMode();
    root["blessOrCurseEnabled"] = data.IsBlessOrCurseEnabled();
    root["floors"] = json::array();

    for (int i = 0; i < data.GetFloorCount(); ++i)
    {
        const FloorData& f = data.GetFloor(i);
        json jf;
        jf["width"] = f.width;
        jf["height"] = f.height;
        jf["useRandomMapSize"] = f.useRandomMapSize;
        jf["useFullyRandomMapSize"] = f.useFullyRandomMapSize;
        jf["minWidth"] = f.minWidth;
        jf["maxWidth"] = f.maxWidth;
        jf["minHeight"] = f.minHeight;
        jf["maxHeight"] = f.maxHeight;
        jf["mapSource"] = (int)f.mapSource; 
        jf["itemTableId"] = f.itemTableId;
        jf["enemyTableId"] = f.enemyTableId;
        jf["trapTableId"] = f.trapTableId;
        jf["shopItemTableId"] = f.shopItemTableId;
        jf["viewDistance"] = f.viewDistance;
        jf["playerVisionClear"] = f.playerVisionClear;
        jf["maxEnemyCount"] = f.maxEnemyCount;
        jf["maxItemCount"] = f.maxItemCount;
        jf["maxTrapCount"] = f.maxTrapCount;
        jf["monsterHouseAppearanceRate"] = f.monsterHouseAppearanceRate;
        jf["shopAppearanceRate"] = f.shopAppearanceRate;
        jf["shopTrapDensity"] = f.shopTrapDensity;
        jf["mapFilePath"] = f.mapFilePath;

        jf["useFixedRoom"] = f.useFixedRoom;
        jf["spawnAllFixedRooms"] = f.spawnAllFixedRooms;
        jf["minRoomCount"] = f.minRoomCount;
        jf["maxRoomCount"] = f.maxRoomCount;
        jf["corridorComplexity"] = f.corridorComplexity;
        jf["generationSeedOffset"] = f.generationSeedOffset;

        json layoutArray = json::array();
        for (const auto& layout : f.layoutWeights) {
            json l;
            l["type"] = (int)layout.type;
            l["weight"] = layout.weight;
            layoutArray.push_back(l);
        }
        jf["layoutWeights"] = layoutArray;

        json settingsArray = json::array();
        for (const auto& setting : f.fixedRoomPaths) {
            json s;
            s["path"] = setting.path;
            s["appearanceRate"] = setting.appearanceRate;
            settingsArray.push_back(s);
        }
        jf["fixedRoomPaths"] = settingsArray;

        root["floors"].push_back(jf);
    }

    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;

    ofs << root.dump(4);
    return true;
}


