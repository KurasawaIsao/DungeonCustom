#include "FlyingObject.h" 
#include "modelRenderer.h"
#include "MapManager.h"
#include "manager.h"
#include "scene.h"
#include "Unit.h"
#include "UnitManager.h"
#include "LightManager.h"
#include "ShopUI.h"
#include "Enemy.h"
#include "Player.h"
#include "EffectTargeting.h"
#include "Item.h"
#include "Trap.h"
#include "MessageLog.h"
#include "Time.h"
#include <algorithm>
#include <cmath>

namespace
{
    Vector2Int NormalizeFlightDir(const Vector2Int& dir)
    {
        Vector2Int result(
            (dir.x > 0) ? 1 : (dir.x < 0 ? -1 : 0),
            (dir.y > 0) ? 1 : (dir.y < 0 ? -1 : 0));

        if (result.x == 0 && result.y == 0)
            result = { 0, 1 };

        return result;
    }

    enum class DropCandidateType
    {
        None,
        Free,
        Trap
    };

    struct DropCandidate
    {
        Vector2Int grid;
        DropCandidateType type = DropCandidateType::None;
    };

    // 中心の北側から時計回りに、半径1、半径2の外周を探索する。
    DropCandidate FindClockwiseDropGrid(MapData* map, const Vector2Int& center,
        const std::vector<Vector2Int>& checkedTrapGrids)
    {
        DropCandidate result;
        if (!map) return result;

        for (int radius = 1; radius <= 2; ++radius)
        {
            auto TryGrid = [&](int offsetX, int offsetY)
            {
                const Vector2Int candidate = center + Vector2Int(offsetX, offsetY);
                if (std::find(checkedTrapGrids.begin(), checkedTrapGrids.end(), candidate) != checkedTrapGrids.end())
                    return false;
                if (map->GetUnitAt(candidate.x, candidate.y)) return false;

                MapObject* object = map->GetObjectAt(candidate);
                if (dynamic_cast<Trap*>(object))
                {
                    result.grid = candidate;
                    result.type = DropCandidateType::Trap;
                    return true;
                }
                if (object || !map->IsTileFree(candidate)) return false;

                result.grid = candidate;
                result.type = DropCandidateType::Free;
                return true;
            };

            if (TryGrid(0, -radius)) return result;
            for (int x = 1; x <= radius; ++x) if (TryGrid(x, -radius)) return result;
            for (int y = -radius + 1; y <= radius; ++y) if (TryGrid(radius, y)) return result;
            for (int x = radius - 1; x >= -radius; --x) if (TryGrid(x, radius)) return result;
            for (int y = radius - 1; y >= -radius; --y) if (TryGrid(-radius, y)) return result;
            for (int x = -radius + 1; x < 0; ++x) if (TryGrid(x, -radius)) return result;
        }

        return result;
    }
}
void FlyingObject::Init()
{
    m_Model = new ModelRenderer();
    m_Scale = { 0.5f, 0.5f, 0.5f };
}

void FlyingObject::Update()
{
    if (!m_IsActive) return;

    // 着地先の再探索中は、通常の飛翔処理とは別に更新する。
    if (m_DropResolutionState != DropResolutionState::None)
    {
        UpdateDropResolution();
        return;
    }

    // 矢、石以外の投擲アイテムは、飛翔中に回転させる。
    if (m_RotateItemInFlight && m_FlyingItem->GetData()->type != ItemType::Stone)
    {
        m_ItemSpin += 0.12f;
    }

    // 石だけは従来どおり、水平補間に高さを加えた放物線で飛ばす。
    if (m_UseArcFlight)
    {
        m_FlyT += 0.016f / m_ArcDuration;
        if (m_FlyT >= 1.0f)
        {
            m_FlyT = 1.0f;
            m_Position = m_TargetPos;
            OnHit();
            return;
        }

        m_Position.x = m_StartPos.x + (m_TargetPos.x - m_StartPos.x) * m_FlyT;
        m_Position.z = m_StartPos.z + (m_TargetPos.z - m_StartPos.z) * m_FlyT;
        const float arcHeight = 1.2f;
        m_Position.y = m_StartPos.y + (m_TargetPos.y - m_StartPos.y) * m_FlyT
            - 4.0f * arcHeight * m_FlyT * (m_FlyT - 1.0f);
        return;
    }

    Vector3 toTarget = m_TargetPos - m_Position;
    const float distance = toTarget.LengthSqrt();
    if (distance <= m_Speed || distance <= 0.001f)
    {
        m_Position = m_TargetPos;
        OnHit();
        return;
    }

    toTarget.normalize();
    m_Position += toTarget * m_Speed;
}

