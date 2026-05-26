#include "GameRandom.h"
#include "WhimAI.h"
#include "MapData.h"
#include "Unit.h"
#include "UnitManager.h"
#include <algorithm>
#include <cstdlib>
#include <vector>

WhimAI::WhimAI(bool alwaysAttackAdjacent)
    : m_AlwaysAttackAdjacent(alwaysAttackAdjacent)
{
}

void WhimAI::Update(Unit& self, MapData* map)
{
    UpdateWithTarget(self, nullptr, map);
}

void WhimAI::UpdateWithTarget(Unit& self, Unit* target, MapData* map)
{
    if (!map) {
        self.EndTurn();
        return;
    }

    // 半分の確率でランダム移動を優先し、気まぐれな行動に見せる。
    if (GameRandom::Percent(50) && TryRandomMove(self, map)) {
        self.EndTurn();
        return;
    }

    // ランダム移動しなかった時だけ、見えているターゲットへ追跡行動を取る。
    if (target) {
        m_ChaseAI.MoveOnlyWithTarget(self, target, map);
        return;
    }

    m_PatrolAI.Update(self, map);
}

bool WhimAI::ShouldAttackAdjacent() const
{
    return m_AlwaysAttackAdjacent || GameRandom::Percent(50);
}

bool WhimAI::TryRandomMove(Unit& self, MapData* map)
{
    static const Vector2Int dirs[8] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    std::vector<Vector2Int> candidates;
    candidates.reserve(8);
    for (const Vector2Int& dir : dirs) candidates.push_back(dir);
    for (int i = (int)candidates.size() - 1; i > 0; --i) {
        int j = GameRandom::Range(0, i);
        std::swap(candidates[i], candidates[j]);
    }

    Vector2Int current = self.GetGridPos();
    for (const Vector2Int& dir : candidates) {
        Vector2Int next = current + dir;
        if (!map->IsInBounds(next) || !map->IsWalkable(next)) continue;
        if (self.IsDiagonalMoveBlocked(current, dir, map)) continue;
        if (UnitManager::Instance()->GetUnitAt(next)) continue;

        self.SetCurrentDir(dir);
        self.LookAt(dir);
        self.RequestMove(next);
        return true;
    }

    return false;
}
