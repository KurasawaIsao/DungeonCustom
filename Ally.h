#pragma once
#include "Unit.h"
#include "EnemyData.h"
#include "UnitAI.h"
#include <vector>
#include <memory>

enum class AllyAIMode
{
    Follow,
    Counter,
    Wait,
    Patrol,
    NoSkill,
    Retreat
};

// Ally は味方勢力の Unit。
// 勧誘された Enemy のデータを引き継ぎつつ、プレイヤー追従、AI モード切替、会話、味方死亡処理を担当する。
// 戦闘・移動フェーズの骨格は Enemy と近いため、勢力差分だけを残す形への分離対象。
class Ally : public Unit
{
private:

    std::string m_TalkMessage[3];
    EffectBillboard* m_AllyMark = nullptr;
    std::unique_ptr<class ChaseAI> chaseAI;
    std::unique_ptr<class RunAwayAI> runAwayAI;
    std::unique_ptr<class BasicPatrolAI> patrolAI;
    AllyAIMode m_AIMode = AllyAIMode::Follow;
    bool m_PlayerRecognized = false;
    bool m_IsLostPatrolling = false;
    static constexpr int kPlayerRecognizeRange = 10;
    static constexpr int kPlayerRecognizedBonusRange = 5;

public:
    Ally();
    virtual ~Ally();

    void Init() override;
    void Draw() override;
    void Update() override;
    void Uninit() override;
    void OnDeath(Unit* attacker = nullptr) override;

    void LevelUp();

    void Attack()override;

    void UpdateUnit() override;
    void UpdateActionPhase();
    void UpdateMovePhase();
    void EndTurn();

    // 敵のデータからステータスやモデルをコピーする
    void InitFromEnemy(class Enemy* source);
    void Talk();
    AllyAIMode GetAIMode() const { return m_AIMode; }
    const char* GetAIModeName() const;
    void SetAIMode(AllyAIMode mode);

private:
    bool CanRecognizePlayer(class Player* player, class MapData* map);
    bool CanKeepRecognizedPlayer(class Player* player, class MapData* map);
    bool UpdatePlayerRecognition(class Player* player, class MapData* map);
    class Enemy* FindVisibleEnemy(class MapData* map);
};