void FlyingObject::Draw()
{
    if (!m_IsActive) return;

    Renderer::SetLight(LightManager::Instance().GetLight());
    Renderer::SetCommonShader();

    XMMATRIX scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    const float facingYaw = std::atan2((float)m_Dir.x, (float)m_Dir.y);
    XMMATRIX rot = m_RotateItemInFlight
        ? XMMatrixRotationRollPitchYaw(m_ItemSpin, facingYaw, m_ItemSpin * 0.7f)
        : XMMatrixRotationRollPitchYaw(0.0f, facingYaw, 0.0f);
    XMMATRIX trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(scale * rot * trans);

    m_Model->Draw();
}

void FlyingObject::Fire(const std::string& modelPath, ItemInstance* sourceItem, const Vector3& startPos,
    const Vector2Int& startGrid, const Vector2Int& dir, float speed, int maxRange, Unit* user)
{
    m_SourceItem = sourceItem;
    m_FlyingItem.reset();
    m_RotateItemInFlight = false;
    m_UseArcFlight = false;
    m_ItemSpin = 0.0f;
    m_Scale = { 0.5f, 0.5f, 0.5f };
    m_SourceEffect = nullptr;
    m_User = user;
    m_SourceType = EffectSourceType::Item;
    m_Model->Load(modelPath.c_str());
    SetupFlight(startPos, startGrid, dir, speed, maxRange);
}

void FlyingObject::FireItem(ItemInstance&& item, Unit* user,
    const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& dir, float speed, int maxRange)
{
    const ItemData* itemData = item.GetData();
    if (!itemData) return;

    m_SourceItem = nullptr;
    m_FlyingItem.emplace(std::move(item));
    // 実アイテムは地面表示と同じ大きさにし、矢以外だけ回転させる。
    m_Scale = { 1.0f, 1.0f, 1.0f };
    m_RotateItemInFlight = itemData->type != ItemType::Arrow;
    m_UseArcFlight = itemData->type == ItemType::Stone;
    m_ItemSpin = 0.0f;
    m_SourceEffect = nullptr;
    m_User = user;
    m_SourceType = EffectSourceType::Item;
    Item::LoadModelForType(m_Model, itemData->type);
    SetupFlight(startPos, startGrid, dir, speed, maxRange);
}

void FlyingObject::DropItemFromPot(ItemInstance&& item, const Vector2Int& centerGrid)
{
    const ItemData* itemData = item.GetData();
    if (!itemData)
    {
        SetDestroy();
        return;
    }

    // 壺の中身は中心マスから着地判定を始め、必要なら時計回りに配置先を探す。
    m_SourceItem = nullptr;
    m_FlyingItem.emplace(std::move(item));
    m_SourceEffect = nullptr;
    m_User = nullptr;
    m_SourceType = EffectSourceType::Item;
    m_Scale = { 1.0f, 1.0f, 1.0f };
    m_RotateItemInFlight = itemData->type != ItemType::Arrow;
    m_UseArcFlight = false;
    m_ItemSpin = 0.0f;
    m_HitWall = false;
    m_TargetGrid = centerGrid;
    m_Position = Vector3(static_cast<float>(centerGrid.x * TILE_DISTANCE), 0.5f,
        static_cast<float>(centerGrid.y * TILE_DISTANCE));
    Item::LoadModelForType(m_Model, itemData->type);

    const bool finishedImmediately = DropFlyingItem(MapManager::Instance()->GetCurrentMap());
    if (!finishedImmediately)
    {
        m_IsActive = true;
    }
}

void FlyingObject::FireEffect(const std::string& modelPath, EffectBase* sourceEffect, Unit* user, EffectSourceType sourceType,
    const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& dir, float speed, int maxRange)
{
    m_SourceItem = nullptr;
    m_FlyingItem.reset();
    m_RotateItemInFlight = false;
    m_UseArcFlight = false;
    m_ItemSpin = 0.0f;
    m_Scale = { 0.5f, 0.5f, 0.5f };
    m_SourceEffect = sourceEffect;
    m_User = user;
    m_SourceType = sourceType;
    m_Model->Load(modelPath.c_str());
    SetupFlight(startPos, startGrid, dir, speed, maxRange);
}

