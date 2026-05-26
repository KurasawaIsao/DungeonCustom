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
#include "EffectTargeting.h"
#include "Item.h"
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
}
void FlyingObject::Init()
{
    m_Model = new ModelRenderer();
    m_Scale = { 0.5f, 0.5f, 0.5f };
}

void FlyingObject::Update()
{
    if (!m_IsActive) return;

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
    XMMATRIX rot = XMMatrixRotationRollPitchYaw(0.0f, std::atan2((float)m_Dir.x, (float)m_Dir.y), 0.0f);
    XMMATRIX trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(scale * rot * trans);

    m_Model->Draw();
}

void FlyingObject::Fire(const std::string& modelPath, ItemInstance* sourceItem, const Vector3& startPos,
    const Vector2Int& startGrid, const Vector2Int& dir, float speed, int maxRange, Unit* user)
{
    m_SourceItem = sourceItem;
    m_SourceEffect = nullptr;
    m_DropItemData = nullptr;
    m_User = user;
    m_SourceType = EffectSourceType::Item;
    SetupFlight(modelPath, startPos, startGrid, dir, speed, maxRange);
}

void FlyingObject::FireEffect(const std::string& modelPath, EffectBase* sourceEffect, Unit* user, EffectSourceType sourceType,
    const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& dir, float speed, int maxRange, const ItemData* dropItemData)
{
    m_SourceItem = nullptr;
    m_SourceEffect = sourceEffect;
    m_DropItemData = dropItemData;
    m_User = user;
    m_SourceType = sourceType;
    SetupFlight(modelPath, startPos, startGrid, dir, speed, maxRange);
}

void FlyingObject::FireEffectToTarget(const std::string& modelPath, EffectBase* sourceEffect, Unit* user, EffectSourceType sourceType,
    const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& targetGrid, float speed)
{
    m_SourceItem = nullptr;
    m_SourceEffect = sourceEffect;
    m_DropItemData = nullptr;
    m_User = user;
    m_SourceType = sourceType;
    SetupDirectFlight(modelPath, startPos, startGrid, targetGrid, speed);
}

void FlyingObject::SetupFlight(const std::string& modelPath, const Vector3& startPos,
    const Vector2Int& startGrid, const Vector2Int& dir, float speed, int maxRange)
{
    const Vector2Int flightDir = NormalizeFlightDir(dir);

    m_Model->Load(modelPath.c_str());
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
            targetPos = Vector3(next.x * 2.0f, startPos.y, next.y * 2.0f);
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

    if (Enemy* shopkeeper = dynamic_cast<Enemy*>(hitUnit))
    {
        ShopUI::AngerShopKeeper(shopkeeper);
    }

    if (hitUnit && m_SourceItem)
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
    else if (hitUnit && m_SourceEffect)
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
    else if (m_HitWall && m_DropItemData && map && map->IsTileFree(m_TargetGrid))
    {
        Item* dropped = Manager::GetScene()->AddGameObject<Item>(1);
        if (dropped)
        {
            ItemInstance inst(m_DropItemData);
            inst.SetStackCount(1);
            dropped->SetInstance(std::move(inst));
            dropped->SetPosition(Vector3(static_cast<float>(m_TargetGrid.x * TILE_DISTANCE), 0.01f,
                static_cast<float>(m_TargetGrid.y * TILE_DISTANCE)));
            map->AddMapObject(dropped, m_TargetGrid.x, m_TargetGrid.y);
        }
    }

    SetDestroy();
}
