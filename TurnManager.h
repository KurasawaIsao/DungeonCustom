#pragma once
#include "GameObject.h"

class TurnManager : public GameObject
{
private:
    // ターンは「入力」「攻撃/特技」「移動」「移動後処理」に分けて進める。
    // 1フレームで全て進めず、演出や補間移動が終わるまで各フェーズで待つ。
    enum class Phase
    {
        PlayerTurn,          // プレイヤーの入力と行動を受け付ける。
        EnemyAction,         // 敵/仲間の攻撃・特技を1体ずつ処理する。
        EnemyMove,           // 敵/仲間の移動を処理する。
        EnemyPostMoveAction, // 倍速/三倍速など、移動後に攻撃できるユニットを処理する。
        MoveResolution       // StartMove の補間移動が終わるまで待つ。
    };

    static TurnManager* instance;

    // 現在どの処理段階で止まっているか。Update() はこの値を見て少しずつ進める。
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
    // 風ターンの警告は同じ残りターンで一度だけ表示する。
    bool m_WindWarning200Shown = false;
    bool m_WindWarning100Shown = false;
    bool m_WindWarning50Shown = false;

    bool HandleWindTurnLimit();
    void StartWindGameOver();

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

    void ResetDungeonState()
    {
        m_Phase = Phase::PlayerTurn;
        m_TurnCount = 0;
        m_ShopTheftMode = false;
        m_EnemyActionLoopHadProgress = false;
        m_IsPaused = false;
        m_WindWarning200Shown = false;
        m_WindWarning100Shown = false;
        m_WindWarning50Shown = false;
    }
    // プレイヤー入力待ちへ戻る時の初期化。
    void StartPlayerTurn();
    // プレイヤー行動後、敵/仲間へターン予算を配って敵行動フェーズへ入る。
    void StartEnemyTurn();
    // 全ユニットの処理が終わった時、ターン数・風制限・自然湧きを処理する。
    void FinishTurnCycle();
    // 現在ターンとフロア設定を見て、自然湧きする敵を生成する。
    void SpawnEnemy();
    void SetShopTheftMode(bool enabled) { m_ShopTheftMode = enabled; }
    bool IsShopTheftMode() const { return m_ShopTheftMode; }
    void PauseTurnProgression() { m_IsPaused = true; }
    void ResumeTurnProgression() { m_IsPaused = false; }

    void Update() override;
    // Shiftダッシュのような即時移動後、敵ターンを入力待ちまでまとめて解決する。
    void ResolveAfterPlayerInstantMove();

    int GetTurnCount() const { return m_TurnCount; }
};
