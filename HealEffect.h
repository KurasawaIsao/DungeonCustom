#pragma once
#include "EffectBase.h"
#include "player.h"
#include "Unit.h"
class HealEffect : public EffectBase {
    int m_Power;
    int m_MaxPower;
    bool m_MaxHealBlessed;
public:
    HealEffect(int power, int max,bool MaxHeal) : m_Power(power), m_MaxPower(max),m_MaxHealBlessed(MaxHeal) {}

    void Apply(const EffectContext& ctx) override {
        if (!ctx.target) return;

        int currentHeal = m_Power;
        int currentMaxBoost = m_MaxPower;
        bool isFullHeal = false;
        bool isMaxHPUp = false;

        //祝福補正
        if (ctx.rank == EffectRank::Blessed) {
            currentHeal *= 2;
            currentMaxBoost += 1;
            if (m_MaxHealBlessed) isFullHeal = true;
        }

        //  状態による分岐
        if (ctx.target->GetHP() >= ctx.target->GetMaxHP()) {
            isMaxHPUp = true;
        }

        // メッセージと実行 (source ごとに文言を調整)
        std::string name = ctx.target->GetName();

        if (isMaxHPUp) {
 
            ctx.target->SetMaxHP(ctx.target->GetMaxHP() + currentMaxBoost);
            MessageLog::Instance().AddMessage(name + u8"のHPが" + std::to_string(currentMaxBoost) + u8"増えた。");
        }
        else {
            // HP回復
            if (isFullHeal) {
                ctx.target->Heal(ctx.target->GetMaxHP());
                MessageLog::Instance().AddMessage(name + u8"のHPが完全に回復した。");
            }
            else if (currentHeal > 0) {
                ctx.target->Heal(currentHeal);
                MessageLog::Instance().AddMessage(name + u8"のHPが" + std::to_string(currentHeal) + u8"回復した。");
            }
        }
    }
};