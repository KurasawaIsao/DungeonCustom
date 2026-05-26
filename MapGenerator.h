#pragma once
#include "MapData.h"
#include <memory>
#include <cstdlib>
#include "scene.h"
struct FloorData;
class MapGenerator
{
public:
    static std::unique_ptr<MapData> Generate(int width, int height, Scene* scene);
    static void SpawnPlayerInRoom();
    static void SetAllies();
    static void SetEnemies(const FloorData& floor);
    static void SetEnemiesOne(const FloorData& floor);
    static void SetItems(const FloorData& floor);
};
