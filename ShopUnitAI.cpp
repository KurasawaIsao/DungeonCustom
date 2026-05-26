#include "ShopUnitAI.h"
#include "Unit.h"
#include "UnitManager.h"
#include "MapData.h"

void ShopUnitAI::Update(Unit& self, MapData* map)
{
    // ”ñ“G‘ÎŽž‚Í“X”Ô‚Æ‚µ‚Ä‘Ò‹@‚µA“D–_‚È‚Ç‚Å“G‘Î‰»‚µ‚½Œã‚¾‚¯’ÇÕAI‚Ö“n‚·B
    if (!m_Hostile)
    {
        self.EndTurn();
        return;
    }

    Unit* target = UnitManager::Instance()->GetNearestHostileToEnemy(self.GetGridPos());
    m_ChaseAI.UpdateWithTarget(self, target, map);
}