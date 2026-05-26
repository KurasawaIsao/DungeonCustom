#include "CorridorGenarator.h"
#include "GameRandom.h"
#include <algorithm>
#include <cstdlib>
#include <queue>
namespace
{
    bool TryGetRoomCenter(const Room& room, Vector2Int& outCenter)
    {
        const Vector2Int pos = room.GetPosition();
        const Vector2Int size = room.GetSize();
        if (size.x <= 0 || size.y <= 0)
            return false;

        outCenter = Vector2Int(pos.x + size.x / 2, pos.y + size.y / 2);
        return true;
    }
}


void CorridorGenerator::ApplyCorridors(MapData* map, const std::vector<std::pair<int, int>>& mst, bool digExtraCorridors, int corridorComplexity)
{
    if (!map) return;

    m_Map = map;
    auto& rooms = map->GetRooms();

    // 1. 最小全域木 (MST) に基づくメイン通路の生成
    for (const auto& pair : mst)
    {
        if (pair.first < 0 || pair.second < 0) continue;
        if (pair.first >= (int)rooms.size() || pair.second >= (int)rooms.size()) continue;
        DigCorridorNormal(rooms[pair.first], rooms[pair.second]);
    }

    corridorComplexity = (std::max)(0, (std::min)(100, corridorComplexity));
    if (!digExtraCorridors || corridorComplexity <= 0)
        return;

    // 2. 追加通路の生成 
    for (auto& room : rooms)
    {
        // 1部屋あたり最大3回まで通路掘削を試行 
        int attempts = RandomRange(1, 10);

        for (int i = 0; i < attempts; ++i)
        {
            float rnd = RandomFloat();

           
            if (rnd < corridorComplexity / 100.0f)
            {
                // パターンB: 他の通路や部屋に繋がる通路 
                TryDigExtra(room, 1, RandomRange(10, 22));
            }
            // 残りの確率は掘削しない
        }
    }
}

// --- 追加の通路掘削の処理 ---

void CorridorGenerator::TryDigExtra(Room& room, int count, int length)
{
    for (int i = 0; i < count; ++i)
    {
        Vector2Int pos = room.GetPosition();
        Vector2Int size = room.GetSize();
        if (size.x < 3 || size.y < 3) continue;

        // 上下左右のどの壁から伸ばすか決定
        int side = RandomRange(0, 3);
        Vector2Int start, dir;

        switch (side)
        {
        case 0: // 上壁
            start = { RandomRange(pos.x + 1, pos.x + size.x - 2), pos.y };
            dir = { 0, -1 }; break;
        case 1: // 下壁
            start = { RandomRange(pos.x + 1, pos.x + size.x - 2), pos.y + size.y - 1 };
            dir = { 0, 1 }; break;
        case 2: // 左壁
            start = { pos.x, RandomRange(pos.y + 1, pos.y + size.y - 2) };
            dir = { -1, 0 }; break;
        case 3: // 右壁
            start = { pos.x + size.x - 1, RandomRange(pos.y + 1, pos.y + size.y - 2) };
            dir = { 1, 0 }; break;
        }

        Vector2Int firstStep = start + dir;
        if (IsSafeToDig(firstStep.x, firstStep.y, dir))
        {
            DigFloor(start.x, start.y); // 出入り口の作成
            ExtendPassage(start, dir, length);
        }
    }
}

