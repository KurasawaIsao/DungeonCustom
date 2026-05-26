#pragma once
#include "UnitAI.h"
#include "BasicPatrolAI.h"
#include "ChaseAI.h"
#include "RunAwayAI.h"
#include <algorithm>

// KeepDistAI は target との距離を一定に保つAI。
// 近すぎる時は RunAwayAI、遠すぎる時は ChaseAI、ちょうどよい時は待機という合成AIになっている。
class KeepDistAI : public UnitAI
{
public:
    explicit KeepDistAI(int keepDistance = 3) : m_KeepDistance(keepDistance) {}

    void SetTarget(Unit* target) { m_Target = target; }
    void SetKeepDistance(int distance) { m_KeepDistance = (std::max)(1, distance); }
    int GetKeepDistance() const { return m_KeepDistance; }

    void Update(Unit& self, MapData* map) override;
    void UpdateWithTarget(Unit& self, Unit* target, MapData* map);

private:
    int GetDistance(const Vector2Int& a, const Vector2Int& b) const;

    Unit* m_Target = nullptr;
    int m_KeepDistance = 3;
    BasicPatrolAI m_PatrolAI;
    ChaseAI m_ChaseAI;
    RunAwayAI m_RunAwayAI;
};
