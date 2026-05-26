#pragma once
#include "MapData.h"
#include "MapObjectPlacer.h"
#include <string>
#include <vector>
#include "external/json.hpp"

class MapFileManagement {
public:
    // マップデータの書き出し
    static bool ExportMap(
        const std::string& path,
        MapData* map,
        const std::vector<EditorPlacedObject>& placedObjects
    );

    // マップデータの読み込み
    static bool ImportMap(
        const std::string& path,
        MapData* map,
        MapObjectPlacer& placer
    );
};