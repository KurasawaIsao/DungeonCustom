#pragma once
#include "DamageEffect.h"
#include "EffectBase.h"
#include "FlyingObject.h"
#include "MapData.h"
#include "MapManager.h"
#include "MessageLog.h"
#include "Unit.h"
#include "UnitManager.h"
#include "manager.h"
#include "scene.h"

class FlyingObjectEffect : public EffectBase
{
private:
    DamageEffect m_Damage;
    std::string m_ModelPath;

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
    explicit FlyingObjectEffect(int damage = 10, const std::string& modelPath = "Asset\\Model\\Items\\Arrow.obj")
        : m_Damage(damage), m_ModelPath(modelPath) {}

    void Apply(const EffectContext& ctx) override
    {
        Unit* triggerTarget = ctx.target;
        Scene* scene = Manager::GetScene();
        if (!triggerTarget) return;

        MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
        Vector2Int fromDir = GetLeftSourceDir(triggerTarget);
        Vector2Int startGrid = GetLineStart(triggerTarget, map, fromDir);
        Unit* impactTarget = FindFirstUnitOnLine(triggerTarget, map, startGrid);
        if (!impactTarget) return;
        if (ctx.source == EffectSourceType::Trap && UnitManager::Instance() && triggerTarget == UnitManager::Instance()->GetPlayer())
            triggerTarget->LookAt(fromDir);

        if (!impactTarget->ShouldShowCombatVisual(ctx.user)) {
            EffectContext hitCtx = ctx;
            hitCtx.target = impactTarget;
            hitCtx.pos = impactTarget->GetGridPos();
            hitCtx.direction = NormalizeDir(impactTarget->GetGridPos() - startGrid);
            m_Damage.Apply(hitCtx);
            MessageLog::Instance().AddMessage(u8"”ò‚Ñ“¹‹ï‚ª”ò‚ñ‚Å‚«‚½I");
            return;
        }
        if (!scene) return;

        FlyingObject* bullet = scene->AddGameObject<FlyingObject>(1);
        bullet->FireEffectToTarget(
            m_ModelPath,
            &m_Damage,
            ctx.user,
            ctx.source,
            Vector3(static_cast<float>(startGrid.x * TILE_DISTANCE), triggerTarget->GetPosition().y + 0.5f, static_cast<float>(startGrid.y * TILE_DISTANCE)),
            startGrid,
            impactTarget->GetGridPos(),
            4.8f);

        MessageLog::Instance().AddMessage(u8"”ò‚Ñ“¹‹ï‚ª”ò‚ñ‚Å‚«‚½I");
    }
};
