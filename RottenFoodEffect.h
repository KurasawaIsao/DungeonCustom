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
        {
            ctx.target->SetActionSpeed(TurnSpeed::Slow);
            ctx.target->SetMoveSpeed(TurnSpeed::Slow);
            MessageLog::Instance().AddMessage(ctx.target->GetName() + u8"は鈍足になった。");

            Player* player = dynamic_cast<Player*>(ctx.target);

            if (player)
            {
                // プレイヤーの鈍足は10ターンで解除し、満腹度も回復させる。
                player->RefreshTemporaryTurnSpeed(10);
                player->AddFullness(30);
            }
            else if (ctx.target)
            {
                ctx.target->ConstantDamage(2);
            }
            break;
        }
        case 1:
        {
            PoisonEffect poison;
            poison.Apply(ctx);

            Player* player = dynamic_cast<Player*>(ctx.target);

            if (player)
            {
                // Player だった場合のみ、AddFullness を呼ぶ
                player->AddFullness(30);
            }
            else if (ctx.target)
            {
                ctx.target->ConstantDamage(2);
            }

            break;
        }
        case 2:
        default:
        {
            ctx.target->SetStatus(Status::Confusion, 5);
            Player* player = dynamic_cast<Player*>(ctx.target);

            if (player)
            {
                // Player だった場合のみ、AddFullness を呼ぶ
                player->AddFullness(30);
            }
            else if (ctx.target)
            {
                ctx.target->ConstantDamage(2);
            }
            break;
        }
        }
    }
};
