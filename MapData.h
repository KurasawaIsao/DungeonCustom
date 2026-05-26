#pragma once
#include <vector>
#include "Vector2Int.h"
#include "Room.h"
#include "Vector3.h"
#include <string>

class Unit;
class TileObject;
class Item;
class Shrine;
class Trap;
class MapObject;
#define TILE_DISTANCE (2)

enum class TileType
{
    Wall,
    Floor,
    Stair,
    Corridor
};

class MapData
{
private:
    int m_Width;
    int m_Height;

    bool lockTiles;
    bool lockRooms;

    int m_Floor = 1;

    std::vector<TileType> m_Tiles;
    std::vector<bool> m_ActiveTiles;
    std::vector<Room> m_Rooms;
    std::vector<MapObject*> m_AllObjects;
    std::vector<MapObject*> m_GridObjects;

    static int NormalizeMapSize(int value)
    {
        if (value < 10) return 10;
        if (value > 120) return 120;
        return value;
    }

public:
    MapData(int width, int height)
        : m_Width(NormalizeMapSize(width))
        , m_Height(NormalizeMapSize(height))
        , m_Tiles(m_Width * m_Height, TileType::Wall)
        , m_ActiveTiles(m_Width * m_Height, true)
        , m_GridObjects(m_Width * m_Height, nullptr)
    {}

    void Reset(int newWidth, int newHeight) {
        m_Rooms.clear();

        m_Width = NormalizeMapSize(newWidth);
        m_Height = NormalizeMapSize(newHeight);

        m_Tiles.assign(m_Width * m_Height, TileType::Wall);
        m_ActiveTiles.assign(m_Width * m_Height, true);
        m_GridObjects.assign(m_Width * m_Height, nullptr);
    }

    inline int GetWidth() const { return m_Width; }
    inline int GetHeight() const { return m_Height; }

    bool IsInArrayBounds(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < m_Width && y < m_Height;
    }

    bool IsActiveTile(int x, int y) const
    {
        return IsInArrayBounds(x, y) && m_ActiveTiles[y * m_Width + x];
    }

    void SetAllActive(bool active);
    void SetActiveRect(int left, int top, int right, int bottom, bool active);

    inline TileType GetTile(int x, int y) const
    {
        if (!IsInside(x, y))
            return TileType::Wall;
        return m_Tiles[y * m_Width + x];
    }

    inline void SetTile(int x, int y, TileType type)
    {
        if (!IsInside(x, y))
            return;

        m_Tiles[y * m_Width + x] = type;
    }

    void SetAllTile(TileType type);
    Vector3 ToWorldPos(int x, int y);

    bool IsWalkable(int x, int y) const
    {
        TileType t = GetTile(x, y);
        return (t == TileType::Floor || t == TileType::Stair || t == TileType::Corridor);
    }

    bool IsWalkable(Vector2Int xy) const
    {
        TileType t = GetTile(xy.x, xy.y);
        return (t == TileType::Floor || t == TileType::Stair || t == TileType::Corridor);
    }

    bool IsWalkable(TileType t) const
    {
        return t == TileType::Floor || t == TileType::Corridor;
    }

    void ApplyRooms();
    void ApplyRoom(const Room& r);

    bool IsInsideRoom(int roomIndex, int x, int y) const
    {
        if (roomIndex < 0 || roomIndex >= (int)m_Rooms.size()) return false;
        const Room& r = m_Rooms[roomIndex];
        Vector2Int pos = r.GetPosition();
        Vector2Int size = r.GetSize();

        return x >= pos.x && x < pos.x + size.x &&
            y >= pos.y && y < pos.y + size.y;
    }

    void AddMapObject(MapObject* obj, int x, int y);
    void RemoveMapObject(MapObject* obj);
    MapObject* GetObjectAt(int x, int y) const;
    MapObject* GetObjectAt(const Vector2Int& pos) const;
    void ClearAllMapObjects();

    Unit* GetUnitAt(int x, int y);
    Unit* GetUnitAt(int x, int y) const;

    bool IsWall(const Vector2Int& pos);
    bool IsTileFree(const Vector2Int& pos);

    std::vector<Room>& GetRooms() { return m_Rooms; }
    const std::vector<Room>& GetRooms() const { return m_Rooms; }
    Room* GetRoomAt(const Vector2Int& pos);
    const Room* GetRoomAt(const Vector2Int& pos) const;

    void AddRoom(const Room& room);
    void AddRooms(const std::vector<Room>& rooms);

    int GetRoomIndex(const Room* room) const
    {
        const auto& rooms = GetRooms();
        for (int i = 0; i < (int)rooms.size(); ++i)
        {
            if (&rooms[i] == room)
                return i;
        }
        return -1;
    }

    int GetRoomIndexAt(Vector2Int pos) const
    {
        return GetRoomIndexAt(pos.x, pos.y);
    }

    int GetRoomIndexAt(int x, int y) const;
    bool IsRoomBorderTile(int x, int y) const;

    bool IsInside(int x, int y) const
    {
        return IsActiveTile(x, y);
    }

    bool IsInBounds(int x, int y) const
    {
        return IsActiveTile(x, y);
    }

    bool IsInBounds(const Vector2Int& p) const
    {
        return IsInBounds(p.x, p.y);
    }

    bool IsInside(const Vector2Int& p) const { return IsInside(p.x, p.y); }

    bool IsRoomTile(int x, int y) const
    {
        if (!IsInside(x, y)) return false;
        return GetRoomIndexAt(x, y) != -1;
    }

    bool FindAnyWalkableTile(const MapData* map, int& outX, int& outY);

    bool IsRoomTile(const Vector2Int& p) const { return IsRoomTile(p.x, p.y); }
    bool IsEntranceTile(int x, int y) const;

    void Deserialize(const nlohmann::json& j);
};