void FlyingObject::FireEffectToTarget(const std::string& modelPath, EffectBase* sourceEffect, Unit* user, EffectSourceType sourceType,
    const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& targetGrid, float speed)
{
    m_SourceItem = nullptr;
    m_FlyingItem.reset();
    m_RotateItemInFlight = false;
    m_UseArcFlight = false;
    m_ItemSpin = 0.0f;
    m_Scale = { 0.5f, 0.5f, 0.5f };
    m_SourceEffect = sourceEffect;
    m_User = user;
    m_SourceType = sourceType;
    SetupDirectFlight(modelPath, startPos, startGrid, targetGrid, speed);
}

void FlyingObject::SetupFlight(const Vector3& startPos,
    const Vector2Int& startGrid, const Vector2Int& dir, float speed, int maxRange)
{
    const Vector2Int flightDir = NormalizeFlightDir(dir);

    m_StartPos = startPos;
    m_Position = startPos;
    m_FlyT = 0.0f;
    m_Speed = (std::max)(speed * maxRange * TILE_DISTANCE, 0.01f);
    m_Dir = flightDir;
    m_HitWall = false;

    MapData* map = MapManager::Instance()->GetCurrentMap();
    Vector2Int current = startGrid;
    Vector3 targetPos = startPos;

    for (int i = 0; i < maxRange; i++)
    {
        Vector2Int next = current + flightDir;

        if (!map->IsInside(next) || map->IsWall(next))
        {
            m_HitWall = true;
            // 壁まで飛ぶ表示にし、アイテムを落とす座標は直前の床マスを維持する。
            targetPos = Vector3(next.x * TILE_DISTANCE, startPos.y, next.y * TILE_DISTANCE);
            break;
        }

        current = next;
        targetPos = Vector3(current.x * 2.0f, startPos.y, current.y * 2.0f);

        if (map->GetUnitAt(current.x, current.y))
        {
            break;
        }
    }

    m_TargetGrid = current;
    m_TargetPos = targetPos;
    m_IsActive = true;
}

void FlyingObject::SetupDirectFlight(const std::string& modelPath, const Vector3& startPos,
    const Vector2Int& startGrid, const Vector2Int& targetGrid, float speed)
{
    const Vector2Int flightDir = NormalizeFlightDir(targetGrid - startGrid);

    m_Model->Load(modelPath.c_str());
    m_StartPos = startPos;
    m_Position = startPos;
    m_FlyT = 0.0f;
    m_Speed = (std::max)(speed, 0.01f);
    m_Dir = flightDir;
    m_HitWall = false;
    m_TargetGrid = targetGrid;
    m_TargetPos = Vector3(static_cast<float>(targetGrid.x * TILE_DISTANCE), startPos.y,
        static_cast<float>(targetGrid.y * TILE_DISTANCE));
    m_IsActive = true;
}

void FlyingObject::OnHit()
{
    m_IsActive = false;
    MapData* map = MapManager::Instance()->GetCurrentMap();
    Unit* hitUnit = UnitManager::Instance()->GetUnitAt(m_TargetGrid);
    if (!hitUnit && map)
    {
        hitUnit = map->GetUnitAt(m_TargetGrid.x, m_TargetGrid.y);
    }

    // 実アイテムは ItemInstance::Throw 内で店主への命中を処理する。
    if (!m_FlyingItem)
    {
        if (Enemy* shopkeeper = dynamic_cast<Enemy*>(hitUnit))
        {
            ShopUI::AngerShopKeeper(shopkeeper);
        }
    }

    bool shouldDestroy = true;
    if (m_FlyingItem)
    {
        shouldDestroy = ResolveFlyingItemHit(map, hitUnit);
    }
    else if (hitUnit && m_SourceItem && hitUnit != m_User)
    {
        EffectContext ctx;
        ctx.source = EffectSourceType::Item;
        ctx.user = m_User;
        ctx.target = hitUnit;
        ctx.pos = m_TargetGrid;
        ctx.direction = m_Dir;

        if (m_SourceItem->IsBlessed()) ctx.rank = EffectRank::Blessed;
        else if (m_SourceItem->IsCursed()) ctx.rank = EffectRank::Cursed;
        else ctx.rank = EffectRank::Normal;

        if (m_SourceItem->GetData()->effect) {
            if (ctx.targetType == EffectTargetType::Single) m_SourceItem->GetData()->effect->Apply(ctx);
            else EffectTargeting::ApplyToCollectedTargets(*m_SourceItem->GetData()->effect, ctx);
        }
    }
    else if (hitUnit && m_SourceEffect && hitUnit != m_User)
    {
        EffectContext ctx;
        ctx.source = m_SourceType;
        ctx.user = m_User;
        ctx.target = hitUnit;
        ctx.pos = m_TargetGrid;
        ctx.direction = m_Dir;
        if (ctx.targetType == EffectTargetType::Single) m_SourceEffect->Apply(ctx);
        else EffectTargeting::ApplyToCollectedTargets(*m_SourceEffect, ctx);
    }

    if (shouldDestroy) SetDestroy();
}