void CorridorGenerator::ExtendPassage(Vector2Int currentPos, Vector2Int direction, int maxLen)
{
    for (int i = 0; i < maxLen; ++i)
    {
        Vector2Int next = currentPos + direction;

        if (!m_Map->IsInArrayBounds(next.x, next.y)) break;

        // 部屋の床、または既存の通路に到達したら接続して終了
        if (m_Map->IsRoomTile(next.x, next.y) || m_Map->GetTile(next.x, next.y) == TileType::Corridor)
        {
            DigFloor(next.x, next.y);
            break;
        }

        // 2マス幅（並走）回避のチェック
        if (!IsSafeToDig(next.x, next.y, direction)) break;

        DigFloor(next.x, next.y);
        currentPos = next;

        // 途中で曲がる判定 (30%の確率)
        if (i > 1 && RandomFloat() < 0.3f)
        {
            Vector2Int newDir = (RandomRange(0, 1) == 0)
                ? Vector2Int{ -direction.y, direction.x }
            : Vector2Int{ direction.y, -direction.x };

            // 曲がった先にすぐ接続先があるか先読み
            for (int dist = 1; dist <= 3; ++dist)
            {
                Vector2Int checkPos = Vector2Int(currentPos.x + newDir.x * dist, currentPos.y + newDir.y * dist);
                if (m_Map->IsInArrayBounds(checkPos.x, checkPos.y) && m_Map->GetTile(checkPos.x, checkPos.y) == TileType::Corridor)
                {
                    bool canConnect = true;
                    for (int j = 1; j < dist; ++j)
                    {
                        Vector2Int digPos = Vector2Int(currentPos.x + newDir.x * j, currentPos.y + newDir.y * j);
                        if (!IsSafeToDig(digPos.x, digPos.y, newDir))
                        {
                            canConnect = false;
                            break;
                        }
                    }
                    if (!canConnect) continue;

                    for (int j = 1; j < dist; ++j)
                    {
                        Vector2Int digPos = Vector2Int(currentPos.x + newDir.x * j, currentPos.y + newDir.y * j);
                        DigFloor(digPos.x, digPos.y);
                    }
                    return;
                }
            }
            ExtendPassage(currentPos, newDir, maxLen - i);
            break;
        }
    }
}

// --- 基盤処理 ---

std::vector<std::pair<int, int>> CorridorGenerator::BuildMST(const std::vector<Room>& rooms)
{
    std::vector<std::pair<int, int>> result;

    std::vector<int> roomIndices;
    std::vector<Vector2Int> centers;
    roomIndices.reserve(rooms.size());
    centers.reserve(rooms.size());

    for (int i = 0; i < (int)rooms.size(); ++i)
    {
        Vector2Int center;
        if (!TryGetRoomCenter(rooms[i], center))
            continue;

        roomIndices.push_back(i);
        centers.push_back(center);
    }

    const int n = (int)centers.size();
    if (n <= 1) return result;

    std::vector<bool> used(n, false);
    used[0] = true;

    for (int iter = 0; iter < n - 1; iter++)
    {
        float bestDist = -1.0f;
        int bestA = -1, bestB = -1;

        for (int i = 0; i < n; i++)
        {
            if (!used[i]) continue;
            const Vector2Int ci = centers[i];

            for (int j = 0; j < n; j++)
            {
                if (used[j]) continue;
                const Vector2Int cj = centers[j];

                float dx = (float)(ci.x - cj.x);
                float dy = (float)(ci.y - cj.y);
                float dist = dx * dx + dy * dy;

                if (bestDist < 0 || dist < bestDist)
                {
                    bestDist = dist;
                    bestA = i;
                    bestB = j;
                }
            }
        }
        if (bestA != -1 && bestB != -1)
        {
            used[bestB] = true;
            result.emplace_back(roomIndices[bestA], roomIndices[bestB]);
        }
    }
    return result;
}
std::vector<CorridorGenerator::DoorCandidate> CorridorGenerator::CollectDoorCandidates(const Room& room) const
{
    std::vector<DoorCandidate> candidates;
    if (!m_Map) return candidates;

    const Vector2Int pos = room.GetPosition();
    const Vector2Int size = room.GetSize();
    if (size.x <= 0 || size.y <= 0) return candidates;

    auto addCandidate = [&](const Vector2Int& door, const Vector2Int& dir)
    {
        const Vector2Int outside = door + dir;
        if (!m_Map->IsInArrayBounds(door.x, door.y)) return;
        if (!m_Map->IsInArrayBounds(outside.x, outside.y)) return;
        if (!room.Contains(door)) return;
        if (!m_Map->IsWalkable(door)) return;
        if (m_Map->GetRoomAt(outside) != nullptr) return;

        candidates.push_back({ door, outside, dir });
    };

    const int left = pos.x;
    const int right = pos.x + size.x - 1;
    const int top = pos.y;
    const int bottom = pos.y + size.y - 1;

    for (int x = left + 1; x <= right - 1; ++x)
    {
        addCandidate({ x, top }, { 0, -1 });
        addCandidate({ x, bottom }, { 0, 1 });
    }

    for (int y = top + 1; y <= bottom - 1; ++y)
    {
        addCandidate({ left, y }, { -1, 0 });
        addCandidate({ right, y }, { 1, 0 });
    }

    return candidates;
}

