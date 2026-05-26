#include "Fade.h"
#include "main.h" // SCREEN_WIDTH, SCREEN_HEIGHT が定義されている想定

void Fade::Init() {
    m_Polygon = new Polygon2D();
    // 画面全体サイズで初期化。FileNameにnullptrを指定
    m_Polygon->Init(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, "texture\\shadow.png");
    m_Polygon->SetPosition(Vector3(0.0f, 0.0f, 0.0f)); // 上
    m_Polygon->SetScale(Vector3(1.0f, 1.0f, 1.0f));

    // 初期色は黒。最初は透明(0.0f)
    m_Polygon->SetColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    m_Alpha = 1.0f;
    m_State = State::None;
}

void Fade::Update() {
    if (m_State == State::FadeIn) {
        m_Alpha -= FADE_SPEED;
        if (m_Alpha <= 0.0f) {
            m_Alpha = 0.0f;
            m_State = State::None;
        }
    }
    else if (m_State == State::FadeOut) {
        m_Alpha += FADE_SPEED;
        if (m_Alpha >= 1.0f) {
            m_Alpha = 1.0f;
            m_State = State::None;
            // State::None にせず、暗転状態で止める（シーン遷移待ち）
        }
    }

    // アルファ値をポリゴンに反映
    m_Polygon->SetAlpha(m_Alpha);
}

void Fade::Draw() {
    if (m_Alpha > 0.0f) {
        // 2D描画。他のゲームオブジェクトより後に描画することで最前面にくる
       
    }
    m_Polygon->Draw();
}

void Fade::Uninit() {
    if (m_Polygon) {
        m_Polygon->Uninit();
        delete m_Polygon;
        m_Polygon = nullptr;
    }
}