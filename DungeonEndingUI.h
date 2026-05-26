#pragma once

#include "GameObject.h"
#include "GameUIObject.h"
#include <string>

class DungeonEndingUI
    : public GameObject
    , public GameUIObject
{
public:
    void StartDeath(
        const std::string& playerName,
        const std::string& dungeonName,
        const std::string& cause);
    void StartClear(
        const std::string& playerName,
        const std::string& dungeonName);

    bool IsPlaying() const { return m_Playing; }

    void Init() override {}
    void Uninit() override {}
    void Update() override;
    void Draw() override {}
    void DrawGameUI() override;

private:
    enum class EndingType
    {
        Death,
        Clear
    };

    void Start(EndingType type, const std::string& playerName, const std::string& dungeonName, const std::string& cause);

    EndingType m_Type = EndingType::Death;
    bool m_Playing = false;
    bool m_MessageReady = false;
    float m_Alpha = 0.0f;
    float m_Timer = 0.0f;

    std::string m_PlayerName;
    std::string m_DungeonName;
    std::string m_Cause;
};
