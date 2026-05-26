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
   // class AnimationModel* m_AnimationModel;

 
    EnemyData m_Data;
    // ターン中に敵対対象を見つけたかを記録し、状態遷移や一時的な索敵停止に使う。
    bool m_TargetRecognized = false;
    bool m_SuppressHostileRecognitionThisTurn = false;

    bool IsDead = false;
    int m_ExpReward;
    int m_RecruitmentModifier;
    bool m_IsShopKeeper = false;
    bool m_IsShopHostile = false;
    bool m_ConsumeTurnAfterNapConditionWake = false;
    bool m_RecruitByPlayerNormalAttack = false;
  

public:
    Enemy();
    virtual ~Enemy();

    void Init() override;
    void Draw() override;
    void Update()override;
    void Uninit() override;
    void OnDeath(Unit* attacker = nullptr) override;

    void Attack()override;

    virtual void UpdateUnit() override;
    void UpdateActionPhase();
    void UpdateMovePhase();

    bool UpdateNap();
    void ClearNap(bool consumeTurn = false);
    void DecideNextAction();
    void ApplyData(const EnemyData& d);
    int GetExpReward() { return m_ExpReward; }
  

    EnemyData GetEnemyData() const { return m_Data; }
    void SetShopKeeperMode(bool hostile);
    void ChangeAI(EnemyAIType aiType);
    bool IsShopKeeper() const { return m_IsShopKeeper; }
    bool IsShopHostile() const { return m_IsShopHostile; }
    void MarkPlayerNormalAttackDamage() { m_RecruitByPlayerNormalAttack = true; }
    void ClearPlayerNormalAttackDamage() { m_RecruitByPlayerNormalAttack = false; }
    void SuppressHostileRecognitionThisTurn();
    void ClearHostileRecognitionSuppression() { m_SuppressHostileRecognitionThisTurn = false; }

    int GetRecruitmentModifier() const { return m_RecruitmentModifier; }

private:
    void DropItem();
    // プレイヤーと仲間から、現在の部屋/視界ルールで見えている最寄りの敵対対象を探す。
    Unit* FindVisibleHostileTarget(const char* context = "Unknown");
    // 巡回中の向き更新用に、プレイヤーを除いた敵対対象だけを探す。
    Unit* FindVisibleNonPlayerHostileTarget();
    // 単体の敵対対象が現在の部屋/視界ルールに入っているか調べる。
    bool CanRecognizeHostileTarget(Unit* target, class MapData* map);
    void ClearTargetRecognition();
    void ReturnToPatrolFromCurrentPos();
    void ResetAI(EnemyAIType aiType);
    EnemyState state = EnemyState::Patrol;

    std::unique_ptr<UnitAI> patrolAI;
    std::unique_ptr<UnitAI> chaseAI;
    UnitAI* currentAI = nullptr;

};
