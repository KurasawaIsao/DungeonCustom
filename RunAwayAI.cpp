#include "RunAwayAI.h"
#include "Ally.h"
#include "MapData.h"
#include "Unit.h"
#include "UnitManager.h"
#include <algorithm>
#include <climits>
#include <queue>
#include <vector>

namespace
{
    constexpr int kCorridorSearchRadius = 8;
    constexpr int kQuietUnitDistance = 3;

    int ChebyshevDistance(const Vector2Int& a, const Vector2Int& b)
    {
        return (std::max)(abs(a.x - b.x), abs(a.y - b.y));
    }

    bool IsPlainCorridor(MapData* map, const Vector2Int& pos)
    {
        return map
            && map->IsInBounds(pos)
            && map->GetTile(pos.x, pos.y) == TileType::Corridor
            && map->GetRoomAt(pos) == nullptr;
    }

    int MinDistanceToAny(const Vector2Int& pos, const std::vector<Vector2Int>& positions)
    {
        if (positions.empty()) return kCorridorSearchRadius + kQuietUnitDistance + 1;

        int best = INT_MAX;
        for (const Vector2Int& p : positions) {
            best = (std::min)(best, ChebyshevDistance(pos, p));
        }
        return best;
    }

    bool HasBlockingUnit(Unit& self, const Vector2Int& pos)
    {
        Unit* unit = UnitManager::Instance()->GetUnitAt(pos);
        return unit && unit != &self;
    }

    void CollectAvoidPositions(Unit& self, std::vector<Vector2Int>& hostilePositions, std::vector<Vector2Int>& unitPositions)
    {
        UnitManager* um = UnitManager::Instance();
        if (!um) return;

        if (Player* player = um->GetPlayer()) {
            if (player != &self) {
                hostilePositions.push_back(player->GetGridPos());
                unitPositions.push_back(player->GetGridPos());
            }
        }

        for (Ally* ally : um->GetAllies()) {
            if (!ally || ally == &self) continue;
            hostilePositions.push_back(ally->GetGridPos());
            unitPositions.push_back(ally->GetGridPos());
        }

        for (Enemy* enemy : um->GetEnemies()) {
            if (!enemy || enemy == &self) continue;
            unitPositions.push_back(enemy->GetGridPos());
        }
    }

    int FindNearestCorridorDistance(Unit& self, const Vector2Int& start, MapData* map, const std::vector<Vector2Int>& unitPositions, bool requireQuiet)
    {
        if (!map || !map->IsInBounds(start) || !map->IsWalkable(start)) return -1;

        const int width = map->GetWidth();
        const int height = map->GetHeight();
        std::vector<int> dist(width * height, -1);
        std::queue<Vector2Int> queue;

        auto indexOf = [width](const Vector2Int& p) { return p.y * width + p.x; };
        dist[indexOf(start)] = 0;
        queue.push(start);

        static const Vector2Int dirs[4] = {
            {1,0}, {-1,0}, {0,1}, {0,-1}
        };

        while (!queue.empty()) {
            Vector2Int current = queue.front();
            queue.pop();

            int currentDist = dist[indexOf(current)];
            if (IsPlainCorridor(map, current)
                && (!requireQuiet || MinDistanceToAny(current, unitPositions) >= kQuietUnitDistance)) {
                return currentDist;
            }

            if (currentDist >= kCorridorSearchRadius) continue;

            for (const Vector2Int& dir : dirs) {
                Vector2Int next = current + dir;
                if (!map->IsInBounds(next) || !map->IsWalkable(next)) continue;
                if (HasBlockingUnit(self, next)) continue;

                int idx = indexOf(next);
                if (dist[idx] >= 0) continue;
                dist[idx] = currentDist + 1;
                queue.push(next);
            }
        }

        return -1;
    }
}

void RunAwayAI::Update(Unit& self, MapData* map)
{
    MoveAwayFromTarget(self, m_ThreatTarget, m_SafeTarget, map);
}

