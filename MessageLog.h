#pragma once
#include <vector>
#include <string>


class MessageLog
{
private:
    MessageLog() = default;
    std::vector<std::string> m_Logs;
    std::vector<std::string> m_History;
    bool m_ScrollToBottom = false;

    bool m_Visible = false;
    float m_VisibleTimer = 0.0f;   // 表示持続時間
    float m_VisibleDuration = 3.0f; // 3秒表示するなど


    size_t m_MaxLines = 3; // 0 = 無制限
public:
    static MessageLog& Instance();

    // メッセージを追加
    void AddMessage(const std::string& msg);

    // 表示中のログだけを消す。履歴は m_History に残す。
    void Clear();
    void ClearHistory();
    const std::vector<std::string>& GetHistory() const { return m_History; }

    // メッセージログ描画
    void Draw(const char* windowName = "Message Log");

    void Update(float dt);

    bool IsVisible() { return m_Visible; }
    void SetVisible(bool vis) { m_Visible = vis; }

    // 表示される最大行数（0 = 無制限）
    void SetMaxLines(size_t maxLines) { m_MaxLines = maxLines; }  

};
