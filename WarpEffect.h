#pragma once
#include "GameRandom.h"
#include "EffectBase.h"
#include "Unit.h"
#include "MessageLog.h"
#include "player.h"
#include "MapManager.h"
#include "EffectManager.h"
class WarpEffect : public EffectBase
{
public:
    void Apply(const EffectContext& ctx) override
    {
        if (!ctx.target) return;
        MapData* map = MapManager::Instance()->GetCurrentMap();
        auto& rooms = map->GetRooms();
        if (rooms.empty()) return;

        int currentRoomIdx = map->GetRoomIndexAt(ctx.target->GetGridPos());

        Vector2Int newPos;
        bool found = false;

        for (int i = 0; i < 100; i++) {
            int roomIdx = GameRandom::Index(rooms.size());


            if (rooms.size() > 1 && roomIdx == currentRoomIdx) {
                continue;
            }

            Room& room = rooms[roomIdx];
            newPos = room.GetRoomInsideRandam();


            if (map->IsInside(newPos) && map->IsTileFree(newPos) && map->GetUnitAt(newPos.x, newPos.y) == nullptr) {
                found = true;
                break;
            }
        }

        if (found) {
            const bool showVisual = ctx.target->ShouldShowCombatVisual(ctx.user);
            if (showVisual) EffectManager::PlaySE("Asset\\Sound\\Warp.wav");
            bool prevSkip = Unit::IsSkipMoveAnimation();
            if (!showVisual) Unit::SetSkipMoveAnimation(true);
            ctx.target->StartWarp(newPos);
            Unit::SetSkipMoveAnimation(prevSkip);
            MessageLog::Instance().AddMessage(ctx.target->GetName() + u8"‚Íƒ[ƒv‚µ‚½B");
        }
    }
};