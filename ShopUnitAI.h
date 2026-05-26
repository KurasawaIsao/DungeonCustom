#pragma once
#include "UnitAI.h"
#include "ChaseAI.h"

// ShopUnitAI は店番用AI。通常は店に立ち続け、敵対化した時だけ ChaseAI で追跡する。
// 店のルールと戦闘AIを分けるため、敵対フラグだけをこのクラスで持つ。
class ShopUnitAI : public UnitAI
{
public:
    explicit ShopUnitAI(bool hostile = false) : m_Hostile(hostile) {}
    void Update(Unit& self, MapData* map) override;
    void SetHostile(bool hostile) { m_Hostile = hostile; }
    bool IsHostile() const { return m_Hostile; }

private:
    bool m_Hostile = false;
    ChaseAI m_ChaseAI;
};