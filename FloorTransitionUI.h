#pragma once
#include "GameObject.h"
#include "GameUIObject.h"
#include <functional>
#include <string>

class FloorTransitionUI
    : public GameObject
    , public GameUIObject
{
public:
    void StartTransition(
        const std::string& dungeonId,
        int floorNumber,
        std::function<void()> onBlackCallback);

    void StartInitTransition(const std::string& dungeonId, int floorNumber);
    bool IsPlaying() const { return m_Playing; }

    void Init()override {};
    void Uninit()override {};
    void Update() override;
    void Draw()override {};
    void DrawGameUI() override;

private:
    enum class State
    {
        None,
        FadeIn,
        Execute,
        Wait,
        FadeOut
    };

    State m_State = State::None;

    float m_Alpha = 0.0f;
    float m_Timer = 0.0f;
    float m_FadeSpeed = 2.0f;

    bool m_Playing = false;

    std::string m_DungeonName;
    int m_FloorNumber = 1;

    std::function<void()> m_Callback;
};
