#include "UnitMovementPlanner.h"
#include "MapData.h"
#include "Player.h"
#include "Unit.h"
#include "UnitAI.h"
#include "UnitManager.h"
#include <algorithm>
#include <cmath>

namespace UnitMovementPlanner
{
    namespace
    {
        const Vector2Int kEightDirs[8] = {
            {1,0}, {-1,0}, {0,1}, {0,-1},
            {1,1}, {1,-1}, {-1,1}, {-1,-1}
        };
    }
    // 座標差を上下左右の4方向へ変換し、プレイヤーの前後ラインを決めるために使用する。
    Vector2Int CardinalDir(const Vector2Int& dir)
    {
        Vector2Int n = dir.normalized();
        if (n.x == 0 && n.y == 0) return n;
        if (abs(dir.x) >= abs(dir.y)) return { n.x, 0 };
        return { 0, n.y };
    }

    // 現在位置から複数の目標候補までの最小チェビシェフ距離を求め、移動候補の評価に使用する。
    int DistanceToGoals(const Vector2Int& pos, const std::vector<Vector2Int>& goals)
    {
        int best = 999999;
        for (const Vector2Int& goal : goals) {
            best = (std::min)(best, Vector2Int::ChebyshevDistance(goal, pos));
        }
        return best;
    }

    // プレイヤーの移動方向または向きを基準に、前方・後方の追従候補マスを作成する。
    std::vector<Vector2Int> BuildPlayerLinePositions(Player* player, int rearCount, int frontCount)
    {
        std::vector<Vector2Int> positions;
        if (!player) return positions;

        Vector2Int playerPos = player->GetGridPos();
        Vector2Int dir = CardinalDir(playerPos - player->GetPreviousGridPos());
        if (dir.x == 0 && dir.y == 0) {
            dir = CardinalDir(player->GetFacingDir());
        }
        if (dir.x == 0 && dir.y == 0) dir = { 0, 1 };

        Vector2Int rear = { -dir.x, -dir.y };
        Vector2Int front = dir;
        for (int i = 1; i <= rearCount; ++i) {
            positions.push_back(playerPos + Vector2Int(rear.x * i, rear.y * i));
        }
        for (int i = 1; i <= frontCount; ++i) {
            positions.push_back(playerPos + Vector2Int(front.x * i, front.y * i));
        }
        return positions;
    }

    // 指定マスがマップ内の通行可能マスで、他ユニットに占有されていないか判定する。
    bool CanStandAt(Unit& self, const Vector2Int& pos, MapData* map)
    {
        if (!map || !map->IsInBounds(pos) || !map->IsWalkable(pos)) return false;
        Unit* unit = UnitManager::Instance()->GetUnitAt(pos);
        return !unit || unit == &self;
    }

    // 隣接マスへの移動条件と斜めの角抜けを確認し、移動可能なら移動を開始する。
    bool TryStartMove(Unit& self, const Vector2Int& next, MapData* map)
    {
        if (next == self.GetGridPos()) return false;
        if (!CanStandAt(self, next, map)) return false;

        Vector2Int dir = next - self.GetGridPos();
        if (dir.Chebyshev(Vector2Int(0, 0)) > 1) return false;
        if (self.IsDiagonalMoveBlocked(self.GetGridPos(), dir, map)) return false;

        self.SetCurrentDir(dir);
        self.RequestMove(next);
        return true;
    }

    // 対象へ攻撃可能な隣接位置を探し、現在位置から一歩だけ近づく。既に隣接済みの場合もtrueを返す。
    bool TryMoveToAdjacentTarget(Unit& self, Unit* target, MapData* map, UnitAI& pathAI)
    {
        if (!target || !map) return false;
        if (pathAI.IsAttackAdjacent(self, target, map)) return true;

        std::vector<Vector2Int> attackGoals;
        std::vector<Vector2Int> cornerGoals;
        Vector2Int targetPos = target->GetGridPos();
        for (const Vector2Int& dir : kEightDirs) {
            Vector2Int goal = targetPos + dir;
            if (!CanStandAt(self, goal, map)) continue;

            Vector2Int toTarget = targetPos - goal;
            bool cornerAdjacent = toTarget.Chebyshev(Vector2Int(0, 0)) == 1 && toTarget.Manhattan(Vector2Int(0, 0)) == 2 && self.IsDiagonalMoveBlocked(goal, toTarget, map);
            if (cornerAdjacent) cornerGoals.push_back(goal);
            else attackGoals.push_back(goal);
        }

        Vector2Int selfPos = self.GetGridPos();
        auto tryMoveToGoals = [&](std::vector<Vector2Int>& goals) -> bool {
            if (goals.empty()) return false;
            for (const Vector2Int& goal : goals) {
                if (goal == selfPos) return true;
            }

            std::sort(goals.begin(), goals.end(), [&](const Vector2Int& a, const Vector2Int& b) {
                int chebyshevA = a.Chebyshev(selfPos);
                int chebyshevB = b.Chebyshev(selfPos);
                if (chebyshevA != chebyshevB) return chebyshevA < chebyshevB;

                // 必要手数が同じ場合は、直交方向へ寄る候補を優先する。
                return a.Manhattan(selfPos) < b.Manhattan(selfPos);
            });

            int currentScore = DistanceToGoals(selfPos, goals);
            int bestScore = currentScore;
            int bestTargetDist = Vector2Int::ChebyshevDistance(targetPos, selfPos);
            int bestApproach = 0;
            Vector2Int bestStep = selfPos;
            for (const Vector2Int& dir : kEightDirs) {
                Vector2Int next = selfPos + dir;
                if (!CanStandAt(self, next, map)) continue;
                if (self.IsDiagonalMoveBlocked(selfPos, dir, map)) continue;

                int score = DistanceToGoals(next, goals);
                int targetDist = Vector2Int::ChebyshevDistance(targetPos, next);
                int approach = targetPos.Manhattan(selfPos) - targetPos.Manhattan(next);
                if (score < bestScore
                    || (score == bestScore && approach > bestApproach)
                    || (score == bestScore && approach == bestApproach && targetDist < bestTargetDist)) {
                    bestScore = score;
                    bestApproach = approach;
                    bestTargetDist = targetDist;
                    bestStep = next;
                }
            }

            return bestStep != selfPos && TryStartMove(self, bestStep, map);
        };

        return tryMoveToGoals(attackGoals) || tryMoveToGoals(cornerGoals);
    }

