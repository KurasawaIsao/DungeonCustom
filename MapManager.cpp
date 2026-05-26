#include "main.h"
#include "GameRandom.h"
#include "MapManager.h"
#include "player.h"
#include "manager.h"
#include "UnitManager.h"
#include "MiniMapRenderer.h"
#include "Item.h"
#include "Trap.h"
#include "MapRenderer.h"
#include "DungeonData.h"
#include "ItemDataBase.h"
#include "DungeonEndingUI.h"
#include "scene.h"
#include "FloorTransitionUI.h"
#include "TurnManager.h"
#include "EffectManager.h"
#include "Enemy.h"
#include "ShopUI.h"
#include "MessageLog.h"
#include <cstdlib>
#include <ctime>

MapManager* MapManager::instance = nullptr;

namespace
{
    unsigned int MixGenerationSeed(unsigned int value)
    {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    }

    unsigned int ResolveGenerationSeed(const DungeonData& dungeon, const FloorData& floor, int floorIndex)
    {
        const unsigned int baseSeed = static_cast<unsigned int>(dungeon.GetGenerationSeed());
        const unsigned int floorSeed = static_cast<unsigned int>((std::max)(0, floorIndex) + 1) * 0x9e3779b9u;
        const unsigned int floorOffset = static_cast<unsigned int>(floor.generationSeedOffset) * 0x85ebca6bu;
        return MixGenerationSeed(baseSeed ^ floorSeed ^ floorOffset);
    }

    void ReseedRuntimeRandom()
    {
        const unsigned int timeSeed = static_cast<unsigned int>(std::time(nullptr));
        const unsigned int clockSeed = static_cast<unsigned int>(std::clock());
        GameRandom::Seed(timeSeed ^ (clockSeed << 1));
    }

    class ScopedGenerationSeed
    {
    public:
        ScopedGenerationSeed(const DungeonData& dungeon, const FloorData& floor, int floorIndex)
            : m_Enabled(dungeon.UseGenerationSeed())
        {
            if (m_Enabled)
                GameRandom::Seed(ResolveGenerationSeed(dungeon, floor, floorIndex));
        }

        ~ScopedGenerationSeed()
        {
            if (m_Enabled)
                ReseedRuntimeRandom();
        }

    private:
        bool m_Enabled = false;
    };

    bool HasOpenShopTransaction(Player* player)
    {
        return player && (ShopUI::GetUnpaidShopTotal(player) > 0 || ShopUI::HasPendingShopSales(player));
    }

