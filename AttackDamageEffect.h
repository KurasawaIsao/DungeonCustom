#pragma once
#include "EffectBase.h"
#include "Unit.h"
#include <algorithm>

class AttackDamageEffect : public EffectBase
{
private:
    float m_AtkRate;
    int m_FlatBonus;

public:
    explicit AttackDamageEffect(float atkRate = 1.0f, int flatBonus = 0)
        : m_AtkRate(atkRate), m_FlatBonus(flatBonus) {}

    void Apply(const EffectContext& ctx) override
    {
        if (!ctx.target) return;

        Unit* attacker = ctx.user ? ctx.user : ctx.target;
        int damage = attacker->CalcDamage(attacker->GetATK(), ctx.target->GetDEF(), m_AtkRate);
        damage = (std::max)(1, damage + m_FlatBonus);
        ctx.target->TakeDamage(damage, ctx.user);
    }
};
