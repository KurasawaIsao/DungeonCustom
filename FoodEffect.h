#pragma once
#include "EffectBase.h" 
#include "Player.h"
#include "Unit.h"

class FoodEffect : public EffectBase
{
    int m_Power;

public:
    FoodEffect(int power) : m_Power(power) {}

    void Apply(const EffectContext& ctx) override
    {
        Player* player = dynamic_cast<Player*>(ctx.target);

        if (player)
        {
            // Player ‚¾‚Á‚½ê‡‚Ì‚ÝAAddFullness ‚ðŒÄ‚Ô
            player->AddFullness(m_Power);
        }
        else if (ctx.target)
        {
            ctx.target->ConstantDamage(2);
        }
    }
};