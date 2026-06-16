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
    // 各AIは命令変更のたびに作り直さず、仲間が生存している間は同じインスタンスを再利用する。
    std::unique_ptr<class ChaseAI> m_ChaseAI;
    std::unique_ptr<class RunAwayAI> m_RunAwayAI;
    std::unique_ptr<class BasicPatrolAI> m_PatrolAI;
    // 現在の移動判断に使用するAI。所有権は上記のunique_ptrが持つ。
    UnitAI* m_CurrentAI = nullptr;
    AllyAIMode m_AIMode = AllyAIMode::Follow;
    // 特技の使用可否は移動AIとは別の命令設定として管理する。
    bool m_CanUseSkill = true;
    bool m_PlayerRecognized = false;
    bool m_IsLostPatrolling = false;

    // 攻撃・特技アニメーションのNotifyまで、選択済みのゲーム処理を保持する。
    Unit* m_PendingAttackTarget = nullptr;
    bool m_PendingAttackHit = false;
    std::shared_ptr<EffectBase> m_PendingSkillEffect;
    EffectContext m_PendingSkillContext;
    std::vector<Unit*> m_PendingSkillTargets;
    static constexpr int kPlayerRecognizeRange = 10;
    static constexpr int kPlayerRecognizedBonusRange = 5;

public:
    Ally();
    virtual ~Ally();

    void Init() override;
    void Draw() override;
    void Update() override;
    void Uninit() override;
    void OnDeath(Unit* = nullptr) override;

    void LevelUp();

    void Attack()override;
    // UnitAIから選択済みの特技を受け取り、SkillEffect Notifyまで効果適用を待機する。
    bool QueueSkillForNotify(
        const Skill& skill,
        const EffectContext& context,
        const std::vector<Unit*>& targets);
    // 混乱攻撃を含む仲間の通常攻撃を、AttackHit Notifyへ接続する。
    void StartAttackWithNotify(Unit* target);

    void UpdateUnit() override;
    void UpdateActionPhase();
    void UpdateMovePhase();
    void EndTurn();

    // 敵のデータからステータスやモデルをコピーする
    void InitFromEnemy(class Enemy* source);
    void Talk();
    const char* GetAIModeName() const;
    void SetAIMode(AllyAIMode mode);
    void DismissFromParty();

protected:
    // AnimationModelから届いた攻撃・特技通知を、保留中のゲーム処理へ変換する。
    void OnAnimationNotify(const std::string& animationName, const std::string& notifyName) override;

private:
    void ResolvePendingAttack();
    void ResolvePendingSkill();
    void ClearPendingCombatActions();
    // 保持済みAIの中から、現在使用するAIだけを切り替える。
    void SwitchAI(UnitAI* nextAI);
    // 仲間命令に対応する基本AIを返す。状況による一時切替は移動フェーズ側で行う。
    UnitAI* GetCommandAI() const;
    bool CanRecognizePlayer(class Player* player, class MapData* map);
    bool CanKeepRecognizedPlayer(class Player* player, class MapData* map);
    bool UpdatePlayerRecognition(class Player* player, class MapData* map);
    // 隣接している敵のうち、仲間が攻撃対象にできる相手だけを返す。
    class Enemy* FindAdjacentHostileEnemy();
    class Enemy* FindVisibleEnemy(class MapData* map);
};