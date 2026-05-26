#pragma once
#include "EffectBase.h"
#include "Unit.h"
#include "Player.h"

class PoisonEffect : public EffectBase
{
private:
    int m_PlayerStrengthDamage;
    int m_NonPlayerDuration;

public:
    explicit PoisonEffect(int playerStrengthDamage = 1, int nonPlayerDuration = 50)
        : m_PlayerStrengthDamage(playerStrengthDamage), m_NonPlayerDuration(nonPlayerDuration)
    {
    }

    void Apply(const EffectContext& ctx) override
    {
        if (!ctx.target) return;

        if (Player* player = dynamic_cast<Player*>(ctx.target))
        {
            player->ReduceStrength(m_PlayerStrengthDamage);
            return;
        }

        ctx.target->SetStatus(Status::Poison, m_NonPlayerDuration, ctx.user);
    }
};
