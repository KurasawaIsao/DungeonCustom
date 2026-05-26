#include "GameRandom.h"
#include "Trap.h"
#include "player.h"
#include "Unit.h"
#include "EffectBase.h"
#include "LightManager.h"
#include "EffectManager.h"
#include "TurnManager.h"
void Trap::Init()
{
    m_Model = new ModelRenderer();
    m_Model->Load("Asset\\Model\\Traps\\WarpTrap.obj");
    Renderer::InitCommonShader();
    m_Rotation = Vector3{ 0.0f,0.0f,0.0f };
    m_Scale = { 1.0f, 1.0f, 1.0f };
    m_IsVisible = false;
}

void Trap::Update()

{
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
        SetDestroy();
    }
}
void Trap::OnStepped(Player* player)
{
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