bool FlyingObject::ResolveFlyingItemHit(MapData* map, Unit* hitUnit)
{
    if (!m_FlyingItem || !m_FlyingItem->GetData()) return true;

    Player* thrower = dynamic_cast<Player*>(m_User);

    // 発射者以外のユニットに当たった場合は、床へ落とさず命中効果を解決する。
    if (hitUnit && hitUnit != m_User)
    {
        m_FlyingItem->Throw(thrower, hitUnit, m_TargetGrid);
        if (m_FlyingItem->IsPot())
        {
            m_FlyingItem->OnBreak(m_TargetGrid);
        }
        return true;
    }

    // 壁に当たった壺は着地点へ置かず、その場で割って中身を展開する。
    if (m_HitWall && m_FlyingItem->IsPot())
    {
        MessageLog::Instance().AddMessage(m_FlyingItem->GetDisplayName() + u8"は割れた。");
        m_FlyingItem->OnBreak(m_TargetGrid);
        return true;
    }

    m_FlyingItem->Throw(thrower, nullptr, m_TargetGrid);
    if (m_FlyingItem->GetData()->type == ItemType::Stone)
    {
        MessageLog::Instance().AddMessage(m_FlyingItem->GetDisplayName() + u8"は砕け散った。");
        return true;
    }

    return DropFlyingItem(map);
}

bool FlyingObject::DropFlyingItem(MapData* map)
{
    if (!map || !m_FlyingItem || !m_FlyingItem->GetData())
    {
        // 配置先を解決できない場合は、飛翔物だけ残らないよう終了させる。
        m_FlyingItem.reset();
        m_IsActive = false;
        SetDestroy();
        return true;
    }

    m_CheckedTrapGrids.clear();
    m_DropResolutionState = DropResolutionState::None;

    MapObject* landingObject = map->GetObjectAt(m_TargetGrid);
    if (dynamic_cast<Trap*>(landingObject))
    {
        HandleDropCandidate(map, m_TargetGrid, true, true);
        return m_DropResolutionState == DropResolutionState::None;
    }

    // 既存アイテムなどで着地点が埋まっている場合は、時計回りに配置先を探す。
    if (landingObject || !map->IsTileFree(m_TargetGrid))
    {
        ContinueDropSearch(map);
        return m_DropResolutionState == DropResolutionState::None;
    }

    PlaceFlyingItem(map, m_TargetGrid);
    return true;
}

void FlyingObject::UpdateDropResolution()
{
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map || !m_FlyingItem)
    {
        m_IsActive = false;
        m_DropResolutionState = DropResolutionState::None;
        SetDestroy();
        return;
    }

    if (m_DropResolutionState == DropResolutionState::MovingToCandidate)
    {
        // 壁から選択マスまでは、Lerpではなく一定速度で直進させる。
        if (m_RotateItemInFlight) m_ItemSpin += 0.12f;
        Vector3 toTarget = m_TargetPos - m_Position;
        const float distance = toTarget.LengthSqrt();
        if (distance <= m_DropMoveSpeed || distance <= 0.001f)
        {
            m_Position = m_TargetPos;
            const Vector2Int grid = m_PendingDropGrid;
            const bool isTrap = m_PendingDropIsTrap;
            m_DropResolutionState = DropResolutionState::None;
            HandleDropCandidate(map, grid, isTrap, false);
            return;
        }

        toTarget.normalize();
        m_Position += toTarget * m_DropMoveSpeed;
        return;
    }

    if (m_DropResolutionState == DropResolutionState::WaitingForTrap)
    {
        // 罠の起動演出が終わるまでは、アイテムを罠マス上に待機させる。
        m_TrapWaitTimer -= Time::DeltaTime();
        if (m_TrapWaitTimer <= 0.0f)
        {
            m_DropResolutionState = DropResolutionState::None;
            ContinueDropSearch(map);
        }
    }
}

