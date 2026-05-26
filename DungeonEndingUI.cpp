#include "DungeonEndingUI.h"
#include "Input.h"
#include "Manager.h"
#include "MessageLog.h"
#include "Renderer.h"
#include "Time.h"
#include "Title.h"
#include "TurnManager.h"
#include "UIRenderer.h"
#include "UITextRenderer.h"
#include "main.h"

#include <algorithm>

namespace
{
    constexpr float kFadeSpeed = 1.2f;
    constexpr float kMessageDelay = 0.35f;

    void RestoreMainRenderTarget()
    {
        ID3D11RenderTargetView* rtv = Renderer::GetMainRenderTargetView();
        Renderer::GetDeviceContext()->OMSetRenderTargets(1, &rtv, nullptr);
    }
}

void DungeonEndingUI::StartDeath(
    const std::string& playerName,
    const std::string& dungeonName,
    const std::string& cause)
{
    Start(EndingType::Death, playerName, dungeonName, cause);
}

void DungeonEndingUI::StartClear(
    const std::string& playerName,
    const std::string& dungeonName)
{
    Start(EndingType::Clear, playerName, dungeonName, "");
}

void DungeonEndingUI::Start(
    EndingType type,
    const std::string& playerName,
    const std::string& dungeonName,
    const std::string& cause)
{
    if (m_Playing) return;

    m_Type = type;
    m_PlayerName = playerName.empty() ? u8"プレイヤー" : playerName;
    m_DungeonName = dungeonName.empty() ? u8"ダンジョン" : dungeonName;
    m_Cause = cause.empty() ? u8"原因不明" : cause;
    m_Alpha = 0.0f;
    m_Timer = 0.0f;
    m_MessageReady = false;
    m_Playing = true;

    MessageLog::Instance().SetVisible(false);
    if (TurnManager::Instance())
        TurnManager::Instance()->PauseTurnProgression();
}

void DungeonEndingUI::Update()
{
    if (!m_Playing) return;

    const float dt = Time::DeltaTime();
    if (m_Alpha < 1.0f)
    {
        m_Alpha = (std::min)(1.0f, m_Alpha + dt * kFadeSpeed);
        return;
    }

    m_Timer += dt;
    if (m_Timer >= kMessageDelay)
        m_MessageReady = true;

    if (m_MessageReady && Input::GetKeyTrigger(VK_RETURN))
    {
        Manager::SetScene<Title>();
    }
}

void DungeonEndingUI::DrawGameUI()
{
    if (!m_Playing) return;

    const float alpha = std::clamp(m_Alpha, 0.0f, 1.0f);

    UIRenderer::DrawSolidRect(
        UIRect{ 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) },
        XMFLOAT4(0.0f, 0.0f, 0.0f, alpha * 0.88f));

    UIRenderer::DrawSolidRect(
        UIRect{ 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) },
        XMFLOAT4(0.42f, 0.0f, 0.0f, alpha * 0.34f));

    UIRenderer::DrawSolidRect(
        UIRect{ 0.0f, static_cast<float>(SCREEN_HEIGHT) * 0.5f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) * 0.5f },
        XMFLOAT4(0.0f, 0.0f, 0.0f, alpha * 0.28f));

    if (!m_MessageReady) return;

    const float textAlpha = std::clamp((m_Timer - kMessageDelay) / 0.35f, 0.0f, 1.0f);
    const float centerY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;
    const D2D1_COLOR_F white = D2D1::ColorF(1.0f, 0.96f, 0.90f, textAlpha);
    const D2D1_COLOR_F sub = D2D1::ColorF(0.78f, 0.72f, 0.68f, textAlpha);

    const std::string message =
        (m_Type == EndingType::Death)
        ? (m_Cause + u8"で倒れた")
        : (m_PlayerName + u8"は" + m_DungeonName + u8"を無事に突破した");

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8Aligned(
        message,
        40.0f,
        centerY - 42.0f,
        static_cast<float>(SCREEN_WIDTH) - 80.0f,
        64.0f,
        34.0f,
        white,
        DWRITE_TEXT_ALIGNMENT_CENTER);

    UITextRenderer::DrawOutlinedTextUtf8Aligned(
        u8"Enterでタイトルへ",
        40.0f,
        centerY + 34.0f,
        static_cast<float>(SCREEN_WIDTH) - 80.0f,
        44.0f,
        22.0f,
        sub,
        DWRITE_TEXT_ALIGNMENT_CENTER);
    UITextRenderer::End();
    RestoreMainRenderTarget();
}
