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
        // 追う相手がいない場合は距離維持ができないため、通常巡回に戻す。
        m_PatrolAI.Update(self, map);
        return;
    }

    // 距離が近い/遠い/適正の3段階で、逃走・接近・待機を切り替える。
    // 斜め移動も1手なので、距離維持にはチェビシェフ距離を使う。
    int distance = Vector2Int::ChebyshevDistance(self.GetGridPos(), target->GetGridPos());
    if (distance < m_KeepDistance) {
        // 近すぎる時は専用の逃走AIへ任せる。
        m_RunAwayAI.MoveAwayFromTarget(self, target, nullptr, map);
        return;
    }

    if (distance > m_KeepDistance + 1) {
        // 遠すぎる時は攻撃せず、追跡AIの移動部分だけを使って距離を詰める。
        m_ChaseAI.MoveOnlyWithTarget(self, target, map);
        return;
    }

    self.EndTurn();
}

