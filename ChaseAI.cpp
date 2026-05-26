#include "ChaseAI.h"
#include "UnitManager.h"
#include "MapData.h"
#include "MapManager.h"
#include "Ally.h"
#include "Player.h"
#include <algorithm>
#include <climits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace
{
    int EncodePos(const Vector2Int& p, int width)
    {
        return p.y * width + p.x;
    }

    int ChebyshevDistance(const Vector2Int& a, const Vector2Int& b)
    {
        return (std::max)(abs(a.x - b.x), abs(a.y - b.y));
    }

    int ManhattanDistance(const Vector2Int& a, const Vector2Int& b)
    {
        return abs(a.x - b.x) + abs(a.y - b.y);
    }

    bool IsEnemy(Unit& unit)
    {
        return dynamic_cast<Enemy*>(&unit) != nullptr;
    }

    Vector2Int NormalizeDir(const Vector2Int& dir)
    {
        return {
            (dir.x > 0) ? 1 : ((dir.x < 0) ? -1 : 0),
            (dir.y > 0) ? 1 : ((dir.y < 0) ? -1 : 0)
        };
    }

    Vector2Int CardinalDir(const Vector2Int& dir)
    {
        Vector2Int normalized = NormalizeDir(dir);
        if (normalized.x == 0 && normalized.y == 0) return normalized;
        if (abs(dir.x) >= abs(dir.y)) return { normalized.x, 0 };
        return { 0, normalized.y };
    }

}

void ChaseAI::Update(Unit& self, MapData* map)
{
    Unit* target = nullptr;

    // 自分の勢力に応じて標準ターゲットを決める。特殊AIは UpdateWithTarget で直接ターゲットを渡す。
    if (IsEnemy(self)) {
        target = UnitManager::Instance()->GetNearestHostileToEnemy(self.GetGridPos());
    }
    else if (dynamic_cast<Ally*>(&self)) {
        target = UnitManager::Instance()->GetPlayer();
    }

    UpdateWithTarget(self, target, map);
}

void ChaseAI::UpdateWithTarget(Unit& self, Unit* target, MapData* map)
{
    if (!target || !map) { self.EndTurn(); return; }

    // 隣接していても、味方や追従対象なら攻撃せず待機する。
    bool isTargetHostile = false;
    if (dynamic_cast<Ally*>(&self) && dynamic_cast<Enemy*>(target)) isTargetHostile = true;
    if (IsEnemy(self) && (dynamic_cast<Player*>(target) || dynamic_cast<Ally*>(target))) isTargetHostile = true;

    bool adjacentForSelf = IsAdjacent(self, target);
    if (adjacentForSelf) {
        if (isTargetHostile) {
            if (!IsAttackAdjacent(self, target, map)) {
                self.EndTurn();
                return;
            }
            if (ExecuteSkill(self, target)) {
                self.EndTurn();
                return;
            }
            AttackTarget(self, *target);
            return;
        }
        self.EndTurn();
        return;
    }

    MoveOnlyWithTarget(self, target, map);
}

void ChaseAI::MoveOnlyWithTarget(Unit& self, Unit* target, MapData* map)
{
    if (!target || !map) { self.EndTurn(); return; }
    if (IsAdjacent(self, target)) {
        self.EndTurn();
        return;
    }

    Vector2Int targetPos = target->GetGridPos();
    Vector2Int selfPos = self.GetGridPos();
    std::vector<Vector2Int> goals = BuildAdjacentGoals(self, target, map);
    if (goals.empty()) {
        path.clear();
        self.EndTurn();
        return;
    }

    bool selfMovedOutsidePath = (m_LastSelfPos.x != -999 && selfPos != m_LastSelfPos);
    bool nextStepInvalid = (!path.empty() && ChebyshevDistance(path.front(), selfPos) != 1);
    // ターゲット位置・自分の位置・次の一歩がずれた時だけ経路を作り直す。
    if (path.empty() || targetPos != m_LastTargetPos || selfMovedOutsidePath || nextStepInvalid) {
        path = BFSPathToAny(self, selfPos, goals, map);
        m_LastTargetPos = targetPos;
        m_LastSelfPos = selfPos;
    }

    if (!path.empty()) {
        Vector2Int next = path.front();
        if (TryStartMove(self, next, map)) {
            path.erase(path.begin());
            m_LastSelfPos = next;
            self.EndTurn();
            return;
        }
        path.clear();
    }

    self.EndTurn();
}

