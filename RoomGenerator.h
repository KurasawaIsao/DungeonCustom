#pragma once
#include "DungeonData.h"

class MapData;
class Scene;

class RoomGenerator
{
public:
    static MapLayoutType GenerateRooms(MapData* map, const FloorData& floor, Scene* scene);
    static void ScanRoomEntrances(MapData* map);
};
