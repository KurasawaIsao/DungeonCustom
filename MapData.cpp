#include "GameRandom.h"
#include "MapData.h"
#include "manager.h"
#include "scene.h"
#include "Item.h"
#include "Trap.h"
#include "Unit.h"
#include "UnitManager.h"
#include "Shrine.h"
#include "MessageLog.h"
#include <algorithm>

namespace
{
    bool IsRoomRectValidForMap(const Room& room, int width, int height)
    {
        const Vector2Int pos = room.GetPosition();
        const Vector2Int size = room.GetSize();
        if (width <= 0 || height <= 0) return false;
        if (size.x <= 0 || size.y <= 0) return false;
        if (pos.x < 0 || pos.y < 0) return false;
        if (pos.x >= width || pos.y >= height) return false;
        if (size.x > width || size.y > height) return false;
        if (pos.x > width - size.x || pos.y > height - size.y) return false;
        return true;
    }
}
void MapData::AddRoom(const Room& room)
{
    if (!IsRoomRectValidForMap(room, m_Width, m_Height)) return;

    m_Rooms.push_back(room);
}

void MapData::AddRooms(const std::vector<Room>& rooms)
{
    m_Rooms.clear();
    for (const Room& room : rooms)
    {
        AddRoom(room);
    }
}

void MapData::SetAllTile(TileType type)
{
    // 全部壁でリセット
    for (int i = 0; i < m_Width * m_Height; i++)
        m_Tiles[i] = type;
}

void MapData::SetAllActive(bool active)
{
    m_ActiveTiles.assign(m_Width * m_Height, active);
}

void MapData::SetActiveRect(int left, int top, int right, int bottom, bool active)
{
    const int minX = (std::max)(0, left);
    const int minY = (std::max)(0, top);
    const int maxX = (std::min)(m_Width - 1, right);
    const int maxY = (std::min)(m_Height - 1, bottom);

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            m_ActiveTiles[y * m_Width + x] = active;
        }
    }
}
void MapData::ApplyRooms()
{
    // 全部壁でリセット
    for (int i = 0; i < m_Width * m_Height; i++)
        m_Tiles[i] = TileType::Wall;

    // 部屋をすべて適用し直す
    for (auto& r : m_Rooms)
        ApplyRoom(r);
}

void MapData::ApplyRoom(const Room& r)
{
    if (!IsRoomRectValidForMap(r, m_Width, m_Height)) return;

    Vector2Int pos = r.GetPosition();
    Vector2Int size = r.GetSize();
    const int endX = (std::min)(m_Width, pos.x + size.x);
    const int endY = (std::min)(m_Height, pos.y + size.y);

    for (int y = pos.y; y < endY; y++)
    {
        for (int x = pos.x; x < endX; x++)
        {
            if (IsInBounds(x, y))
                m_Tiles[y * m_Width + x] = TileType::Floor;
        }
    }
}
Room* MapData::GetRoomAt(const Vector2Int& pos)
{
    if (!IsInArrayBounds(pos.x, pos.y)) return nullptr;

    for (auto& room : m_Rooms)
    {
        if (!IsRoomRectValidForMap(room, m_Width, m_Height)) continue;
        if (room.Contains(pos))
            return &room;
    }
    return nullptr;
}

const Room* MapData::GetRoomAt(const Vector2Int& pos) const
{
    if (!IsInArrayBounds(pos.x, pos.y)) return nullptr;

    for (auto& room : m_Rooms)
    {
        if (!IsRoomRectValidForMap(room, m_Width, m_Height)) continue;
        if (room.Contains(pos))
            return &room;
    }
    return nullptr;
}
int MapData::GetRoomIndexAt(int x, int y) const
{
    if (!IsInArrayBounds(x, y)) return -1;

    for (int i = 0; i < (int)m_Rooms.size(); ++i)
    {
        if (!IsRoomRectValidForMap(m_Rooms[i], m_Width, m_Height)) continue;
        if (m_Rooms[i].Contains({ x, y }))
            return i;
    }
    return -1;
}

