#pragma once
#include "GameRandom.h"
#include "EffectBase.h"
#include "Enemy.h"
#include "EnemyDatabase.h"
#include "MapManager.h"
#include "MessageLog.h"
#include "Unit.h"
#include "UnitManager.h"
#include "manager.h"
#include "scene.h"
#include <algorithm>
#include <string>
#include <vector>

class SummonEffect : public EffectBase
{
private:
    int m_Count;
    std::string m_EnemyId;

public:
    explicit SummonEffect(int count = 1, const std::string& enemyId = "")
        : m_Count(count), m_EnemyId(enemyId) {}

    void Apply(const EffectContext& ctx) override
    {
        MapManager* mapManager = MapManager::Instance();
        MapData* map = mapManager ? mapManager->GetCurrentMap() : nullptr;
        Scene* scene = Manager::GetScene();
        if (!map || !scene) return;

        Vector2Int center = ctx.target ? ctx.target->GetGridPos() : ctx.pos;
        std::vector<Vector2Int> candidates = {
            { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 },
            { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
        };
        for (int i = (int)candidates.size() - 1; i > 0; --i)
        {
            int j = GameRandom::Range(0, i);
            std::swap(candidates[i], candidates[j]);
        }

        int summoned = 0;
        for (const Vector2Int& offset : candidates)
        {
            if (summoned >= m_Count) break;
            Vector2Int pos = center + offset;
            if (!map->IsTileFree(pos)) continue;

            const EnemyData* data = m_EnemyId.empty()
                ? EnemyDatabase::DrawFromTable(mapManager->GetCurrentFloorData().enemyTableId)
                : EnemyDatabase::Get(m_EnemyId);
            if (!data) continue;

            Enemy* enemy = scene->AddGameObject<Enemy>(1);
            enemy->ApplyData(*data);
            enemy->ClearNap();
            enemy->SetGridPos(pos);
            enemy->ConsumeAllActions();
            UnitManager::Instance()->RegisterEnemy(enemy);
            enemy->StartSummonAppear();
            ++summoned;
        }

        if (summoned > 0)
            MessageLog::Instance().AddMessage(u8"¢Š«ã©‚ªì“®‚µ‚½I");
        else
            MessageLog::Instance().AddMessage(u8"‚µ‚©‚µ‰½‚àŒ»‚ê‚È‚©‚Á‚½B");
    }
};