#pragma once

class MapData;
class Scene;
struct FloorData;

class SpecialRoomGenerator
{
public:
    static void AssignSpecialRooms(MapData* map, const FloorData& floor);
    static void PlaceSpecialRoomObjects(MapData* map, Scene* scene, const FloorData& floor);
};
