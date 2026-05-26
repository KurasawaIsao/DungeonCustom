#pragma once
#include "EffectBase.h"
#include "MapManager.h"
#include "MapData.h"
#include "Room.h"
#include "UnitAI.h"
#include "UnitManager.h"
#include "Enemy.h"
#include "Ally.h"
#include "Player.h"
#include <algorithm>
#include <cstdlib>
#include <vector>

class EffectTargeting
{
public:
    static bool IsValidSkillTargetForUser(Unit* user, Unit* target)
    {
        if (!user || !target || target == user || target->GetHP() <= 0) return false;

        // Enemy skills only affect the player side: Player or Ally.
        if (dynamic_cast<Enemy*>(user)) {
            return dynamic_cast<Player*>(target) || dynamic_cast<Ally*>(target);
        }

        // Player-side skills only affect Enemy units.
        if (dynamic_cast<Player*>(user) || dynamic_cast<Ally*>(user)) {
            return dynamic_cast<Enemy*>(target) != nullptr;
        }

        return true;
    }

	// 効果の対象を収集する関数。効果の対象に含まれるユニットすべてを返す。
    static std::vector<Unit*> CollectUnits(const EffectContext& ctx)
    {
        std::vector<Unit*> result;
        MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
        if (!map) return result;

        const Vector2Int userCenter = ctx.user ? ctx.user->GetGridPos() : ctx.pos;
        const Vector2Int center = ctx.target ? ctx.target->GetGridPos() : ctx.pos;
        const int radius = (std::max)(0, ctx.targetRadius);
        Room* room = map->GetRoomAt(center);
        Room* userRoom = map->GetRoomAt(userCenter);

        auto addUnique = [&](Unit* unit)
        {
            if (!unit) return;
            if (std::find(result.begin(), result.end(), unit) == result.end())
                result.push_back(unit);
        };

        auto isInTarget = [&](Unit* unit)
        {
            if (!unit) return false;
            if (ctx.source == EffectSourceType::Skill && !IsValidSkillTargetForUser(ctx.user, unit)) return false;
            Vector2Int p = unit->GetGridPos();
            int dist = (std::max)(std::abs(p.x - center.x), std::abs(p.y - center.y));

            switch (ctx.targetType)
            {
				//単体
            case EffectTargetType::Single:
                return unit == ctx.target;
            case EffectTargetType::RoomAndVision:
                if (room && map->GetRoomAt(p) == room) return true;
                return dist <= radius && UnitAI::HasLineOfSight(center, p, map);
            case EffectTargetType::TargetAndAround:
                return dist <= 1;
            case EffectTargetType::TargetAndRadius:
                return dist <= radius;
            case EffectTargetType::UserRoom:
            {
                // Room skills affect the user's room plus nearby visible tiles, but never the user.
                if (unit == ctx.user) return false;
                if (userRoom && map->GetRoomAt(p) == userRoom) return true;

                int userDist = (std::max)(std::abs(p.x - userCenter.x), std::abs(p.y - userCenter.y));
                return userDist <= radius && UnitAI::HasLineOfSight(userCenter, p, map);
            }
            case EffectTargetType::UserAround8:
            {
                // 使用者を中心とした周囲8マス。中心マスは対象外。
                int aroundDist = (std::max)(std::abs(p.x - userCenter.x), std::abs(p.y - userCenter.y));
                return unit != ctx.user && aroundDist == 1;
            }
            default:
                return false;
            }
        };

        UnitManager* units = UnitManager::Instance();
        if (isInTarget(units->GetPlayer())) addUnique(units->GetPlayer());
        for (Enemy* enemy : units->GetEnemies()) if (isInTarget(enemy)) addUnique(enemy);
        for (Ally* ally : units->GetAllies()) if (isInTarget(ally)) addUnique(ally);

        return result;
    }

	//これを呼び出すことにより、効果及び対象の情報をもとに、効果の対象を収集し、効果を適用することができる。
    static void ApplyToCollectedTargets(EffectBase& effect, const EffectContext& ctx)
    {
        for (Unit* unit : CollectUnits(ctx))
        {
            EffectContext each = ctx;
            each.target = unit;
            each.pos = unit->GetGridPos();
            effect.Apply(each);
        }
    }
};
