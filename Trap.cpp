#include "GameRandom.h"
#include "Trap.h"
#include "player.h"
#include "Unit.h"
#include "EffectBase.h"
#include "LightManager.h"
#include "EffectManager.h"
#include "TurnManager.h"
#include "MapManager.h"
#include "UnitManager.h"
#include "Time.h"
#include <algorithm>
#include <cmath>
void Trap::Init()
{
    m_Model = new ModelRenderer();
    m_Model->Load("Asset\\Model\\Traps\\WarpTrap.obj");
    Renderer::InitCommonShader();
    m_Rotation = Vector3{ 0.0f,0.0f,0.0f };
    m_Scale = { 1.0f, 1.0f, 1.0f };
    m_IsVisible = true;
}

void Trap::Update()
{
    if (m_ItemActivationTimer <= 0.0f) return;

    // アイテムで起動した罠は、待機時間に合わせて一度だけ拡縮させる。
    m_ItemActivationTimer = (std::max)(0.0f, m_ItemActivationTimer - Time::DeltaTime());
    const float progress = 1.0f - m_ItemActivationTimer / GetItemActivationDuration();
    const float pulse = std::sin(progress * 3.14159265f) * 0.2f;
    m_Scale = { 1.0f + pulse, 1.0f + pulse, 1.0f + pulse };

    if (m_ItemActivationTimer <= 0.0f)
    {
        m_Scale = { 1.0f, 1.0f, 1.0f };
        if (m_DestroyAfterItemActivation) SetDestroy();
    }
}

void Trap::Draw()
{
    if (!m_IsVisible) return;
    Renderer::SetLight(LightManager::Instance().GetLight());
    Renderer::SetCommonShader();


    XMMATRIX scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    XMMATRIX trans = XMMatrixTranslation(m_Position.x, m_Position.y-0.5f, m_Position.z);
    XMMATRIX world = scale * rot * trans;
    Renderer::SetWorldMatrix(world);

    m_Model->Draw();
}
void Trap::Activate(Unit* target) {
    if (!target || !m_Data || !m_Data->effect) return;

    const int currentTurn = TurnManager::Instance() ? TurnManager::Instance()->GetTurnCount() : 0;
    if (m_LastActivatedTurn == currentTurn) return;
    m_LastActivatedTurn = currentTurn;

    // 罠発動ごとに表示中ログをリセットする。履歴は MessageLog 側に残す。
    MessageLog::Instance().Clear();

    MessageLog::Instance().AddMessage(target->GetName() + u8"は" + m_Data->name + u8"を踏んだ！");
    if (Player* player = dynamic_cast<Player*>(target))
    {
        player->SetPendingDeathCause(m_Data->name);
    }

    EffectContext ctx;
    ctx.source = EffectSourceType::Trap; 
    ctx.target = target;
    ctx.pos = m_GridPos;

    m_Data->effect->Apply(ctx);
    if (m_Data->singleUse || (m_Data->breakChancePercent > 0 && GameRandom::Percent(m_Data->breakChancePercent))) {
        if (auto* map = MapManager::Instance()->GetCurrentMap())
        {
            map->RemoveMapObject(this);
        }

        SetDestroy();
    }
}
bool Trap::ActivateByItem(ItemInstance* item)
{
    if (!m_Data || !m_Data->effect) return false;

    const int currentTurn = TurnManager::Instance() ? TurnManager::Instance()->GetTurnCount() : 0;
    if (m_LastActivatedTurn == currentTurn) return false;
    m_LastActivatedTurn = currentTurn;

    // アイテム着地では不発判定を行わず、罠の表示と効果を直接解決する。
    EffectManager::PlaySE("Asset\\Sound\\Switch.wav");
    m_IsVisible = true;
    m_ItemActivationTimer = GetItemActivationDuration();
    MessageLog::Instance().AddMessage(m_Data->name + u8"が作動した！");

    EffectContext ctx;
    ctx.source = EffectSourceType::Trap;
    // アイテムで作動した場合だけ、罠マス上の敵・仲間も効果対象として渡す。
    ctx.target = UnitManager::Instance() ? UnitManager::Instance()->GetUnitAt(m_GridPos) : nullptr;
    // 泥の罠などが、作動させた落下アイテム自体を変更できるように渡す。
    ctx.item = item;
    ctx.pos = m_GridPos;
    m_Data->effect->Apply(ctx);

    if (m_Data->singleUse || (m_Data->breakChancePercent > 0 && GameRandom::Percent(m_Data->breakChancePercent)))
    {
        if (auto* map = MapManager::Instance()->GetCurrentMap())
        {
            // 効果側が罠を別アイテムへ置き換えた場合、新しく置いたアイテムは削除しない。
            if (map->GetObjectAt(m_GridPos) == this)
                map->RemoveMapObject(this);
        }
        m_DestroyAfterItemActivation = true;
    }
    return true;
}
void Trap::OnStepped(Player* player)
{
    // 罠の作動・不発演出へ入る時は、次入力による移動モーション継続を無効にする。
    if (player) player->ClearMoveRunHold();

    const int currentTurn = TurnManager::Instance() ? TurnManager::Instance()->GetTurnCount() : 0;
    if (m_LastActivatedTurn == currentTurn) return;

    // 効果の発動
    EffectManager::PlaySE("Asset\\Sound\\Switch.wav");
    m_IsVisible = true;
    if (GameRandom::Percent(30))
    {
        MessageLog::Instance().AddMessage( u8"何か罠を踏んだ！");
        MessageLog::Instance().AddMessage(u8"しかし罠は動かなかった。");
    }
    else
    {
        Activate(player);
    }
  
}