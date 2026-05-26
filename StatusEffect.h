#pragma once
#include "GameRandom.h"
#include "EffectBase.h"
#include "Unit.h"
#include "MessageLog.h"
#include "player.h"
#include "Enemy.h"

class StatusEffect : public EffectBase {
    Status m_EffectType;
    int m_Duration;

public:
    // duration: 0を指定すると「永続（敵のかなしばり用）」として扱う
    StatusEffect(Status effect, int duration) 
        : m_EffectType(effect), m_Duration(duration) {}

    void Apply(const EffectContext& ctx) override {
        if (!ctx.target) return;

        int finalDuration = m_Duration;
        if (m_EffectType == Status::Sleep)
        {
            Enemy* enemy = dynamic_cast<Enemy*>(ctx.target);
            finalDuration = enemy ? GameRandom::Range(6, 8) : GameRandom::Range(3, 4);
        }

        //祝福でターン増加
        if (ctx.rank == EffectRank::Blessed) {
            if (finalDuration > 0) finalDuration += 5; 
        }
     
        
        // かなしばり状態のみ分岐
        if (m_EffectType == Status::Paralysis)
        {
            Player* player = dynamic_cast<Player*>(ctx.target);
            if (player)
            {
                player->SetStatus(m_EffectType, 50, ctx.user);
            }
            else
            {
                ctx.target->SetStatus(m_EffectType, 0, ctx.user);
            }
            
        }
        else
        {
            ctx.target->SetStatus(m_EffectType, finalDuration, ctx.user);
        }
        
     
    }
};