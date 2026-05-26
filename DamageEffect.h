#pragma once
#include "EffectBase.h"
#include "Unit.h"

class DamageEffect : public EffectBase
{
    int m_Power;
public:
    explicit DamageEffect(int power) : m_Power(power) {}
    void Apply(const EffectContext& ctx) override
    {
        if (!ctx.target) return;
        ctx.target->TakeDamage(m_Power, ctx.user);
    }
};
