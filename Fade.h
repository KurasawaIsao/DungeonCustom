#pragma once
#include "Polygon.h"
#include "GameObject.h"
enum class State {
    None,      // 待機
    FadeIn,    // 明るくなる
    FadeOut,   // 暗くなる
};
class Fade :public GameObject{
public:
 

    void Init();
    void Uninit();
    void Update();
    void Draw();

    // フェード開始指示
    void StartFadeIn() { m_State = State::FadeIn; }
    void StartFadeOut() { m_State = State::FadeOut; }

    // 現在の状態を取得（シーン切り替えのタイミング判定用）
    State GetState() const { return m_State; }

private:
    Polygon2D* m_Polygon = nullptr;
    State      m_State = State::None;
    float      m_Alpha = 0.0f;
    const float FADE_SPEED = 0.02f; // フェードの速さ
};