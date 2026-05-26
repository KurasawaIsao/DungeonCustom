#include "KeepDistAI.h"
#include "MapData.h"
#include "Unit.h"
#include <algorithm>

void KeepDistAI::Update(Unit& self, MapData* map)
{
    UpdateWithTarget(self, m_Target, map);
}

void KeepDistAI::UpdateWithTarget(Unit& self, Unit* target, MapData* map)
{
    if (!map) {
        self.EndTurn();
        return;
    }

    if (!target) {
        m_PatrolAI.Update(self, map);
        return;
    }

    // 距離が近い/遠い/適正の3段階で、逃走・接近・待機を切り替える。
    int distance = GetDistance(self.GetGridPos(), target->GetGridPos());
    if (distance < m_KeepDistance) {
        m_RunAwayAI.MoveAwayFromTarget(self, target, nullptr, map);
        return;
    }

    if (distance > m_KeepDistance + 1) {
        m_ChaseAI.MoveOnlyWithTarget(self, target, map);
        return;
    }

    self.EndTurn();
}

int KeepDistAI::GetDistance(const Vector2Int& a, const Vector2Int& b) const
{
    return (std::max)(abs(a.x - b.x), abs(a.y - b.y));
}
