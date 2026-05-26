#pragma once
#include "UnitAI.h"
#include "BasicPatrolAI.h"
#include "ChaseAI.h"

// WhimAI はランダム移動を混ぜながら、見えている相手がいれば追いかける気まぐれAI。
// 隣接攻撃も確定/ランダムを切り替えられるため、敵の性格差を少ない分岐で表現できる。
class WhimAI : public UnitAI
{
public:
    explicit WhimAI(bool alwaysAttackAdjacent);

    void Update(Unit& self, MapData* map) override;
    void UpdateWithTarget(Unit& self, Unit* target, MapData* map);
    bool ShouldAttackAdjacent() const;

private:
    bool TryRandomMove(Unit& self, MapData* map);

    bool m_AlwaysAttackAdjacent = true;
    BasicPatrolAI m_PatrolAI;
    ChaseAI m_ChaseAI;
};
