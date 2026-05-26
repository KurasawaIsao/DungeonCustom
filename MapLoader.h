#pragma once
#include <memory>
#include "MapData.h"

#include "scene.h"


class MapLoader
{
public:
    static std::unique_ptr<MapData>
        LoadFromFile(const std::string& path, Scene* scene);
    static std::unique_ptr<MapData> LoadDataOnly(
        const std::string& path);
    static void InsertFixedRoom(MapData* targetMap, const std::string& path, Vector2Int offset, Scene* scene);

};
