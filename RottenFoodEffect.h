#pragma once
#include "EffectBase.h"
#include "GameRandom.h"
#include "MessageLog.h"
#include "PoisonEffect.h"
#include "Unit.h"

class RottenFoodEffect : public EffectBase
{
public:
    void Apply(const EffectContext& ctx) override
    {
        if (!ctx.target) return;

        switch (GameRandom::Range(0, 2))
        {
        case 0:
            ctx.target->SetActionSpeed(TurnSpeed::Slow);
            ctx.target->SetMoveSpeed(TurnSpeed::Slow);
            MessageLog::Instance().AddMessage(ctx.target->GetName() + u8"‚Í“Ý‘«‚É‚È‚Á‚½B");
            break;
        case 1:
        {
            PoisonEffect poison;
            poison.Apply(ctx);
            break;
        }
        case 2:
        default:
            ctx.target->SetStatus(Status::Confusion, 5);
            break;
        }
    }
};