void FlyingObject::ContinueDropSearch(MapData* map)
{
    if (!map || !m_FlyingItem) return;

    const DropCandidate candidate = FindClockwiseDropGrid(map, m_TargetGrid, m_CheckedTrapGrids);
    if (candidate.type == DropCandidateType::None)
    {
        MessageLog::Instance().AddMessage(m_FlyingItem->GetDisplayName() + u8"は落ちる場所がなかった。");
        m_FlyingItem.reset();
        m_IsActive = false;
        m_DropResolutionState = DropResolutionState::None;
        SetDestroy();
        return;
    }

    HandleDropCandidate(map, candidate.grid, candidate.type == DropCandidateType::Trap, true);
}

void FlyingObject::HandleDropCandidate(MapData* map, const Vector2Int& grid, bool isTrap, bool allowMove)
{
    if (!map || !m_FlyingItem) return;

    const Vector3 candidatePos(static_cast<float>(grid.x * TILE_DISTANCE), m_Position.y,
        static_cast<float>(grid.y * TILE_DISTANCE));
    const Vector3 moveDistance = candidatePos - m_Position;

    // 壁に当たった後の再配置だけは、壁位置から選択マスまで直進表示を行う。
    if (allowMove && m_HitWall && moveDistance.LengthSqrt() > 0.001f)
    {
        StartDropMove(grid, isTrap);
        return;
    }

    m_Position = candidatePos;
    if (isTrap)
    {
        StartTrapWait(map, grid);
        return;
    }

    // 移動中に候補マスが埋まった場合は、同じ順序で選び直す。
    if (!map->IsTileFree(grid))
    {
        ContinueDropSearch(map);
        return;
    }

    PlaceFlyingItem(map, grid);
}

void FlyingObject::StartDropMove(const Vector2Int& grid, bool isTrap)
{
    m_PendingDropGrid = grid;
    m_PendingDropIsTrap = isTrap;
    m_TargetPos = Vector3(static_cast<float>(grid.x * TILE_DISTANCE), m_Position.y,
        static_cast<float>(grid.y * TILE_DISTANCE));
    m_DropResolutionState = DropResolutionState::MovingToCandidate;
    m_IsActive = true;
}

void FlyingObject::StartTrapWait(MapData* map, const Vector2Int& grid)
{
    Trap* trap = dynamic_cast<Trap*>(map ? map->GetObjectAt(grid) : nullptr);
    if (!trap)
    {
        ContinueDropSearch(map);
        return;
    }

    // 再抽選で同じ罠へ戻らないよう、起動前に処理済みとして記録する。
    if (std::find(m_CheckedTrapGrids.begin(), m_CheckedTrapGrids.end(), grid) == m_CheckedTrapGrids.end())
    {
        m_CheckedTrapGrids.push_back(grid);
    }

    if (!trap->ActivateByItem())
    {
        ContinueDropSearch(map);
        return;
    }

    m_TrapWaitTimer = Trap::GetItemActivationDuration();
    m_DropResolutionState = DropResolutionState::WaitingForTrap;
    m_IsActive = true;
}

void FlyingObject::PlaceFlyingItem(MapData* map, const Vector2Int& grid)
{
    if (!map || !m_FlyingItem) return;

    Item* dropped = Manager::GetScene()->AddGameObject<Item>(1);
    if (!dropped) return;

    const std::string itemName = m_FlyingItem->GetDisplayName();
    dropped->SetInstance(std::move(*m_FlyingItem));
    dropped->SetPosition(Vector3(static_cast<float>(grid.x * TILE_DISTANCE), 0.01f,
        static_cast<float>(grid.y * TILE_DISTANCE)));
    map->AddMapObject(dropped, grid.x, grid.y);
    MessageLog::Instance().AddMessage(itemName + u8"は地面に落ちた。");

    m_FlyingItem.reset();
    m_IsActive = false;
    m_DropResolutionState = DropResolutionState::None;
    SetDestroy();
}