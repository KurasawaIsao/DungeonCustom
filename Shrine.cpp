#include "Shrine.h"
#include "MapManager.h"
#include "MessageLog.h"
#include "Renderer.h"
#include "Manager.h"
#include "Scene.h"
#include "PlayerInventoryUI.h"

void Shrine::Init() {
    m_Model = new AnimationModel();
    m_Model->Load("Asset\\Model\\Shrine.obj"); // âK—p‚Ìƒ‚ƒfƒ‹
    m_Scale = { 1.0f, 1.0f, 1.0f };
}

void Shrine::Draw() {
    // Trap::Draw() ‚Æ“¯—l‚Ì•`‰æˆ—
    XMMATRIX world = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
        XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(world);
    m_Model->Draw();
}

void Shrine::Uninit()
{
    if (m_Model)
    { m_Model->Uninit(); delete m_Model; }
}

namespace
{
    void OpenShrineConfirmUI()
    {
        auto* scene = Manager::GetScene();
        auto* ui = scene ? scene->GetGameObject<PlayerInventoryUI>() : nullptr;
        if (ui)
        {
            ui->OpenShrineConfirm();
        }
    }
}

void Shrine::OnStepped(Player* player)
{
    (void)player;
    OpenShrineConfirmUI();
}

void Shrine::OnAttacked()
{
    OpenShrineConfirmUI();
}
