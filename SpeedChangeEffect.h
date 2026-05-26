#pragma once
#include "EffectBase.h"
#include "Unit.h"
#include "MessageLog.h"
#include <string>

class SpeedChangeEffect : public EffectBase
{
private:
    TurnSpeed m_Speed;
    std::string m_Message;

public:
    SpeedChangeEffect(TurnSpeed speed, const std::string& message)
        : m_Speed(speed), m_Message(message)
    {
    }

    void Apply(const EffectContext& ctx) override
    {
        if (!ctx.target) return;

        SpeedRank rank = GetSpeedRank(ctx.target);
        rank = ApplySpeedChange(rank);
        ApplySpeedRank(ctx.target, rank);

        MessageLog::Instance().AddMessage(
            ctx.target->GetName() + m_Message + u8"現在: " + GetSpeedRankName(rank) + u8"。");
    }

private:
    enum class SpeedRank
    {
        Slow = 0,
        Normal,
        FastOneAttack,
        FastTwoAttack
    };

    SpeedRank GetSpeedRank(const Unit* unit) const
    {
        if (!unit) return SpeedRank::Normal;

        const TurnSpeed actionSpeed = unit->GetActionSpeed();
        const TurnSpeed moveSpeed = unit->GetMoveSpeed();

        if (actionSpeed == TurnSpeed::Slow || moveSpeed == TurnSpeed::Slow)
        {
            return SpeedRank::Slow;
        }
        if (actionSpeed == TurnSpeed::Fast || actionSpeed == TurnSpeed::Triple ||
            moveSpeed == TurnSpeed::Triple)
        {
            return SpeedRank::FastTwoAttack;
        }
        if (moveSpeed == TurnSpeed::Fast)
        {
            return SpeedRank::FastOneAttack;
        }
        return SpeedRank::Normal;
    }

    SpeedRank ApplySpeedChange(SpeedRank rank) const
    {
        int value = static_cast<int>(rank);
        if (m_Speed == TurnSpeed::Slow)
        {
            --value;
        }
        else if (m_Speed == TurnSpeed::Fast || m_Speed == TurnSpeed::Triple)
        {
            ++value;
        }

        if (value < static_cast<int>(SpeedRank::Slow)) return SpeedRank::Slow;
        if (value > static_cast<int>(SpeedRank::FastTwoAttack)) return SpeedRank::FastTwoAttack;
        return static_cast<SpeedRank>(value);
    }

    void ApplySpeedRank(Unit* unit, SpeedRank rank) const
    {
        switch (rank)
        {
        case SpeedRank::Slow:
            unit->SetActionSpeed(TurnSpeed::Slow);
            unit->SetMoveSpeed(TurnSpeed::Slow);
            break;
        case SpeedRank::FastOneAttack:
            unit->SetActionSpeed(TurnSpeed::Normal);
            unit->SetMoveSpeed(TurnSpeed::Fast);
            break;
        case SpeedRank::FastTwoAttack:
            unit->SetActionSpeed(TurnSpeed::Fast);
            unit->SetMoveSpeed(TurnSpeed::Fast);
            break;
        case SpeedRank::Normal:
        default:
            unit->SetActionSpeed(TurnSpeed::Normal);
            unit->SetMoveSpeed(TurnSpeed::Normal);
            break;
        }
    }

    const char* GetSpeedRankName(SpeedRank rank) const
    {
        switch (rank)
        {
        case SpeedRank::Slow: return u8"鈍足";
        case SpeedRank::FastOneAttack: return u8"倍速1回攻撃";
        case SpeedRank::FastTwoAttack: return u8"倍速2回攻撃";
        case SpeedRank::Normal:
        default: return u8"等速";
        }
    }
};
