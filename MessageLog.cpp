#include "MessageLog.h"
#include "Renderer.h"
#include "UIRenderer.h"
#include "UITextRenderer.h"

MessageLog& MessageLog::Instance()
{
    static MessageLog inst;
    return inst;
}

void MessageLog::AddMessage(const std::string& msg)
{
    // 表示用ログとは別に、あとで見返すための履歴へも必ず残す。
    m_History.push_back(msg);
    m_Logs.push_back(msg);
    m_ScrollToBottom = true;

    if (m_MaxLines > 0 && m_Logs.size() > m_MaxLines)
    {
        m_Logs.erase(m_Logs.begin(), m_Logs.begin() + (m_Logs.size() - m_MaxLines));
    }

    m_Visible = true;
    m_VisibleTimer = m_VisibleDuration;
}

void MessageLog::Clear()
{
    // 攻撃や罠など、新しい出来事の表示を始めるために現在表示分だけ消す。
    m_Logs.clear();
    m_Visible = false;
    m_VisibleTimer = 0.0f;
}

void MessageLog::ClearHistory()
{
    m_History.clear();
    Clear();
}

void MessageLog::Draw(const char* windowName)
{
    (void)windowName;
    if (!m_Visible) return;

    UIRect windowRect{ 140.0f, 520.0f, 1000.0f, 170.0f };
    UIRenderer::DrawWindow(windowRect);

    UITextRenderer::Begin();

    const int total = static_cast<int>(m_Logs.size());
    const int maxLines = static_cast<int>(m_MaxLines);
    const int visibleLines = (maxLines > 0) ? maxLines : total;
    const int start = (total > visibleLines) ? total - visibleLines : 0;

    float y = windowRect.y + 28.0f;
    for (int i = start; i < total; ++i)
    {
        UITextRenderer::DrawOutlinedTextUtf8(
            m_Logs[i],
            windowRect.x + 28.0f,
            y,
            windowRect.w - 56.0f,
            36.0f,
            30.0f,
            D2D1::ColorF(D2D1::ColorF::White));
        y += 34.0f;
    }

    UITextRenderer::End();

    ID3D11RenderTargetView* rtv = Renderer::GetMainRenderTargetView();
    Renderer::GetDeviceContext()->OMSetRenderTargets(1, &rtv, nullptr);
}

void MessageLog::Update(float dt)
{
    if (m_Visible)
    {
        m_VisibleTimer -= dt;
        if (m_VisibleTimer <= 0.0f)
        {
            m_Visible = false;
            Clear();
        }
    }
}
