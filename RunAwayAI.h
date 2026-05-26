#pragma once
#include "UnitAI.h"
#include "BasicPatrolAI.h"

class MapData;
class Unit;

// RunAwayAI は脅威から距離を取り、可能なら静かな通路側へ逃げるAI。
// safeTarget を渡すと「脅威から離れつつ味方/安全目標に寄る」挙動にも使える。
class RunAwayAI : public UnitAI
{
public:
    // threat は逃げる対象、safeTarget は寄りたい対象。未設定なら UpdateWithTarget 側で直接指定できる。
    void SetTarget(Unit* threat) { m_ThreatTarget = threat; }
    void SetSafeTarget(Unit* safeTarget) { m_SafeTarget = safeTarget; }

    void Update(Unit& self, MapData* map) override;
    void UpdateWithTarget(Unit& self, Unit* threat, MapData* map);
    void MoveAwayFromTarget(Unit& self, Unit* threat, Unit* safeTarget, MapData* map);

private:
    Unit* m_ThreatTarget = nullptr;
    Unit* m_SafeTarget = nullptr;
    BasicPatrolAI m_PatrolAI;

    bool CanStepOn(Unit& self, const Vector2Int& pos, MapData* map) const;
    bool TryStartMove(Unit& self, const Vector2Int& next) const;
};
