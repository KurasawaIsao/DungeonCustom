#include "TitleUI.h"
#include "manager.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

void TitleUI::Init()
{
    m_TitleLogo = new Polygon2D();
    m_TitleLogo->Init(385, -50, SCREEN_WIDTH / 2.5, SCREEN_HEIGHT / 2.0, "Asset\\Texture\\Titile.png",1.0f);
    m_TitleLogo->SetPosition(Vector3(0.0f, -600.0f, 0.0f)); // ã
    m_TitleLogo->SetScale(Vector3(1.0f, 1.0f, 1.0f));

    m_LogoSpeed = 2.0f;
    m_LogoArrived = false;



}

void TitleUI::Start()
{
    m_Time = 0.0f;
    m_Started = true;
}
void TitleUI::Update()
{
    if (!m_Started) return;

    m_Time += 1.0f / 60.0f;

    if (!m_LogoArrived)
    {
        m_LogoSpeed += 0.25f;

        Vector3 pos = m_TitleLogo->GetPosition();
        pos.y += m_LogoSpeed;

        if (pos.y >= 0.0f)   // ŒÅ’èˆÊ’u
        {
            pos.y = 0.0f;
            m_LogoArrived = true;
            m_LogoSpeed = 0.0f;
        }

        m_TitleLogo->SetPosition(pos);
    }

}
void TitleUI::Draw()
{
    if (!m_Started) return;

    m_TitleLogo->Draw();
}
void TitleUI::Uninit()
{
}
void TitleUI::OnStartButtonPressed()
{
   
}
