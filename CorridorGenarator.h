#pragma once
#include <vector>
#include "Room.h"
#include "MapData.h"
#include "Vector2Int.h"

class CorridorGenerator
{
public:
    static std::vector<std::pair<int, int>> BuildMST(const std::vector<Room>& rooms);

   void ApplyCorridors(MapData* map,
        const std::vector<std::pair<int, int>>& mst,
        bool digExtraCorridors = true,
        int corridorComplexity = 45);
   void DigFloor(int x, int y);
   void DigLineX(int x1, int x2, int y);
   void DigLineY(int y1, int y2, int x);
    void DigCorridorNormal(Room& a, Room& b);
    void ExtendPassage(Vector2Int currentPos, Vector2Int direction, int maxLen);
    bool IsSafeToDig(int x, int y, Vector2Int direction);
    void TryDigExtra(Room& room, int count, int length);

    int RandomRange(int min, int max);
    float RandomFloat();
  

private:
    // 指定したタイルが「通路」として掘っても安全か（2x2の塊にならないか）をチェック
    struct DoorCandidate
    {
        Vector2Int door;
        Vector2Int outside;
        Vector2Int dir;
    };

    std::vector<DoorCandidate> CollectDoorCandidates(const Room& room) const;
    bool DigProtectedCorridor(Room& a, Room& b);
    bool FindProtectedPath(const Vector2Int& start, const Vector2Int& goal, std::vector<Vector2Int>& outPath) const;
    bool IsRoomMarginTile(const Vector2Int& pos) const;
    bool IsConnectingRoomMarginTile(const Vector2Int& pos, const Vector2Int& direction) const;
    bool CanDigProtectedPathTile(const Vector2Int& pos, const Vector2Int& start, const Vector2Int& goal) const;

    MapData* m_Map = nullptr;
};