bool MapData::FindAnyWalkableTile(const MapData* map, int& outX, int& outY)
{
    if (!map) return false;

    const int w = map->GetWidth();
    const int h = map->GetHeight();

    // ランダム性を持たせたいなら候補を溜める
    std::vector<Vector2Int> candidates;

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (map->GetTile(x, y) == TileType::Floor)
            {
                candidates.push_back({ x, y });
            }
        }
    }

    if (candidates.empty())
        return false;

    const auto& p = candidates[GameRandom::Index(candidates.size())];
    outX = p.x;
    outY = p.y;
    return true;
}
bool MapData::IsEntranceTile(int x, int y) const
{
    if (!IsInside(x, y)) return false;

    if (GetTile(x, y) != TileType::Corridor)
        return false;

    const Room* room = GetRoomAt({ x, y });
    if (!room) return false;

    static const Vector2Int dirs[4] =
    {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
    };

    for (auto& d : dirs)
    {
        int nx = x + d.x;
        int ny = y + d.y;

        if (!IsInside(nx, ny)) continue;

        if (GetTile(nx, ny) == TileType::Corridor)
        {
            if (GetRoomAt({ nx, ny }) == nullptr)
                return true;
        }
    }

    return false;
}
void MapData::AddMapObject(MapObject* obj, int x, int y)
{
    if (!obj) return;
    if (!IsInArrayBounds(x, y))
    {
        obj->SetDestroy();
        return;
    }

    // Cell already occupied.
    int index = y * m_Width + x;
    if (m_GridObjects[index] != nullptr) {
        obj->SetDestroy();
        return;
    }

    obj->SetGridPos({ x, y });

    // Register in the grid.
    m_GridObjects[index] = obj;

    // Track for management.
    m_AllObjects.push_back(obj);
}
void MapData::RemoveMapObject(MapObject* obj)
{
    if (!obj) return;

    // Clear the grid cell if the object is still inside the map.
    Vector2Int pos = obj->GetGridPos();
    if (IsInArrayBounds(pos.x, pos.y))
        m_GridObjects[pos.y * m_Width + pos.x] = nullptr;

    // Remove from management list.
    auto it = std::find(m_AllObjects.begin(), m_AllObjects.end(), obj);
    if (it != m_AllObjects.end()) {
        m_AllObjects.erase(it);
    }
}

MapObject* MapData::GetObjectAt(int x, int y) const {
    if (!IsInside(x, y)) return nullptr;

    return m_GridObjects[y * m_Width + x];
}

MapObject* MapData::GetObjectAt(const Vector2Int& pos) const {
    return GetObjectAt(pos.x, pos.y);
}
void MapData::ClearAllMapObjects()
{
    // 全オブジェクトの実体をシーンから破棄予約
    for (auto* obj : m_AllObjects)
    {
        if (obj)
        {
            obj->SetDestroy();
        }
    }

    m_AllObjects.clear();

    // グリッド配列を nullptr で埋める（地図を空にする）
    std::fill(m_GridObjects.begin(), m_GridObjects.end(), nullptr);
}
void MapData::Deserialize(const nlohmann::json& j)
{
    // サイズ
    int width = j.at("width").get<int>();
    int height = j.at("height").get<int>();

    Reset(width, height);

    // タイル
    const auto& tiles = j.at("tiles");
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int index = y * width + x;
            m_Tiles[index] =
                static_cast<TileType>(tiles[index].get<int>());
        }
    }

    // 部屋（あれば）
    if (j.contains("rooms"))
    {
        m_Rooms.clear();
        for (const auto& jr : j["rooms"])
        {
            Room r;
            r.Deserialize(jr); 
            AddRoom(r);
        }
    }
}

bool MapData::IsRoomBorderTile(int x, int y) const
{
    // 1. そもそも座標がマップ外なら即終了
    if (!IsInside(x, y)) return false;

    // 2. 床タイルでなければ判定しない
    if (GetTile(x, y) != TileType::Floor) return false;

    // 3. 部屋のインデックスを取得
    int roomIndex = GetRoomIndexAt(x, y);
    if (roomIndex < 0) return false;

    // 4. m_Rooms のサイズチェック（安全策）
    if (roomIndex >= (int)m_Rooms.size()) return false;

    static const Vector2Int dirs[4] = {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
    };

    for (const auto& d : dirs)
    {
        int nx = x + d.x;
        int ny = y + d.y;

        // 隣がマップ外ならスキップ
        if (!IsInside(nx, ny)) continue;

        int nextRoom = GetRoomIndexAt(nx, ny);

        // 隣が「別の部屋」または「部屋ではない（廊下 = -1）」場合、境界とみなす
        if (nextRoom != roomIndex)
            return true;
    }

    return false;
}



Vector3 MapData::ToWorldPos(int x, int y)
{
    return Vector3(
        static_cast<float>(x) * 2.0f,
        0.0f,
        static_cast<float>(y) * 2.0f
    );
}
bool MapData::IsWall(const Vector2Int& pos)
{
    TileType t = GetTile(pos.x, pos.y);
    if (t == TileType::Wall)return true;
    return false;
}
//アイテムや罠、敵配置用
bool MapData::IsTileFree(const Vector2Int& pos)
{
    if (!IsInside(pos)) return false;

    // 床・階段以外は配置できない
    TileType t = GetTile(pos.x, pos.y);
    if (t != TileType::Floor && t != TileType::Stair) return false;

    if (GetObjectAt(pos.x,pos.y) != nullptr)return false;

    if (GetUnitAt(pos.x, pos.y) != nullptr) return false;

    return true;
}
Unit* MapData::GetUnitAt(int x, int y)
{
    return static_cast<const MapData*>(this)->GetUnitAt(x, y);
}

Unit* MapData::GetUnitAt(int x, int y) const
{
    if (!IsInside(x, y)) return nullptr;
    return UnitManager::Instance()->GetUnitAt({ x, y });
}