std::vector<Vector2Int> ChaseAI::BuildPlayerFollowGoals(Player* player, bool allowExtendedLine) const
{
    std::vector<Vector2Int> goals;
    if (!player) return goals;

    Vector2Int playerPos = player->GetGridPos();
    Vector2Int moveDir = playerPos - player->GetPreviousGridPos();
    Vector2Int dir = NormalizeDir(moveDir);
    if (dir.x == 0 && dir.y == 0) {
        dir = NormalizeDir(player->GetFacingDir());
    }
    if (dir.x == 0 && dir.y == 0) dir = { 0, 1 };

    Vector2Int rear = { -dir.x, -dir.y };
    Vector2Int front = dir;

    auto addGoal = [&](const Vector2Int& goal) {
        if (std::find(goals.begin(), goals.end(), goal) != goals.end()) return;
        goals.push_back(goal);
    };

    addGoal(playerPos + rear);
    if (allowExtendedLine) {
        addGoal(playerPos + Vector2Int(rear.x * 2, rear.y * 2));
        addGoal(playerPos + Vector2Int(rear.x * 3, rear.y * 3));
    }
    addGoal(playerPos + front);
    if (allowExtendedLine) {
        addGoal(playerPos + Vector2Int(front.x * 2, front.y * 2));
        addGoal(playerPos + Vector2Int(front.x * 3, front.y * 3));
    }
    return goals;
}

std::vector<Vector2Int> ChaseAI::BuildAdjacentGoals(Unit& self, Unit* target, MapData* map) const
{
    std::vector<Vector2Int> goals;
    if (!target || !map) return goals;

    static const Vector2Int dirs[8] = {
        {1,0}, {-1,0}, {0,1}, {0,-1},
        {1,1}, {1,-1}, {-1,1}, {-1,-1}
    };

    Vector2Int targetPos = target->GetGridPos();
    bool preferPlayerRear = dynamic_cast<Player*>(target) != nullptr;

    auto addGoal = [&](const Vector2Int& goal) {
        if (!CanStepOn(self, goal, map)) return;
        if (std::find(goals.begin(), goals.end(), goal) != goals.end()) return;
        goals.push_back(goal);
    };

    if (preferPlayerRear) {
        if (auto* player = dynamic_cast<Player*>(target)) {
            bool allowExtendedLine = dynamic_cast<Ally*>(&self) != nullptr;
            for (const auto& goal : BuildPlayerFollowGoals(player, allowExtendedLine)) {
                addGoal(goal);
            }
        }
    }

    std::vector<Vector2Int> adjacentGoals;
    for (const auto& dir : dirs) {
        Vector2Int goal = targetPos + dir;
        if (!CanStepOn(self, goal, map)) continue;
        adjacentGoals.push_back(goal);
    }

    Vector2Int selfPos = self.GetGridPos();
    std::sort(adjacentGoals.begin(), adjacentGoals.end(), [&](const Vector2Int& a, const Vector2Int& b) {
        int distA = ManhattanDistance(a, selfPos);
        int distB = ManhattanDistance(b, selfPos);
        if (distA != distB) return distA < distB;

        int cardinalA = ManhattanDistance(a, targetPos);
        int cardinalB = ManhattanDistance(b, targetPos);
        return cardinalA < cardinalB;
    });

    for (const auto& goal : adjacentGoals) {
        addGoal(goal);
    }

    return goals;
}

