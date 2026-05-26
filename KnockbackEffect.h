#pragma once
#include "EffectBase.h"
#include "EffectManager.h"
#include "MapManager.h"
#include "MessageLog.h"
#include "Unit.h"
#include "UnitManager.h"
#include "Enemy.h"
#include <algorithm>
#include <cstdlib>

class KnockbackEffect : public EffectBase
{
private:
    int m_Distance;

    Vector2Int NormalizeDir(Vector2Int dir) const
    {
        if (dir.x != 0) dir.x = dir.x > 0 ? 1 : -1;
        if (dir.y != 0) dir.y = dir.y > 0 ? 1 : -1;
        return dir;
    }

    Vector2Int GetLeftSourceDir(Unit* target) const
    {
        if (!target) return { 0, 0 };

        Vector2Int forward = NormalizeDir(target->GetFacingDir());
        if (forward.x == 0 && forward.y == 0) forward = { 0, 1 };

        Vector2Int left = { -forward.y, forward.x };
        return NormalizeDir(left);
    }

    Vector2Int GetIncomingFromLeftDir(Unit* target) const
    {
        return NormalizeDir(-GetLeftSourceDir(target));
    }

    Vector2Int GetLineStart(Unit* target, MapData* map, const Vector2Int& fromDir) const
    {
        Vector2Int startGrid = target->GetGridPos() + fromDir;
        if (!map) return startGrid;

        startGrid = target->GetGridPos();
        Vector2Int next = startGrid + fromDir;
        while (map->IsInArrayBounds(next.x, next.y))
        {
            startGrid = next;
            next += fromDir;
        }
        return startGrid;
    }

    Unit* FindFirstUnitOnLine(Unit* fallbackTarget, MapData* map, const Vector2Int& startGrid) const
    {
        if (!fallbackTarget) return nullptr;

        Vector2Int targetGrid = fallbackTarget->GetGridPos();
        Vector2Int step = NormalizeDir(targetGrid - startGrid);
        Vector2Int current = startGrid;

        for (int guard = 0; guard < 256; ++guard)
        {
            Unit* unit = UnitManager::Instance()->GetUnitAt(current);
            if (unit && unit->GetHP() > 0) return unit;

            if (current == targetGrid) break;
            current += step;
            if (map && !map->IsInArrayBounds(current.x, current.y)) break;
        }

        return fallbackTarget;
    }

public:
    explicit KnockbackEffect(int dist) : m_Distance(dist) {}

    void Apply(const EffectContext& ctx) override
    {
        Unit* triggerTarget = ctx.target;
        if (!triggerTarget) return;

        MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
        if (!map) return;

        Vector2Int trapFromDir = GetLeftSourceDir(triggerTarget);
        Unit* target = triggerTarget;
        if (ctx.source == EffectSourceType::Trap)
        {
            Vector2Int startGrid = GetLineStart(triggerTarget, map, trapFromDir);
            target = FindFirstUnitOnLine(triggerTarget, map, startGrid);
        }
        if (!target) return;
        if (ctx.source == EffectSourceType::Trap && UnitManager::Instance() && triggerTarget == UnitManager::Instance()->GetPlayer())
            triggerTarget->LookAt(trapFromDir);

        Vector2Int dir = NormalizeDir(ctx.direction);
        if (ctx.source == EffectSourceType::Trap)
            dir = NormalizeDir(-trapFromDir);
        if (dir.x == 0 && dir.y == 0 && ctx.user)
            dir = NormalizeDir(target->GetGridPos() - ctx.user->GetGridPos());
        if (dir.x == 0 && dir.y == 0)
            dir = NormalizeDir(target->GetFacingDir());
        if (dir.x == 0 && dir.y == 0) return;

        Vector2Int current = target->GetGridPos();
        Vector2Int finalGrid = current;
        bool hitWall = false;
        Unit* collisionUnit = nullptr;
        const int distance = (std::max)(1, m_Distance);
        Vector2Int probeGrid = current;
        for (int i = 1; i <= distance; ++i)
        {
            Vector2Int next = probeGrid + dir;
            if (!map->IsInside(next) || !map->IsWalkable(next.x, next.y) || target->IsDiagonalMoveBlocked(probeGrid, dir, map))
            {
                hitWall = true;
                break;
            }
            Unit* hitUnit = UnitManager::Instance()->GetUnitAt(next);
            if (!hitUnit) hitUnit = map->GetUnitAt(next.x, next.y);
            if (hitUnit)
            {
                collisionUnit = hitUnit;
                break;
            }
            finalGrid = next;
            probeGrid = next;
        }

        if (finalGrid == current && !hitWall && !collisionUnit) return;

        const bool showVisual = target->ShouldShowCombatVisual(ctx.user);
        bool prevSkip = Unit::IsSkipMoveAnimation();
        if (!showVisual) Unit::SetSkipMoveAnimation(true);
        int impactDamage = hitWall || collisionUnit ? 5 : 0;
        Unit* impactAttacker = collisionUnit ? collisionUnit : ctx.user;
        int collisionDamage = collisionUnit ? 5 : 0;
        target->StartKnockback(finalGrid, impactDamage, impactAttacker, collisionUnit, collisionDamage);
        if (Enemy* enemy = dynamic_cast<Enemy*>(target)) {
            // ã≠êßà⁄ìÆÇ≈à íuä÷åWÇ™âÛÇÍÇΩìGÇÕÅAàÍìxí«ê’ÇêÿÇ¡ÇƒéüÇÃèÑâÒîªífÇ…ñﬂÇ∑ÅB
            enemy->SuppressHostileRecognitionThisTurn();
        }
        Unit::SetSkipMoveAnimation(prevSkip);
        MessageLog::Instance().AddMessage(target->GetName() + u8"ÇÕêÅÇ´îÚÇŒÇ≥ÇÍÇΩÅB");
    }
};
