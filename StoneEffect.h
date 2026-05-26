#pragma once
#include "AttackDamageEffect.h"
#include "EffectBase.h"
#include "MapData.h"
#include "MapManager.h"
#include "MapObject.h"
#include "MessageLog.h"
#include "Trap.h"

class StoneEffect : public EffectBase
{
private:
    AttackDamageEffect m_Damage;

public:
    explicit StoneEffect(float atkRate = 0.8f, int flatBonus = 0)
        : m_Damage(atkRate, flatBonus) {}

    void Apply(const EffectContext& ctx) override
    {
        MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
        if (map)
        {
            MapObject* obj = map->GetObjectAt(ctx.pos);
            if (Trap* trap = dynamic_cast<Trap*>(obj))
            {
                map->RemoveMapObject(trap);
                trap->SetDestroy();
            }
        }

        if (ctx.target)
        {
            m_Damage.Apply(ctx);
        }
    }
};
