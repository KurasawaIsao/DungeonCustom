#include "KeyboardUI.h"
#include "main.h"
#include "Input.h"
#include "Renderer.h"
#include "UIRenderer.h"
#include "UITextRenderer.h"
#include <unordered_map>
#include <cstring>
#include <algorithm>

const std::vector<const char*> KeyboardUI::m_KanaList = {
    u8"あ", u8"か", u8"さ", u8"た", u8"な", u8"は", u8"ま", u8"や", u8" ", u8"ら", u8"わ",
    u8"い", u8"き", u8"し", u8"ち", u8"に", u8"ひ", u8"み", u8" ", u8" ", u8"り", u8"を",
    u8"う", u8"く", u8"す", u8"つ", u8"ぬ", u8"ふ", u8"む", u8"ゆ", u8" ", u8"る", u8"ん",
    u8"え", u8"け", u8"せ", u8"て", u8"ね", u8"へ", u8"め", u8" ", u8" ", u8"れ", u8" ",
    u8"お", u8"こ", u8"そ", u8"と", u8"の", u8"ほ", u8"も", u8"よ", u8" ", u8"ろ", u8"ー"
};

const std::vector<const char*> KeyboardUI::m_KataList = {
    u8"ア", u8"カ", u8"サ", u8"タ", u8"ナ", u8"ハ", u8"マ", u8"ヤ", u8" ", u8"ラ", u8"ワ",
    u8"イ", u8"キ", u8"シ", u8"チ", u8"ニ", u8"ヒ", u8"ミ", u8" ", u8" ", u8"リ", u8"ヲ",
    u8"ウ", u8"ク", u8"ス", u8"ツ", u8"ヌ", u8"フ", u8"ム", u8"ユ", u8" ", u8"ル", u8"ン",
    u8"エ", u8"ケ", u8"セ", u8"テ", u8"ネ", u8"ヘ", u8"メ", u8" ", u8" ", u8"レ", u8" ",
    u8"オ", u8"コ", u8"ソ", u8"ト", u8"ノ", u8"ホ", u8"モ", u8"ヨ", u8" ", u8"ロ", u8"ー"
};

namespace
{
    int g_KeyCursor = 0;

    struct SourceKey
    {
        const char* label;
        const char* value;
    };

    const std::vector<SourceKey> kAlphaList = {
        {"A","A"}, {"B","B"}, {"C","C"}, {"D","D"}, {"E","E"}, {"F","F"}, {"G","G"}, {"H","H"}, {"I","I"}, {"J","J"}, {"K","K"},
        {"L","L"}, {"M","M"}, {"N","N"}, {"O","O"}, {"P","P"}, {"Q","Q"}, {"R","R"}, {"S","S"}, {"T","T"}, {"U","U"}, {"V","V"},
        {"W","W"}, {"X","X"}, {"Y","Y"}, {"Z","Z"}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
        {"a","a"}, {"b","b"}, {"c","c"}, {"d","d"}, {"e","e"}, {"f","f"}, {"g","g"}, {"h","h"}, {"i","i"}, {"j","j"}, {"k","k"},
        {"l","l"}, {"m","m"}, {"n","n"}, {"o","o"}, {"p","p"}, {"q","q"}, {"r","r"}, {"s","s"}, {"t","t"}, {"u","u"}, {"v","v"},
        {"w","w"}, {"x","x"}, {"y","y"}, {"z","z"}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
        {"0","0"}, {"1","1"}, {"2","2"}, {"3","3"}, {"4","4"}, {"5","5"}, {"6","6"}, {"7","7"}, {"8","8"}, {"9","9"}, {"",""},
        {u8"空白"," "}, {"-","-"}, {"_","_"}, {".","."}
    };

    const std::vector<SourceKey> kSymbolList = {
        {"!","!"}, {"?","?"}, {"#","#"}, {"$","$"}, {"%","%"}, {"&","&"}, {"*","*"}, {"+","+"}, {"-","-"}, {"/","/"}, {"=","="},
        {"(","("}, {")",")"}, {"[","["}, {"]","]"}, {"{","{"}, {"}","}"}, {"<","<"}, {">",">"}, {"@","@"}, {":",":"}, {";",";"},
        {"'","'"}, {"\"","\""}, {",",","}, {".","."}, {"_","_"}, {"~","~"}, {"^","^"}, {"|","|"}, {"\\","\\"}, {u8"空白"," "}, {u8"・",u8"・"},
        {u8"。",u8"。"}, {u8"、",u8"、"}, {u8"「",u8"「"}, {u8"」",u8"」"}, {u8"?",u8"?"}, {u8"ー",u8"ー"}, {u8"…",u8"…"}
    };