    // 対象への隣接移動を試した後、移動の成否にかかわらず自分のターンを終了する。
    bool MoveAdjacentAndEndTurn(Unit& self, Unit* target, MapData* map, UnitAI& pathAI)
    {
        TryMoveToAdjacentTarget(self, target, map, pathAI);
        self.EndTurn();
        return true;
    }
    // プレイヤーの前後ラインへの追従を試し、到達できない場合は設定に応じて隣接移動へ切り替えてターンを終了する。
    bool MoveToPlayerLineAndEndTurn(
        Unit& self,
        Player* player,
        MapData* map,
        UnitAI& pathAI,
        int rearCount,
        int frontCount,
        bool fallbackAdjacentWhenNoGoals)
    {
        if (!player || !map) return false;
        if (TryMoveToAdjacentTarget(self, player, map, pathAI)) {
            self.EndTurn();
            return true;
        }

        if (!UnitAI::HasLineOfSight(self.GetGridPos(), player->GetGridPos(), map)) {
            // 見通しがない場合は前後ラインにこだわらず、通常の隣接移動へ戻す。
            return MoveAdjacentAndEndTurn(self, player, map, pathAI);
        }

        // プレイヤーの前後ラインを追従位置として作り、味方がプレイヤーの周囲に自然に並ぶようにする。
        std::vector<Vector2Int> goals = BuildPlayerLinePositions(player, rearCount, frontCount);
        std::vector<Vector2Int> validGoals;
        for (const Vector2Int& goal : goals) {
            if (CanStandAt(self, goal, map)) validGoals.push_back(goal);
        }
        if (validGoals.empty()) {
            if (fallbackAdjacentWhenNoGoals) {
                return MoveAdjacentAndEndTurn(self, player, map, pathAI);
            }
            self.EndTurn();
            return true;
        }

        Vector2Int selfPos = self.GetGridPos();
        for (const Vector2Int& goal : validGoals) {
            if (goal == selfPos) {
                self.EndTurn();
                return true;
            }

            std::vector<Vector2Int> path = pathAI.BFSPath(self, selfPos, goal, map);
            if (path.empty()) continue;
            Vector2Int next = path.front();
            if (TryStartMove(self, next, map)) {
                self.EndTurn();
                return true;
            }
        }

        // BFSで届かない時も、目標ラインに少しでも近づく直交移動を最後に試す。
        static const Vector2Int dirs[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
        int currentScore = DistanceToGoals(selfPos, validGoals);
        int bestScore = currentScore;
        Vector2Int bestStep = selfPos;
        for (const Vector2Int& dir : dirs) {
            Vector2Int next = selfPos + dir;
            if (!CanStandAt(self, next, map)) continue;
            int score = DistanceToGoals(next, validGoals);
            if (score < bestScore) {
                bestScore = score;
                bestStep = next;
            }
        }

        if (bestStep != selfPos && TryStartMove(self, bestStep, map)) {
            self.EndTurn();
            return true;
        }

        self.EndTurn();
        return true;
    }

    // 対象との部屋関係と対象種別に応じて、直接接近またはプレイヤー追従を選択してターンを終了する。
    bool MoveToTargetByAreaAndEndTurn(
        Unit& self,
        Unit* target,
        MapData* map,
        UnitAI& pathAI,
        int rearCount,
        int frontCount,
        bool fallbackAdjacentWhenNoGoals)
    {
        if (!target || !map) return false;

        Room* selfRoom = map->GetRoomAt(self.GetGridPos());
        Room* targetRoom = map->GetRoomAt(target->GetGridPos());
        if (selfRoom && targetRoom && selfRoom == targetRoom) {
            // 同じ部屋内なら位置取りより接敵を優先する。
            return MoveAdjacentAndEndTurn(self, target, map, pathAI);
        }

        if (Player* player = dynamic_cast<Player*>(target)) {
            return MoveToPlayerLineAndEndTurn(self, player, map, pathAI, rearCount, frontCount, fallbackAdjacentWhenNoGoals);
        }

        return MoveAdjacentAndEndTurn(self, target, map, pathAI);
    }
}
