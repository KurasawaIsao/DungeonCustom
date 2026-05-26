#include "FloorTransitionUI.h"
#include "Renderer.h"
#include "Time.h"
#include "UIRenderer.h"
#include "UITextRenderer.h"

#include <algorithm>
#include <string>

namespace
{
    void RestoreMainRenderTarget()
    {
        ID3D11RenderTargetView* rtv = Renderer::GetMainRenderTargetView();
        Renderer::GetDeviceContext()->OMSetRenderTargets(1, &rtv, nullptr);
    }
}

void FloorTransitionUI::StartTransition(
    const std::string& dungeonId,
    int floorNumber,
    std::function<void()> onBlackCallback)
{
    if (m_Playing) return;

    m_DungeonName = dungeonId;

    m_FloorNumber = floorNumber;
    if (onBlackCallback)
    {
        m_Callback = onBlackCallback;
    }

    m_Alpha = 0.0f;
    m_Timer = 0.0f;
    m_State = State::FadeIn;
    m_Playing = true;
}

void FloorTransitionUI::StartInitTransition(const std::string& dungeonId, int floorNumber)
{
    if (m_Playing) return;

    m_DungeonName = dungeonId;
    m_FloorNumber = floorNumber;

    m_Alpha = 1.0f;
    m_Timer = 0.0f;
    m_State = State::Wait;
    m_Playing = true;
    m_Callback = nullptr;
}

void FloorTransitionUI::Update()
{
    if (!m_Playing) return;

    float dt = Time::DeltaTime();

    switch (m_State)
    {
    case State::FadeIn:
        m_Alpha += dt * m_FadeSpeed;
        if (m_Alpha >= 1.0f)
        {
            m_Alpha = 1.0f;
            m_State = State::Execute;
        }
        break;

    case State::Execute:
        if (m_Callback)
            m_Callback();
        m_State = State::Wait;
        break;

    case State::Wait:
        m_Timer += dt;
        if (m_Timer >= 1.0f)
            m_State = State::FadeOut;
        break;

    case State::FadeOut:
        m_Alpha -= dt * m_FadeSpeed;
        if (m_Alpha <= 0.0f)
        {
            m_Alpha = 0.0f;
            m_State = State::None;
            m_Playing = false;
        }
        break;
    }
}

void FloorTransitionUI::DrawGameUI()
{
    if (!m_Playing) return;

    const float alpha = std::clamp(m_Alpha, 0.0f, 1.0f);
    UIRenderer::DrawSolidRect(
        UIRect{ 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) },
        XMFLOAT4(0.0f, 0.0f, 0.0f, alpha));

    if (alpha <= 0.6f) return;

    const float textAlpha = std::clamp((alpha - 0.6f) / 0.4f, 0.0f, 1.0f);
    const float centerY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;
    const std::string floorText = std::to_string(m_FloorNumber) + "F";
    const D2D1_COLOR_F white = D2D1::ColorF(1.0f, 1.0f, 1.0f, textAlpha);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8Aligned(
        m_DungeonName,
        0.0f,
        centerY - 62.0f,
        static_cast<float>(SCREEN_WIDTH),
        56.0f,
        40.0f,
        white,
        DWRITE_TEXT_ALIGNMENT_CENTER);
    UITextRenderer::DrawOutlinedTextUtf8Aligned(
        floorText,
        0.0f,
        centerY + 8.0f,
        static_cast<float>(SCREEN_WIDTH),
        64.0f,
        50.0f,
        white,
        DWRITE_TEXT_ALIGNMENT_CENTER);
    UITextRenderer::End();
    RestoreMainRenderTarget();
}
