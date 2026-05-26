#include "BerserkAI.h"
#include "Ally.h"
#include "Enemy.h"
#include "MapData.h"
#include "Player.h"
#include "Unit.h"
#include "UnitManager.h"
#include <algorithm>

void BerserkAI::Update(Unit& self, MapData* map)
{
    if (!map) {
        self.EndTurn();
        return;
    }

    // まず無差別対象を近い順に集め、隣接している相手がいれば即攻撃する。
    std::vector<Unit*> targets = FindVisibleTargets(self, map);
    if (targets.empty()) {
        m_PatrolAI.Update(self, map);
        return;
    }

    // 近いターゲットから順に、現在位置で攻撃可能か確認する。
    for (Unit* target : targets) {
        if (!target) continue;
        if (IsAttackAdjacent(self, target, map)) {
            Vector2Int dir = target->GetGridPos() - self.GetGridPos();
            self.SetCurrentDir(dir);
            self.LookAt(dir);

            int budgetBefore = self.GetActionBudget();
            self.Attack();
            if (self.GetActionBudget() == budgetBefore) self.EndTurn();
            return;
        }
    }

    // 攻撃できなかった場合は、近いターゲットから順に接近できるか試す。
    for (Unit* target : targets) {
        if (TryMoveTowardTarget(self, target, map)) {
            self.EndTurn();
            return;
        }
    }

    self.EndTurn();
}

Unit* BerserkAI::FindTarget(Unit& self, MapData* map) const
{
    std::vector<Unit*> targets = FindVisibleTargets(self, map);
    return targets.empty() ? nullptr : targets.front();
}

std::vector<Unit*> BerserkAI::FindVisibleTargets(Unit& self, MapData* map) const
{
    std::vector<Unit*> targets;
    if (!map) return targets;

    UnitManager* um = UnitManager::Instance();

    auto consider = [&](Unit* unit) {
        if (!unit || unit == &self || unit->GetHP() <= 0) return;
        if (!UnitAI::CanSee(self, unit, map, m_VisionRange)) return;

        targets.push_back(unit);
    };

    // 勢力を見ずに全ユニットを候補へ入れる。consider 側で自分自身と死亡済みだけ除外する。
    consider(um->GetPlayer());
    for (Ally* ally : um->GetAllies()) consider(ally);
    for (Enemy* enemy : um->GetEnemies())
    {
		if (enemy->IsShopKeeper() && !enemy->IsShopHostile()) continue; // 非敵対的な店主は無視する
        consider(enemy);
    }

    std::sort(targets.begin(), targets.end(), [&](Unit* a, Unit* b) {
        int distA = GetDistance(self.GetGridPos(), a->GetGridPos());
        int distB = GetDistance(self.GetGridPos(), b->GetGridPos());
        return distA < distB;
    });

    return targets;
}

bool BerserkAI::TryMoveTowardTarget(Unit& self, Unit* target, MapData* map)
{
    if (!target || !map) return false;

    static const Vector2Int dirs[8] = {
        {1,0}, {-1,0}, {0,1}, {0,-1},
        {1,1}, {1,-1}, {-1,1}, {-1,-1}
    };

    Vector2Int selfPos = self.GetGridPos();
    Vector2Int targetPos = target->GetGridPos();
    std::vector<Vector2Int> bestPath;

    // ターゲットの周囲8マスをゴール候補にし、最短で隣接できる経路を選ぶ。
    for (const Vector2Int& dir : dirs) {
        Vector2Int goal = targetPos + dir;
        if (!map->IsInBounds(goal) || !map->IsWalkable(goal)) continue;

        Unit* unit = UnitManager::Instance()->GetUnitAt(goal);
        if (unit && unit != &self) continue;

        Vector2Int toTarget = targetPos - goal;
        if (abs(toTarget.x) > 1 || abs(toTarget.y) > 1) continue;
        if (self.IsDiagonalMoveBlocked(goal, toTarget, map)) continue;

        std::vector<Vector2Int> path = BFSPath(self, selfPos, goal, map);
        if (path.empty()) continue;
        if (bestPath.empty() || path.size() < bestPath.size()) {
            bestPath = path;
        }
    }

    if (bestPath.empty()) return false;

    Vector2Int next = bestPath.front();
    Vector2Int moveDir = next - selfPos;
    if (abs(moveDir.x) > 1 || abs(moveDir.y) > 1) return false;
    if (self.IsDiagonalMoveBlocked(selfPos, moveDir, map)) return false;
    if (UnitManager::Instance()->GetUnitAt(next)) return false;

    self.SetCurrentDir(moveDir);
    self.LookAt(moveDir);
    self.RequestMove(next);
    return true;
}

int BerserkAI::GetDistance(const Vector2Int& a, const Vector2Int& b) const
{
    return (std::max)(abs(a.x - b.x), abs(a.y - b.y));
}