void RunAwayAI::UpdateWithTarget(Unit& self, Unit* threat, MapData* map)
{
    MoveAwayFromTarget(self, threat, m_SafeTarget, map);
}

void RunAwayAI::MoveAwayFromTarget(Unit& self, Unit* threat, Unit* safeTarget, MapData* map)
{
    if (!map) {
        self.EndTurn();
        return;
    }
    if (!threat) {
        m_PatrolAI.Update(self, map);
        return;
    }

    Vector2Int current = self.GetGridPos();
    Vector2Int threatPos = threat ? threat->GetGridPos() : current;
    Vector2Int safePos = safeTarget ? safeTarget->GetGridPos() : current;

    static const Vector2Int dirs[8] = {
        {1,0}, {-1,0}, {0,1}, {0,-1},
        {1,1}, {1,-1}, {-1,1}, {-1,-1}
    };

    int currentThreatDist = threat ? ChebyshevDistance(current, threatPos) : 0;
    int currentSafeDist = safeTarget ? ChebyshevDistance(current, safePos) : 0;
    // 脅威・他ユニット・通路までの近さを点数化し、一番安全そうな隣接マスを選ぶ。
    std::vector<Vector2Int> hostilePositions;
    std::vector<Vector2Int> unitPositions;
    CollectAvoidPositions(self, hostilePositions, unitPositions);
    if (threat) hostilePositions.push_back(threatPos);

    int bestScore = INT_MIN;
    Vector2Int best = current;

    for (const auto& dir : dirs) {
        Vector2Int next = current + dir;
        if (!CanStepOn(self, next, map)) continue;
        if (self.IsDiagonalMoveBlocked(current, dir, map)) continue;

        int threatDist = threat ? ChebyshevDistance(next, threatPos) : currentThreatDist;
        int safeDist = safeTarget ? ChebyshevDistance(next, safePos) : currentSafeDist;
        int hostileDist = MinDistanceToAny(next, hostilePositions);
        int unitDist = MinDistanceToAny(next, unitPositions);
        int quietCorridorDist = FindNearestCorridorDistance(self, next, map, unitPositions, true);
        int corridorDist = quietCorridorDist >= 0
            ? quietCorridorDist
            : FindNearestCorridorDistance(self, next, map, unitPositions, false);
        int score = 0;

        // 脅威から離れることを最優先しつつ、混雑しない通路へ寄る移動を高評価にする。
        if (threat) score += (threatDist - currentThreatDist) * 20 + threatDist * 3;
        if (safeTarget) score += (currentSafeDist - safeDist) * 8;
        if (safeTarget && safeDist <= 1) score += 12;
        if (threat && threatDist <= currentThreatDist) score -= 8;
        score += hostileDist * 10;
        score += unitDist * 4;
        if (hostileDist <= 1) score -= 80;
        else if (hostileDist == 2) score -= 35;
        if (unitDist <= 1) score -= 30;
        if (quietCorridorDist >= 0) score += (kCorridorSearchRadius - quietCorridorDist + 1) * 12;
        else if (corridorDist >= 0) score += (kCorridorSearchRadius - corridorDist + 1) * 5;
        if (IsPlainCorridor(map, next)) score += 35;

        if (score > bestScore) {
            bestScore = score;
            best = next;
        }
    }

    if (!(best == current)) {
        TryStartMove(self, best);
    }

    self.EndTurn();
}

bool RunAwayAI::CanStepOn(Unit& self, const Vector2Int& pos, MapData* map) const
{
    if (!map || !map->IsInBounds(pos) || !map->IsWalkable(pos)) return false;

    Unit* unit = UnitManager::Instance()->GetUnitAt(pos);
    return !unit || unit == &self;
}

bool RunAwayAI::TryStartMove(Unit& self, const Vector2Int& next) const
{
    if (UnitManager::Instance()->GetUnitAt(next)) return false;

    Vector2Int moveDir = next - self.GetGridPos();
    self.SetCurrentDir(moveDir);
    self.RequestMove(next);
    return true;
}