bool CorridorGenerator::DigProtectedCorridor(Room& a, Room& b)
{
    std::vector<DoorCandidate> doorsA = CollectDoorCandidates(a);
    std::vector<DoorCandidate> doorsB = CollectDoorCandidates(b);
    if (doorsA.empty() || doorsB.empty()) return false;

    Vector2Int centerA;
    Vector2Int centerB;
    if (!TryGetRoomCenter(a, centerA) || !TryGetRoomCenter(b, centerB)) return false;

    auto scoreDoor = [](const DoorCandidate& door, const Vector2Int& target)
    {
        return std::abs(door.outside.x - target.x) + std::abs(door.outside.y - target.y);
    };

    std::stable_sort(doorsA.begin(), doorsA.end(), [&](const DoorCandidate& lhs, const DoorCandidate& rhs)
    {
        return scoreDoor(lhs, centerB) < scoreDoor(rhs, centerB);
    });

    std::stable_sort(doorsB.begin(), doorsB.end(), [&](const DoorCandidate& lhs, const DoorCandidate& rhs)
    {
        return scoreDoor(lhs, centerA) < scoreDoor(rhs, centerA);
    });

    const int firstPassA = (std::min)((int)doorsA.size(), 12);
    const int firstPassB = (std::min)((int)doorsB.size(), 12);

    auto tryDoors = [&](int maxA, int maxB) -> bool
    {
        for (int ai = 0; ai < maxA; ++ai)
        {
            for (int bi = 0; bi < maxB; ++bi)
            {
                std::vector<Vector2Int> path;
                if (!FindProtectedPath(doorsA[ai].outside, doorsB[bi].outside, path)) continue;

                DigFloor(doorsA[ai].door.x, doorsA[ai].door.y);
                for (const Vector2Int& p : path)
                    DigFloor(p.x, p.y);
                DigFloor(doorsB[bi].door.x, doorsB[bi].door.y);
                return true;
            }
        }
        return false;
    };

    if (tryDoors(firstPassA, firstPassB)) return true;
    return tryDoors((int)doorsA.size(), (int)doorsB.size());
}

bool CorridorGenerator::FindProtectedPath(const Vector2Int& start, const Vector2Int& goal, std::vector<Vector2Int>& outPath) const
{
    outPath.clear();
    if (!m_Map) return false;
    if (!CanDigProtectedPathTile(start, start, goal)) return false;
    if (!CanDigProtectedPathTile(goal, start, goal)) return false;

    const int width = m_Map->GetWidth();
    const int height = m_Map->GetHeight();
    const int startIndex = start.y * width + start.x;
    const int goalIndex = goal.y * width + goal.x;

    std::vector<int> previous(width * height, -1);
    std::queue<int> open;
    previous[startIndex] = startIndex;
    open.push(startIndex);

    static const Vector2Int dirs[4] =
    {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
    };

    while (!open.empty())
    {
        const int currentIndex = open.front();
        open.pop();
        if (currentIndex == goalIndex) break;

        const Vector2Int current(currentIndex % width, currentIndex / width);
        for (const Vector2Int& dir : dirs)
        {
            const Vector2Int next = current + dir;
            if (!m_Map->IsInArrayBounds(next.x, next.y)) continue;

            const int nextIndex = next.y * width + next.x;
            if (previous[nextIndex] != -1) continue;
            if (!CanDigProtectedPathTile(next, start, goal)) continue;

            previous[nextIndex] = currentIndex;
            open.push(nextIndex);
        }
    }

    if (previous[goalIndex] == -1) return false;

    std::vector<Vector2Int> reversed;
    for (int index = goalIndex; index != startIndex; index = previous[index])
        reversed.push_back({ index % width, index / width });
    reversed.push_back(start);

    outPath.assign(reversed.rbegin(), reversed.rend());
    return true;
}

