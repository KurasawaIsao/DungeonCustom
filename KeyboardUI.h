#pragma once
#include <vector>
#include <string>

class KeyboardUI {
public:
    enum class Mode { Hiragana, Katakana };

    // キーボードを描画し、決定・キャンセル・入力継続の状態を返す
    // 戻り値: 1 = 決定, -1 = キャンセル, 0 = 入力中
    static int Draw(char* buf, size_t bufSize, int& modeIndex);

private:
    // 文字列操作ヘルパー（内部処理）
    static void ApplyDiacritic(char* buf, bool isHandakuten);
    static void BackspaceUTF8(char* buf);

    static const std::vector<const char*> m_KanaList;
    static const std::vector<const char*> m_KataList;
};