    Enemy* WarpShopKeeperBeside(Player* player)
    {
        if (!player) return nullptr;
        if (TurnManager::Instance()->IsShopTheftMode()) return nullptr;

        MapData* map = MapManager::Instance()->GetCurrentMap();
        if (!map) return nullptr;

        Enemy* keeper = nullptr;
        for (Enemy* enemy : UnitManager::Instance()->GetEnemies())
        {
            if (enemy && enemy->IsShopKeeper()) { keeper = enemy; break; }
        }
        if (!keeper) return nullptr;

        static const Vector2Int dirs[8] = {
            { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
            { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
        };

        for (const Vector2Int& dir : dirs)
        {
            Vector2Int target = player->GetGridPos() + dir;
            if (!map->IsInside(target) || !map->IsWalkable(target)) continue;
            Unit* unit = map->GetUnitAt(target.x, target.y);
            if (unit && unit != keeper) continue;

            EffectManager::PlaySE("Asset\\Sound\\Warp.wav");
            keeper->StartWarp(target);
            keeper->LookAt(player->GetGridPos() - target);
            return keeper;
        }
        return nullptr;
    }

    void ActivateOpeningRoomEvents(MapManager* mapManager)
    {
        Player* player = Manager::GetScene()->GetGameObject<Player>();
        if (!mapManager || !player) return;

        mapManager->AfterUnitMoved(player, true);
    }

    void StartClearEnding(MapManager* mapManager)
    {
        Player* player = Manager::GetScene()->GetGameObject<Player>();
        const std::string playerName = player ? player->GetName() : std::string(u8"プレイヤー");
        const std::string dungeonName = mapManager ? mapManager->GetDungeonData().GetDungeonId() : std::string(u8"ダンジョン");

        if (auto* ending = Manager::GetScene()->GetGameObject<DungeonEndingUI>())
        {
			player->SetVisible(false);
            ending->StartClear(playerName, dungeonName);
        }
    }
}

void MapManager::StartDungeon()
{
    const FloorData& floor = GetCurrentFloorData();

    EffectManager::PlayBGM("Asset\\Sound\\tsumetaidaichi.wav");

    {
        ScopedGenerationSeed generationSeed(m_DungeonData, floor, m_CurrentFloor);
        GenerateNewMap(floor, Manager::GetScene());
        MapGenerator::SetEnemies(floor);
        MapGenerator::SetItems(floor);
    }

    TurnManager::Instance()->ResetDungeonState();
    m_PlayerWasInShop = false;
    ActivateOpeningRoomEvents(this);

    auto* transition = Manager::GetScene()->GetGameObject<FloorTransitionUI>();
    if (!transition) return;

    transition->StartInitTransition(m_DungeonData.GetDungeonId(), m_CurrentFloor + 1);
}

void MapManager::ClearFloor()
{
    auto* renderer = Manager::GetScene()->GetGameObject<MapRenderer>();
    if (renderer) renderer->Clear();

    UnitManager::Instance()->ClearAllEnemies();
    if (GetCurrentMap()) GetCurrentMap()->ClearAllMapObjects();
}

void MapManager::ChangeFloor()
{
    ClearFloor();
    EffectManager::PlayBGM("Asset\\Sound\\tsumetaidaichi.wav");

    if (m_CurrentFloor >= m_DungeonData.GetFloorCount())
    {
        StartClearEnding(this);
        return;
    }

    if (auto* player = Manager::GetScene()->GetGameObject<Player>())
        player->ClearStatus();

	// 現在の階層に応じたマップを生成し、敵やアイテム等を配置させる。
    const FloorData& floor = GetCurrentFloorData();
    {
        ScopedGenerationSeed generationSeed(m_DungeonData, floor, m_CurrentFloor);
        GenerateNewMap(floor, Manager::GetScene());
        MapGenerator::SetEnemies(floor);
        MapGenerator::SetItems(floor);
        MapGenerator::SpawnPlayerInRoom();
        MapGenerator::SetAllies();
    }

    auto* miniMap = Manager::GetScene()->GetGameObject<MiniMapRenderer>();
    if (miniMap) miniMap->ResetMap(GetCurrentMap());

    auto* renderer = Manager::GetScene()->GetGameObject<MapRenderer>();
    if (renderer) renderer->Build(*GetCurrentMap());

    m_IsFloorChanging = false;

	//主にターン数や店の状態をリセットする。階層移動前にやると、移動後の状態がリセットされてしまうので注意。
    TurnManager::Instance()->ResetDungeonState();
    m_PlayerWasInShop = false;
    ActivateOpeningRoomEvents(this);
}

void MapManager::RequestNextFloor()
{
    if (m_IsFloorChanging) return;

    m_IsFloorChanging = true;
    const int nextFloor = m_CurrentFloor + 1;

    if (nextFloor >= m_DungeonData.GetFloorCount())
    {
        StartClearEnding(this);
        return;
    }

    auto* transition = Manager::GetScene()->GetGameObject<FloorTransitionUI>();
    if (!transition)
    {
        m_CurrentFloor = nextFloor;
        ChangeFloor();
        return;
    }

    transition->StartTransition(
        m_DungeonData.GetDungeonId(),
        nextFloor + 1,
        [this, nextFloor]()
        {
            // フェードイン中は旧フロアの視界設定を維持し、黒くなってから新フロアへ切り替える。
            m_CurrentFloor = nextFloor;
            ChangeFloor();
        });
}

void MapManager::RequestBackFloor(int backCount)
{
    if (m_IsFloorChanging) return;
    m_IsFloorChanging = true;

    const int targetFloor = (std::max)(0, m_CurrentFloor - backCount);

    auto* transition = Manager::GetScene()->GetGameObject<FloorTransitionUI>();
    if (!transition)
    {
        m_CurrentFloor = targetFloor;
        ChangeFloor();
        return;
    }

    transition->StartTransition(
        m_DungeonData.GetDungeonId(),
        targetFloor + 1,
        [this, targetFloor]()
        {
            // 戻り階段でも、視界明瞭などの設定はフェードイン完了後に反映する。
            m_CurrentFloor = targetFloor;
            ChangeFloor();
        });
}
void MapManager::AfterUnitMoved(Unit* unit, bool suppressObjectStep)
{
    if (!unit || !GetCurrentMap()) return;

    Vector2Int pos = unit->GetGridPos();
    Player* p = dynamic_cast<Player*>(unit);

    if (p)
    {
        Room* room = GetCurrentMap()->GetRoomAt(pos);
        const bool isInShop = ShopUI::IsPlayerOnShopTile(p);
        if (m_PlayerWasInShop && !isInShop && !TurnManager::Instance()->IsShopTheftMode())
        {

            if (auto* shopUi = Manager::GetScene()->GetGameObject<ShopUI>())
            {
                if(shopUi->GetUnpaidShopTotal(p)<=0 && !shopUi->HasPendingShopSales(p))
                {
                    // 精算が不要な退店でも、店内にいた状態フラグを必ず落として二重表示を防ぐ。
                    m_PlayerWasInShop = false;
                    shopUi->Close();
                    MessageLog::Instance().AddMessage(u8"店から出た。");
                    return;
				}
                Enemy* warpedKeeper = WarpShopKeeperBeside(p);
                shopUi->OpenShopTradeMenu(true, warpedKeeper);
            }
            else if (HasOpenShopTransaction(p))
            {
                ShopUI::StartTheftMode();
            }
        }
        m_PlayerWasInShop = isInShop;

        std::vector<Enemy*> enemies = UnitManager::Instance()->GetEnemies();
        for (Enemy* enemy : enemies)
        {
            if (enemy) enemy->UpdateNap();
        }

        if (room && room->specialType == RoomSpecialType::MonsterHouse && !room->specialMessageShown)
        {
            const int roomIndex = currentMap->GetRoomIndexAt(room->GetCenter());
            for (Enemy* enemy : enemies)
            {
                if (!enemy) continue;
                if (roomIndex >= 0 && currentMap->IsInsideRoom(roomIndex, enemy->GetGridPos().x, enemy->GetGridPos().y))
                    enemy->ClearNap();
            }

            EffectManager::PlayBGM("Asset\\Sound\\maitati.wav");
            MessageLog::Instance().AddMessage(u8"モンスターハウスだ！");
            room->specialMessageShown = true;
        }
    }

    MapObject* obj = GetCurrentMap()->GetObjectAt(pos);
    if (obj && p && !suppressObjectStep)
    {
        obj->OnStepped(p);
    }
}

void MapManager::Update()
{
}

const FloorData& MapManager::GetCurrentFloorData() const
{
    if (m_DungeonData.GetFloorCount() > 0 &&
        m_CurrentFloor >= 0 &&
        m_CurrentFloor < m_DungeonData.GetFloorCount())
    {
        return m_DungeonData.GetFloor(m_CurrentFloor);
    }

    return m_DefaultFloor;
}