bool CorridorGenerator::IsRoomMarginTile(const Vector2Int& pos) const
{
    if (!m_Map || !m_Map->IsInArrayBounds(pos.x, pos.y)) return false;
    if (m_Map->GetRoomAt(pos) != nullptr) return false;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            if (x == 0 && y == 0) continue;
            if (m_Map->GetRoomAt({ pos.x + x, pos.y + y }) != nullptr)
                return true;
        }
    }

    return false;
}

bool CorridorGenerator::IsConnectingRoomMarginTile(const Vector2Int& pos, const Vector2Int& direction) const
{
    if (!IsRoomMarginTile(pos)) return false;
    if (direction.x == 0 && direction.y == 0) return false;

    return m_Map->GetRoomAt(pos - direction) != nullptr
        || m_Map->GetRoomAt(pos + direction) != nullptr;
}

bool CorridorGenerator::CanDigProtectedPathTile(const Vector2Int& pos, const Vector2Int& start, const Vector2Int& goal) const
{
    if (!m_Map || !m_Map->IsInArrayBounds(pos.x, pos.y)) return false;
    if (m_Map->GetRoomAt(pos) != nullptr) return false;
    if (pos == start || pos == goal) return true;
    if (m_Map->GetTile(pos.x, pos.y) == TileType::Corridor) return true;

    return !IsRoomMarginTile(pos);
}

void CorridorGenerator::DigCorridorNormal(Room& a, Room& b)
{
    DigProtectedCorridor(a, b);
}

bool CorridorGenerator::IsSafeToDig(int x, int y, Vector2Int direction)
{
    if (!m_Map->IsInArrayBounds(x, y)) return false;
    if (m_Map->GetTile(x, y) == TileType::Corridor) return false;
    const Vector2Int pos(x, y);
    if (IsRoomMarginTile(pos) && !IsConnectingRoomMarginTile(pos, direction)) return false;

    // 通路が2マス幅（太い通路）にならないよう、進行方向の左右を確認
    Vector2Int side1 = { -direction.y,  direction.x };
    Vector2Int side2 = { direction.y, -direction.x };

    if (m_Map->GetTile(x + side1.x, y + side1.y) == TileType::Corridor) return false;
    if (m_Map->GetTile(x + side2.x, y + side2.y) == TileType::Corridor) return false;

    return true;
}

void CorridorGenerator::DigFloor(int x, int y)
{
    if (!m_Map || !m_Map->IsInArrayBounds(x, y))
        return;

    m_Map->SetActiveRect(x - 1, y - 1, x + 1, y + 1, true);
    m_Map->SetTile(x, y, TileType::Corridor);
}
void CorridorGenerator::DigLineX(int x1, int x2, int y) 
{ 
    int s = (x2 > x1) ? 1 : -1;
    for (int x = x1; x != x2 + s; x += s) DigFloor(x, y);
}
void CorridorGenerator::DigLineY(int y1, int y2, int x)
{
    int s = (y2 > y1) ? 1 : -1;
    for (int y = y1; y != y2 + s; y += s) 
        DigFloor(x, y);
}
int CorridorGenerator::RandomRange(int min, int max) 
{ 
    if (max <= min) return min;
    return GameRandom::Range(min, max); 
}
float CorridorGenerator::RandomFloat() {
    return GameRandom::Value(); 
}