std::vector<Vector2Int> ChaseAI::BFSPathToAny(Unit& self, const Vector2Int& start, const std::vector<Vector2Int>& goals, MapData* map) const
{
    if (!map || goals.empty()) return {};

    std::vector<Vector2Int> activeGoals;
    for (const auto& goal : goals) {
        if (goal == start) continue;
        activeGoals.push_back(goal);
    }
    if (activeGoals.empty()) return {};

    std::queue<Vector2Int> q;
    std::unordered_map<int, Vector2Int> cameFrom;
    q.push(start);
    cameFrom[EncodePos(start, map->GetWidth())] = start;

    static const Vector2Int baseDirs[8] = {
        {1,0}, {-1,0}, {0,1}, {0,-1},
        {1,1}, {1,-1}, {-1,1}, {-1,-1}
    };

    // 複数のゴール候補へ向けてBFSし、最初に到達できた隣接マスまでの経路を復元する。
    while (!q.empty()) {
        Vector2Int cur = q.front();
        q.pop();

        std::vector<Vector2Int> dirs(baseDirs, baseDirs + 8);

        std::sort(dirs.begin(), dirs.end(), [&](const Vector2Int& a, const Vector2Int& b) {
            Vector2Int nextA = cur + a;
            Vector2Int nextB = cur + b;
            int distA = INT_MAX;
            int distB = INT_MAX;
            for (const auto& goal : activeGoals) {
                distA = (std::min)(distA, ManhattanDistance(goal, nextA));
                distB = (std::min)(distB, ManhattanDistance(goal, nextB));
            }
            return distA < distB;
        });

        for (const auto& dir : dirs) {
            Vector2Int next = cur + dir;
            if (!CanStepOn(self, next, map)) continue;
            if (self.IsDiagonalMoveBlocked(cur, dir, map)) continue;

            int code = EncodePos(next, map->GetWidth());
            if (cameFrom.count(code)) continue;

            cameFrom[code] = cur;
            q.push(next);
        }
    }

    bool found = false;
    Vector2Int foundGoal = start;
    for (const auto& goal : activeGoals) {
        int code = EncodePos(goal, map->GetWidth());
        if (cameFrom.count(code)) {
            found = true;
            foundGoal = goal;
            break;
        }
    }

    if (!found) return {};

    std::vector<Vector2Int> result;
    Vector2Int cur = foundGoal;
    while (!(cur == start)) {
        result.push_back(cur);
        cur = cameFrom[EncodePos(cur, map->GetWidth())];
    }
    std::reverse(result.begin(), result.end());
    return result;
}

bool ChaseAI::CanStepOn(Unit& self, const Vector2Int& pos, MapData* map) const
{
    if (!map || !map->IsInBounds(pos) || !map->IsWalkable(pos)) return false;

    Unit* unit = UnitManager::Instance()->GetUnitAt(pos);
    return !unit || unit == &self;
}

bool ChaseAI::TryStartMove(Unit& self, const Vector2Int& next, MapData* map)
{
    if (!map || !map->IsInBounds(next) || !map->IsWalkable(next)) return false;
    if (ChebyshevDistance(next, self.GetGridPos()) != 1) return false;
    if (UnitManager::Instance()->GetUnitAt(next)) return false;

    Vector2Int moveDir = next - self.GetGridPos();
    if (self.IsDiagonalMoveBlocked(self.GetGridPos(), moveDir, map)) return false;

    self.SetCurrentDir(moveDir);
    self.RequestMove(next);
    return true;
}

void ChaseAI::AttackTarget(Unit& self, Unit& target)
{
    Vector2Int dir = target.GetGridPos() - self.GetGridPos();
    self.SetCurrentDir(dir);
    self.LookAt(dir);
    int budgetBefore = self.GetActionBudget();
    self.Attack();
    if (self.GetActionBudget() == budgetBefore) self.EndTurn();
}
