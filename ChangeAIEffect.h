#pragma once
#include "Ally.h"
#include "EffectBase.h"
#include "Enemy.h"
#include "MessageLog.h"

class ChangeAIEffect : public EffectBase
{
private:
    EnemyAIType m_EnemyAI;
    AllyAIMode m_AllyAI;

public:
    explicit ChangeAIEffect(EnemyAIType enemyAI = EnemyAIType::RunAway, AllyAIMode allyAI = AllyAIMode::Retreat)
        : m_EnemyAI(enemyAI), m_AllyAI(allyAI) {}

    void Apply(const EffectContext& ctx) override
    {
        if (!ctx.target) return;

        if (Ally* ally = dynamic_cast<Ally*>(ctx.target))
        {
            ally->SetAIMode(m_AllyAI);
            return;
        }

        if (Enemy* enemy = dynamic_cast<Enemy*>(ctx.target))
        {
            enemy->ChangeAI(m_EnemyAI);
            return;
        }

        MessageLog::Instance().AddMessage(ctx.target->GetName() + " was not affected.");
    }
};
