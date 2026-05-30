#pragma once
#include "EffectBase.h"
#include "Unit.h"
#include "Player.h"
#include "MessageLog.h"

class CurePoisonEffect : public EffectBase
{
public:
    void Apply(const EffectContext& ctx) override
    {
        if (!ctx.target) return;

        if (Player* player = dynamic_cast<Player*>(ctx.target))
        {
            bool restored = player->RestoreStrengthToMax();
            if (player->GetStatus() == Status::Poison)
            {
                player->ClearStatus();
                restored = true;
            }
            if (!restored)
            {
                MessageLog::Instance().AddMessage(player->GetName() + u8"の毒は消えている。");
            }
            return;
        }

        bool cured = false;
        if (ctx.target->GetStatModifierStage(StatModifierType::Attack) < 0)
        {
            // 毒で下がった攻撃段階を戻す。
            ctx.target->ClearStatModifierStage(StatModifierType::Attack);
            cured = true;
        }
        if (ctx.target->GetStatus() == Status::Poison)
        {
            ctx.target->ClearStatus();
            cured = true;
        }
        if (!cured)
        {
            MessageLog::Instance().AddMessage(ctx.target->GetName() + u8"の毒は消えている。");
        }
    }
};
