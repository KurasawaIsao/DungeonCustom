#pragma once
#include "Unit.h"
#include "Room.h"
#include <string>
#include <memory>
#include "EnemyData.h"
class PatrolAI;

class UnitAI;
enum class EnemyState
{
    Patrol,     // 巡回
    Chase       // プレイヤー追跡
};

// Enemy は敵勢力の Unit。
// EnemyData から初期化され、敵専用の AI 選択、索敵対象、ドロップ/勧誘判定、死亡処理を担当する。
// 追従位置決めやターゲット接近など Ally と同型の移動処理は、今後の共通化候補として扱う。
class Enemy : public Unit
{
private:
   //Unitにあるものを継承先で書かないで...

 
    EnemyData m_Data;
    // ターン中に敵対対象を見つけたかを記録し、状態遷移や一時的な索敵停止に使う。
    bool m_TargetRecognized = false;
    bool m_SuppressHostileRecognitionThisTurn = false;

    bool m_IsDead = false;
    int m_ExpReward;
    int m_RecruitmentModifier;
    bool m_IsShopKeeper = false;
    bool m_IsShopHostile = false;
    bool m_EditorPreviewOnly = false;
    bool m_ConsumeTurnAfterNapConditionWake = false;
    bool m_RecruitByPlayerNormalAttack = false;

    // 攻撃アニメーションのNotifyまで、対象と事前に確定した命中結果を保持する。
    Unit* m_PendingAttackTarget = nullptr;
    bool m_PendingAttackHit = false;
    // 特技アニメーションのNotifyまで、効果と対象情報を保持する。
    std::shared_ptr<EffectBase> m_PendingSkillEffect;
    EffectContext m_PendingSkillContext;
    std::vector<Unit*> m_PendingSkillTargets;
  

public:
    Enemy();
    virtual ~Enemy();

    void Init() override;
    void Draw() override;
    void Update()override;
    void Uninit() override;
    void OnDeath(Unit* attacker = nullptr) override;

    void Attack()override;
    // UnitAIから選択済みの特技を受け取り、SkillEffect Notifyまで効果適用を待機する。
    bool QueueSkillForNotify(
        const Skill& skill,
        const EffectContext& context,
        const std::vector<Unit*>& targets);
    // 混乱攻撃を含む敵の通常攻撃を、AttackHit Notifyへ接続する。
    void StartAttackWithNotify(Unit* target);

    virtual void UpdateUnit() override;
    void UpdateActionPhase();
    void UpdateMovePhase();

    bool UpdateNap();
    void ClearNap(bool consumeTurn = false);
    void DecideNextAction();
    void ApplyData(const EnemyData& d);
    int GetExpReward() { return m_ExpReward; }
  

    EnemyData GetEnemyData() const { return m_Data; }
    void SetEditorPreviewOnly(bool previewOnly) { m_EditorPreviewOnly = previewOnly; }
    void SetShopKeeperMode(bool hostile);
    void ChangeAI(EnemyAIType aiType);
    bool IsShopKeeper() const { return m_IsShopKeeper; }
    bool IsShopHostile() const { return m_IsShopHostile; }
    // プレイヤーの通常攻撃によるダメージ中かどうかを設定し、勧誘判定に利用する。
    void SetPlayerNormalAttackDamage(bool damaged) { m_RecruitByPlayerNormalAttack = damaged; }
    // 現在の敵ターン中に索敵を抑制するか設定する。
    void SetHostileRecognitionSuppressed(bool suppressed);

protected:
    // AnimationModelから届いた攻撃・特技通知を、保留中のゲーム処理へ変換する。
    void OnAnimationNotify(const std::string& animationName, const std::string& notifyName) override;

private:
    void ResolvePendingAttack();
    void ResolvePendingSkill();
    void ClearPendingCombatActions();
    void DropItem();
    // 描画とアニメ更新で同じ視界判定を使い、視界外の敵処理をまとめて省く。
    bool IsVisibleForPlayerUpdate(class Player* player) const;
    // プレイヤーと仲間から、現在の部屋/視界ルールで見えている最寄りの敵対対象を探す。
    Unit* FindVisibleHostileTarget(const char* context = "Unknown");
    // 巡回中の向き更新用に、プレイヤーを除いた敵対対象だけを探す。
    Unit* FindVisibleNonPlayerHostileTarget();
    // 単体の敵対対象が現在の部屋/視界ルールに入っているか調べる。
    bool CanRecognizeHostileTarget(Unit* target, class MapData* map);
    void ClearTargetRecognition();
    void ReturnToPatrolFromCurrentPos();
    void ResetAI(EnemyAIType aiType);
    EnemyState m_State = EnemyState::Patrol;

    std::unique_ptr<UnitAI> m_PatrolAI;
    std::unique_ptr<UnitAI> m_ChaseAI;
    UnitAI* m_CurrentAI = nullptr;

};
