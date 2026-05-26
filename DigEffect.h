#pragma once
#include "EffectBase.h"
#include "MapManager.h"
#include "MapRenderer.h"
#include "MessageLog.h"
#include "Unit.h"
#include "manager.h"
#include "scene.h"

class DigEffect : public EffectBase
{
public:
    void Apply(const EffectContext& ctx) override
    {
        Unit* actor = ctx.user ? ctx.user : ctx.target;
        if (!actor) return;

        MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
        if (!map) return;

        Vector2Int dir = ctx.direction;
        if (dir.x == 0 && dir.y == 0) dir = actor->GetFacingDir();
        if (dir.x == 0 && dir.y == 0) return;

        Vector2Int target = actor->GetGridPos() + dir;
        if (!map->IsInside(target)) return;
        if (map->GetTile(target.x, target.y) != TileType::Wall)
        {
            MessageLog::Instance().AddMessage(u8"そこは掘れない。");
            return;
        }

        map->SetTile(target.x, target.y, TileType::Corridor);
        if (MapRenderer* renderer = Manager::GetScene()->GetGameObject<MapRenderer>())
            renderer->UpdateTile(target.x, target.y, TileType::Corridor);

        MessageLog::Instance().AddMessage(u8"正面の壁を掘った。");
    }
};
