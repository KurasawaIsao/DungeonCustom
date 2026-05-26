#pragma once
#include "GameObject.h"

class TurnManager : public GameObject
{
public:
    enum class Phase
    {
        PlayerTurn,
        EnemyAction,
        EnemyMove,
        EnemyPostMoveAction,
        AllyAction,
        AllyMove,
        MoveResolution
    };

private:
    static TurnManager* instance;

    Phase m_Phase = Phase::PlayerTurn;

    int m_TurnCount = 0;
    static constexpr int SPAWN_INTERVAL = 25;
    static constexpr int THEFT_SPAWN_INTERVAL = 3;
    // true の間は自然湧きが敵対店番寄りになり、通常より短い間隔で湧く。
    bool m_ShopTheftMode = false;
    // 倍速/三倍速の敵が追加行動を続けるべきか判定するための進捗フラグ。
    bool m_EnemyActionLoopHadProgress = false;
    // 勧誘メニューなど、UI がターン進行を止めたい時に使う一時停止フラグ。
    bool m_IsPaused = false;

public:
    TurnManager()
    {
        instance = this;
    }

    static TurnManager* Instance()
    {
        return instance;
    }

    void Init() override { ResetDungeonState(); }
    void Draw() override {}
    void Uninit() override { if (instance == this) instance = nullptr; }

    void ResetTurnCount() { m_TurnCount = 0; }
    void ResetDungeonState()
    {
        m_Phase = Phase::PlayerTurn;
        m_TurnCount = 0;
        m_ShopTheftMode = false;
        m_EnemyActionLoopHadProgress = false;
        m_IsPaused = false;
    }
    void StartPlayerTurn();
    void StartEnemyTurn();
    void FinishTurnCycle();
    void SpawnEnemy();
    void SetShopTheftMode(bool enabled) { m_ShopTheftMode = enabled; }
    bool IsShopTheftMode() const { return m_ShopTheftMode; }
    void PauseTurnProgression() { m_IsPaused = true; }
    void ResumeTurnProgression() { m_IsPaused = false; }
    bool IsTurnProgressionPaused() const { return m_IsPaused; }

    void Update() override;
    void ResolveAfterPlayerInstantMove();

    Phase GetPhase() const { return m_Phase; }
    int GetTurnCount() const { return m_TurnCount; }
};