    void RestoreMainRenderTarget()
    {
        ID3D11RenderTargetView* rtv = Renderer::GetMainRenderTargetView();
        Renderer::GetDeviceContext()->OMSetRenderTargets(1, &rtv, nullptr);
    }

    void DrawKeyText(const std::string& text, bool selected, float x, float y, float width, float fontSize = 20.0f)
    {
        std::string line = selected ? u8"・" + text : text;
        UITextRenderer::DrawOutlinedTextUtf8(
            line,
            x,
            y,
            width,
            28.0f,
            fontSize,
            selected ? D2D1::ColorF(1.0f, 0.92f, 0.45f, 1.0f) : D2D1::ColorF(D2D1::ColorF::White));
    }

    bool IsUsableKana(const char* text)
    {
        return text && strcmp(text, " ") != 0;
    }

    const char* GetModeName(int modeIndex)
    {
        switch (modeIndex)
        {
        case 1: return u8"カタカナ";
        case 2: return "ABC";
        case 3: return u8"記号";
        default: return u8"ひらがな";
        }
    }
}

int KeyboardUI::Draw(char* buf, size_t bufSize, int& modeIndex) {
    enum class Action
    {
        Insert,
        Hiragana,
        Katakana,
        Alphabet,
        Symbol,
        Daku,
        Handaku,
        Backspace,
        Decide,
        Cancel
    };

    struct KeyEntry
    {
        std::string label;
        std::string value;
        Action action;
        int row;
        int col;
        float width;
    };

    if (modeIndex < 0 || modeIndex > 3) modeIndex = 0;

    const int columns = 11;
    std::vector<SourceKey> source;
    if (modeIndex == 0 || modeIndex == 1)
    {
        const auto& kana = (modeIndex == 1) ? m_KataList : m_KanaList;
        source.reserve(kana.size());
        for (const char* text : kana)
        {
            source.push_back({ text, IsUsableKana(text) ? text : "" });
        }
    }
    else if (modeIndex == 2)
    {
        source = kAlphaList;
    }
    else
    {
        source = kSymbolList;
    }

    std::vector<KeyEntry> keys;
    keys.reserve(source.size() + 9);

    for (int i = 0; i < static_cast<int>(source.size()); ++i)
    {
        if (!source[i].value || source[i].value[0] == '\0') continue;
        keys.push_back({ source[i].label, source[i].value, Action::Insert, i / columns, i % columns, 52.0f });
    }

    const int gridRows = (static_cast<int>(source.size()) + columns - 1) / columns;
    const int modeRow = gridRows;
    const int actionRow = gridRows + 1;

    keys.push_back({ u8"ひらがな", "", Action::Hiragana, modeRow, 0, 92.0f });
    keys.push_back({ u8"カタカナ", "", Action::Katakana, modeRow, 2, 92.0f });
    keys.push_back({ "ABC", "", Action::Alphabet, modeRow, 4, 68.0f });
    keys.push_back({ u8"記号", "", Action::Symbol, modeRow, 6, 68.0f });
    keys.push_back({ u8"゛", "", Action::Daku, actionRow, 0, 46.0f });
    keys.push_back({ u8"゜", "", Action::Handaku, actionRow, 1, 46.0f });
    keys.push_back({ u8"消す", "", Action::Backspace, actionRow, 3, 72.0f });
    keys.push_back({ u8"決定", "", Action::Decide, actionRow, 5, 82.0f });
    keys.push_back({ u8"キャンセル", "", Action::Cancel, actionRow, 7, 118.0f });

    if (g_KeyCursor < 0) g_KeyCursor = 0;
    if (g_KeyCursor >= static_cast<int>(keys.size())) g_KeyCursor = static_cast<int>(keys.size()) - 1;

    const int maxRow = actionRow;

    auto findNearestInRow = [&](int row, int col) {
        int best = -1;
        int bestDistance = 9999;
        for (int i = 0; i < static_cast<int>(keys.size()); ++i)
        {
            if (keys[i].row != row) continue;
            const int distance = (keys[i].col > col) ? (keys[i].col - col) : (col - keys[i].col);
            if (distance < bestDistance)
            {
                best = i;
                bestDistance = distance;
            }
        }
        return best;
    };

    auto moveHorizontal = [&](int dir) {
        const int row = keys[g_KeyCursor].row;
        const int col = keys[g_KeyCursor].col;
        int best = -1;
        int bestCol = (dir > 0) ? 9999 : -9999;
        int wrap = -1;
        int wrapCol = (dir > 0) ? 9999 : -9999;

        for (int i = 0; i < static_cast<int>(keys.size()); ++i)
        {
            if (keys[i].row != row) continue;
            const int candidateCol = keys[i].col;
            if (dir > 0)
            {
                if (candidateCol > col && candidateCol < bestCol) { best = i; bestCol = candidateCol; }
                if (candidateCol < wrapCol) { wrap = i; wrapCol = candidateCol; }
            }
            else
            {
                if (candidateCol < col && candidateCol > bestCol) { best = i; bestCol = candidateCol; }
                if (candidateCol > wrapCol) { wrap = i; wrapCol = candidateCol; }
            }
        }
        g_KeyCursor = (best >= 0) ? best : wrap;
    };

    auto moveVertical = [&](int dir) {
        const int col = keys[g_KeyCursor].col;
        int row = keys[g_KeyCursor].row;
        for (int attempt = 0; attempt <= maxRow; ++attempt)
        {
            row = (row + dir + maxRow + 1) % (maxRow + 1);
            const int found = findNearestInRow(row, col);
            if (found >= 0)
            {
                g_KeyCursor = found;
                return;
            }
        }
    };

    if (Input::GetKeyTrigger(VK_LEFT)) moveHorizontal(-1);
    if (Input::GetKeyTrigger(VK_RIGHT)) moveHorizontal(1);
    if (Input::GetKeyTrigger(VK_UP)) moveVertical(-1);
    if (Input::GetKeyTrigger(VK_DOWN)) moveVertical(1);
    if (Input::GetKeyTrigger('X')) return -1;

    int result = 0;
    if (Input::GetKeyTrigger('Z'))
    {
        const KeyEntry& key = keys[g_KeyCursor];
        switch (key.action)
        {
        case Action::Insert:
            if (strlen(buf) + key.value.size() < bufSize - 1) strcat_s(buf, bufSize, key.value.c_str());
            break;
        case Action::Hiragana:
            modeIndex = 0;
            break;
        case Action::Katakana:
            modeIndex = 1;
            break;
        case Action::Alphabet:
            modeIndex = 2;
            break;
        case Action::Symbol:
            modeIndex = 3;
            break;
        case Action::Daku:
            ApplyDiacritic(buf, false);
            break;
        case Action::Handaku:
            ApplyDiacritic(buf, true);
            break;
        case Action::Backspace:
            BackspaceUTF8(buf);
            break;
        case Action::Decide:
            result = 1;
            break;
        case Action::Cancel:
            result = -1;
            break;
        }
    }

    UIRect rect{ (SCREEN_WIDTH - 690.0f) * 0.5f, 48.0f, 690.0f, 500.0f };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"名前入力", rect.x + 24.0f, rect.y + 18.0f, rect.w - 48.0f, 28.0f, 22.0f, D2D1::ColorF(D2D1::ColorF::White));

    const std::string nameText = std::string(u8"なまえ: ") + buf + "_";
    UITextRenderer::DrawOutlinedTextUtf8(nameText, rect.x + 34.0f, rect.y + 54.0f, rect.w - 68.0f, 30.0f, 22.0f, D2D1::ColorF(1.0f, 0.92f, 0.45f, 1.0f));

    const float startX = rect.x + 34.0f;
    const float startY = rect.y + 96.0f;
    const float cellW = 56.0f;
    const float cellH = 38.0f;

    for (int i = 0; i < static_cast<int>(keys.size()); ++i)
    {
        const KeyEntry& key = keys[i];
        const bool selected = (i == g_KeyCursor);
        const float x = startX + key.col * cellW;
        const float y = startY + key.row * cellH;
        const float fontSize = (key.action == Action::Insert) ? 22.0f : 18.0f;
        DrawKeyText(key.label, selected, x, y, key.width, fontSize);
    }

    UITextRenderer::DrawOutlinedTextUtf8(GetModeName(modeIndex), rect.x + rect.w - 132.0f, rect.y + 20.0f, 110.0f, 24.0f, 17.0f, D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f));

    UITextRenderer::End();
    RestoreMainRenderTarget();
    return result;
}

