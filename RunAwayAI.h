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

    void Update(Unit& self, MapData* map) override;
    void UpdateWithTarget(Unit& self, Unit* threat, MapData* map);
    void MoveAwayFromTarget(Unit& self, Unit* threat, Unit* safeTarget, MapData* map);

private:
    BasicPatrolAI m_PatrolAI;

    bool CanStepOn(Unit& self, const Vector2Int& pos, MapData* map) const;
    bool TryStartMove(Unit& self, const Vector2Int& next) const;
};