void KeyboardUI::ApplyDiacritic(char* buf, bool isHandakuten) {
    size_t len = strlen(buf);
    if (len == 0) return;
    size_t last_pos = 0; size_t i = 0;
    while (buf[i]) {
        last_pos = i; unsigned char c = (unsigned char)buf[i];
        if (c < 0x80) i += 1; else if (c < 0xE0) i += 2; else if (c < 0xF0) i += 3; else i += 4;
    }
    std::string lastChar(&buf[last_pos]);
    std::string converted = lastChar;

    if (!isHandakuten) {
        static std::unordered_map<std::string, std::string> dakuDict = {
            {u8"か",u8"が"}, {u8"き",u8"ぎ"}, {u8"く",u8"ぐ"}, {u8"け",u8"げ"}, {u8"こ",u8"ご"},
            {u8"さ",u8"ざ"}, {u8"し",u8"じ"}, {u8"す",u8"ず"}, {u8"せ",u8"ぜ"}, {u8"そ",u8"ぞ"},
            {u8"た",u8"だ"}, {u8"ち",u8"ぢ"}, {u8"つ",u8"づ"}, {u8"て",u8"で"}, {u8"と",u8"ど"},
            {u8"は",u8"ば"}, {u8"ひ",u8"び"}, {u8"ふ",u8"ぶ"}, {u8"へ",u8"べ"}, {u8"ほ",u8"ぼ"},
            {u8"カ",u8"ガ"}, {u8"キ",u8"ギ"}, {u8"ク",u8"グ"}, {u8"ケ",u8"ゲ"}, {u8"コ",u8"ゴ"},
            {u8"サ",u8"ザ"}, {u8"シ",u8"ジ"}, {u8"ス",u8"ズ"}, {u8"セ",u8"ゼ"}, {u8"ソ",u8"ゾ"},
            {u8"タ",u8"ダ"}, {u8"チ",u8"ヂ"}, {u8"ツ",u8"ヅ"}, {u8"テ",u8"デ"}, {u8"ト",u8"ド"},
            {u8"ハ",u8"バ"}, {u8"ヒ",u8"ビ"}, {u8"フ",u8"ブ"}, {u8"ヘ",u8"ベ"}, {u8"ホ",u8"ボ"}
        };
        if (dakuDict.count(lastChar)) converted = dakuDict[lastChar];
    }
    else {
        static std::unordered_map<std::string, std::string> handakuDict = {
            {u8"は",u8"ぱ"}, {u8"ひ",u8"ぴ"}, {u8"ふ",u8"ぷ"}, {u8"へ",u8"ぺ"}, {u8"ほ",u8"ぽ"},
            {u8"ハ",u8"パ"}, {u8"ヒ",u8"ピ"}, {u8"フ",u8"プ"}, {u8"ヘ",u8"ペ"}, {u8"ホ",u8"ポ"}
        };
        if (handakuDict.count(lastChar)) converted = handakuDict[lastChar];
    }
    buf[last_pos] = '\0';
    strcat_s(buf, 64, converted.c_str());
}

void KeyboardUI::BackspaceUTF8(char* buf) {
    size_t len = strlen(buf);
    if (len == 0) return;
    size_t i = 0, last_char_pos = 0;
    while (buf[i]) {
        last_char_pos = i;
        unsigned char c = (unsigned char)buf[i];
        if (c < 0x80) i += 1; else if (c < 0xE0) i += 2; else if (c < 0xF0) i += 3; else i += 4;
    }
    buf[last_char_pos] = '\0';
}