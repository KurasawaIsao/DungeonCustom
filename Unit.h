#pragma once
#include "GameObject.h"
#include "Vector2Int.h"
#include "MessageLog.h"
#include <string>
#include "SkillData.h"
#include "MapData.h"

enum class MoveState
{
    Idle,
    Moving,      
    WarpFloating, 
    Knockback,
    KnockbackImpact,
    Summoning
};

enum class Status {
    None,
    Confusion,  
    Sleep,    
    Paralysis,
    Nap,
    Poison
};

enum class TurnSpeed {
    Slow,
    Normal,
    Fast,
    Triple
};

enum class TurnConsumeType {
    Action,
    Move
};
// Unit はプレイヤー/敵/仲間に共通する「盤面上の戦闘ユニット」の土台。
// HP/能力値/状態異常/ターン予算/グリッド移動/アニメーションなど、勢力に依存しない処理だけを持つ。
// どの相手を敵とみなすか、どんな AI 方針で動くか、死亡時に何を残すかは派生クラス側の責務。
class Unit : public GameObject
{
private:
    static bool s_SkipMoveAnimation;

protected:
    ID3D11VertexShader* m_VertexShader;
    ID3D11PixelShader* m_PixelShader;
    ID3D11InputLayout* m_VertexLayout;

    ID3D11VertexShader* m_OutlineVertexShader = nullptr;
    ID3D11PixelShader* m_OutlinePixelShader = nullptr;
    ID3D11InputLayout* m_OutlineVertexLayout = nullptr;

    void InitToonShader();
    void ReleaseToonShader();
    void DrawToonModel(XMMATRIX world);


    std::string m_AnimNow = "Idle";
    std::string m_AnimNext = "Idle";


    float m_Frame = 0.0f;

    float m_AnimSpeedCnt = 0.0f;
    float m_AnimSpeed = 0.5f;
    float m_AnimSpeedCntMax = 1.0f; // 1.0 = 毎フレーム進む
    float m_AnimationBlend = 0.0f;

    class AnimationModel* m_AnimationModel = nullptr;

    MoveState m_MoveState = MoveState::Idle;

    Vector2Int m_MoveTarget;
    Vector2Int m_MoveStartGridPos;
    Vector3 m_MoveStartPos;
    Vector3 m_MoveEndPos;

    float m_MoveTimer = 0.0f;
    float m_MoveDuration = 0.1f;
    Vector3 m_VisualRotationOffset{ 0.0f, 0.0f, 0.0f };
    int m_KnockbackImpactDamage = 0;
    Unit* m_KnockbackImpactAttacker = nullptr;
    Unit* m_KnockbackCollisionUnit = nullptr;
    int m_KnockbackCollisionDamage = 0;
    bool HasKnockbackImpactDamage() const;
    void ApplyKnockbackImpactDamage();
    void ClearKnockbackImpactDamage();

protected:
    std::string m_Name = u8"ユニット";
    // ===== ステータス =====
    int m_HP = 30;
    int m_ATK = 5;
    int m_DEF = 4;
    int m_ACC = 95;
    int m_EVD = 10;

    int m_MaxHP = 30;
    int m_MaxATK = 5;
    int m_MaxDEF = 6;
    int m_MaxACC = 95;
    int m_MaxEVD = 10;

    int m_Level = 1;
    int m_Exp = 0;
    int m_ExpToNext = 10; // 次レベル必要経験値

    int m_RegenCounter = 0;
    int m_RegenConst = 50; // 50ターンで全回復

    std::vector<Skill> m_Skills;

    Status m_Status = Status::None;
    int m_StatusDuration = 0; // 状態異常の残りターン数
    bool m_BlockActionOnceAfterStatusClear = false;

    class EffectBillboard* m_LoopEffect = nullptr;
    // ===== 座標 =====
    Vector2Int m_GridPos;   // マップ上のグリッド座標
    Vector2Int m_LastValidGridPos = { 0, 0 };
    Vector2Int m_CurrentDir;
    Vector2Int m_FacingDir = Vector2Int(0, 1);
    bool m_HasActed = false;   // このターンで行動済みか
    // ターン制の中心データ。
    // Action は攻撃/特技、Move は移動を表し、TurnManager がフェーズごとに別々に消費する。
    // Slow は 2 ターンに 1 回、Normal は 1 回、Fast は 2 回、Triple は 3 回相当になる。
    TurnSpeed m_BaseActionSpeed = TurnSpeed::Normal;
    TurnSpeed m_BaseMoveSpeed = TurnSpeed::Normal;
    TurnSpeed m_ActionSpeed = TurnSpeed::Normal;
    TurnSpeed m_MoveSpeed = TurnSpeed::Normal;

	// 現在のターンの消費タイプ。Action の場合はターン終了時に行動回数を消費し、Move の場合は移動回数を消費する。
    TurnConsumeType m_TurnConsumeType = TurnConsumeType::Action;
    int m_ActionEnergy = 0;
    int m_MoveEnergy = 0;
    int m_ActionBudget = 1;
    int m_MoveBudget = 1;
    bool m_ActionPhaseChecked = false;
    bool m_MovePhaseChecked = false;

    bool m_ConsumeActionOnNextTurn = false;
    bool m_ConsumeMoveOnNextTurn = false;
    bool m_ActionConsumedByMoveThisTurn = false;
    bool m_MovedThisTurn = false;
    bool m_IsAnimatingMove = false; 
    bool m_IsVisible = true;//透明状態だとかに使えるかも？
    bool m_IsActingAnimation = false; // 演出中フラグ
    float yoffset;

    bool m_IsAnimLooping = true; // 現在のアニメーションがループか単発か
    std::string m_DefaultAnim = "Idle"; 

public:

    virtual void OnTurnStart()
    {
        BeginTurnActions();
    }

    virtual void UpdateUnit() = 0;  

    void ResetTurn()
    {
        m_HasActed = false;
        m_ActionBudget = 1;
        m_MoveBudget = 1;
        m_ActionPhaseChecked = false;
        m_MovePhaseChecked = false;
    }
  
    virtual void EndTurn()
    {
        if (m_TurnConsumeType == TurnConsumeType::Move) ConsumeMove();
        else ConsumeAction();
    }
    bool HasActed() const
    {
        return m_HasActed;
    }
    bool CanActThisTurn() const { return m_ActionBudget > 0 && !m_HasActed; }
    bool CanMoveThisTurn() const { return m_MoveBudget > 0; }
    int GetActionBudget() const { return m_ActionBudget; }
    int GetMoveBudget() const { return m_MoveBudget; }
    void ConsumeAction()
    {
        if (m_ActionBudget > 0) --m_ActionBudget;
        m_HasActed = (m_ActionBudget <= 0);
    }
	// 移動のために行動回数を消費する。
    // 倍速移動などで移動だけする場合もあるため、行動回数が1以上残っている場合のみ消費する。
    bool ConsumeActionForMoveIfPossible()
    {
        if (m_ActionBudget <= 1) return false;
		//行動回数が2以上残っているので行動回数を1消費して移動する。
        ConsumeAction();
        m_ActionConsumedByMoveThisTurn = true;
        return true;
    }
    bool HasActionConsumedByMoveThisTurn() const { return m_ActionConsumedByMoveThisTurn; }
    bool HasMovedThisTurn() const { return m_MovedThisTurn; }

	// 行動回数を1消費する。行動回数が0になるとターンは終了するが、
    // 倍速移動などで行動回数を消費せずに移動だけする場合もあるので、行動回数が0だからといって必ずターン終了とは限らない。移動回数は関係ない。  
    void ConsumeMove()
    {
        if (m_MoveBudget > 0) --m_MoveBudget;
    }
	//行動回数をすべて使い切る。行動回数を使い切るとターンは終了する。移動回数は残っていても関係ない。
    void ConsumeAllActions()
    {
        m_ActionBudget = 0;
        m_HasActed = true;
    }
    //移動回数をすべて使い切る。あくまで行動回数を使い切るわけではない。
    void ConsumeAllMoves()
    {
        m_MoveBudget = 0;
    }
    void BeginTurnActions();
    int GetTurnSpeedEnergy(TurnSpeed speed) const
    {
        // 2エネルギーで1回行動できる計算にする。
        // Slow=1 なので2ターンで1回、Fast=4 なので1ターンで2回、Triple=6 なので3回。
        switch (speed)
        {
        case TurnSpeed::Slow: return 1;
        case TurnSpeed::Fast: return 4;
        case TurnSpeed::Triple: return 6;
        case TurnSpeed::Normal:
        default: return 2;
        }
    }
    void SetTurnSpeed(TurnSpeed speed) { m_ActionSpeed = speed; m_MoveSpeed = speed; }
    void SetActionSpeed(TurnSpeed speed) { m_ActionSpeed = speed; }
    void SetMoveSpeed(TurnSpeed speed) { m_MoveSpeed = speed; }
    void SetBaseTurnSpeed(TurnSpeed speed) { m_BaseActionSpeed = speed; m_BaseMoveSpeed = speed; SetTurnSpeed(speed); }
    void SetBaseActionSpeed(TurnSpeed speed) { m_BaseActionSpeed = speed; m_ActionSpeed = speed; }
    void SetBaseMoveSpeed(TurnSpeed speed) { m_BaseMoveSpeed = speed; m_MoveSpeed = speed; }
    void ResetTurnSpeedToBase() { SetTurnSpeed(m_BaseActionSpeed); m_MoveSpeed = m_BaseMoveSpeed; m_ActionEnergy = 0; m_MoveEnergy = 0; }
    TurnSpeed GetActionSpeed() const { return m_ActionSpeed; }
    TurnSpeed GetMoveSpeed() const { return m_MoveSpeed; }

	// 行動フェーズと移動フェーズのチェック状態を管理する。
    // これらは TurnManager がフェーズごとに呼び出す UpdateActionPhase と UpdateMovePhase 内で、行動可能かどうかを判断するために使う。
    bool IsActionPhaseChecked() const { return m_ActionPhaseChecked; }
    bool IsMovePhaseChecked() const { return m_MovePhaseChecked; }
    void MarkActionPhaseChecked() { m_ActionPhaseChecked = true; }
    void MarkMovePhaseChecked() { m_MovePhaseChecked = true; }
    void ResetActionPhaseCheck() { m_ActionPhaseChecked = false; }
    void ResetMovePhaseCheck() { m_MovePhaseChecked = false; }

	// ターンの消費タイプを設定する。ここの設定は各敵AIが個別に判断し、
    // 倍速で移動し攻撃する敵がいない場合はMove、といった運用法
    void SetTurnConsumeType(TurnConsumeType type) { m_TurnConsumeType = type; }
    void ReserveActionConsumedOnNextTurn() { m_ConsumeActionOnNextTurn = true; }
    void ReserveMoveConsumedOnNextTurn() { m_ConsumeMoveOnNextTurn = true; }


    // ===== 座標管理 =====
    void SetInitGridPos(const Vector2Int& g);
    void SetGridPos(const Vector2Int& g);
    bool RepairInvalidGridPos(const char* context);
    const Vector2Int& GetGridPos() const { return m_GridPos; }
    const Vector2Int& GetMoveStartGridPos() const { return m_MoveStartGridPos; }
    void SetCurrentDir(const Vector2Int& g) { m_CurrentDir = g; }

    void UpdateFacingRotation();
    void LookAt(const Vector2Int& targetGrid);
    const Vector2Int& GetFacingDir() const { return m_FacingDir; }

    void SetWorldPosition(const Vector3& w);
    const Vector3& GetWorldPosition() const { return m_Position; }
    Vector3 GetVisualPosition() const { return m_Position; }
    Vector3 GetVisualRotation() const { return m_Rotation + m_VisualRotationOffset; }

    // グリッド → ワールド変換（タイル幅 2.0f を基準とする）
    inline Vector3 GridToWorld(const Vector2Int& grid)
    {
        return Vector3(grid.x * TILE_DISTANCE, 0.0f, grid.y * TILE_DISTANCE);
    }

    // ===== ステータス管理 =====
    void SetName(const std::string& name) { m_Name = name; }
    const std::string& GetName() const { return m_Name; }

    bool IsDead() const { return m_HP <= 0; }
    int GetHP() const { return m_HP; }
    void SetHP(int value) { m_HP = value; }
    virtual int GetDEF() const { return m_DEF; }
    virtual void SetDEF(int value) { m_DEF = value; }
    virtual int GetATK() const { return (m_Status == Status::Poison) ? ((m_ATK + 1) / 2) : m_ATK; }
    virtual void SetATK(int value) { m_ATK = value; }
    int GetACC() const { return m_ACC; }
    void SetACC(int value) { m_ACC = value; }
    int GetEVD() const { return m_EVD; }
    void SetEVD(int value) { m_EVD = value; }

    int GetMaxHP() const { return m_MaxHP; }
    void SetMaxHP(int value) { m_MaxHP = value; }
    int GetMaxDEF() const { return m_MaxDEF; }
    void SetMaxDEF(int value) { m_MaxDEF = value; }
    int GetMaxATK() const { return m_MaxATK; }
    void SetMaxATK(int value) { m_MaxATK = value; }
    int GetMaxACC() const { return m_MaxACC; }
    void SetMaxACC(int value) { m_MaxACC = value; }
    int GetMaxEVD() const { return m_MaxEVD; }
    void SetMaxEVD(int value) { m_MaxEVD = value; }

    const std::vector<Skill>& GetSkills() const { return m_Skills; }
    void AddSkill(const Skill& skill) { m_Skills.push_back(skill); }

    int GetLevel() const { return m_Level; }
    int GetExp() const { return m_Exp; }
    float GetYoffset() { return yoffset; }

    virtual void AddExp(int value)
    {
        m_Exp += value;

        while (m_Exp >= m_ExpToNext)
        {
            m_Exp -= m_ExpToNext;
            LevelUp();
        }
    }
    virtual void LevelUp()
    {
        m_Level++;

        m_MaxHP += 5;
        m_MaxATK += 2;
        m_ATK = m_MaxATK;
        m_MaxDEF += 2;
        m_DEF = m_MaxDEF;
        

        m_HP = m_MaxHP;

        m_ExpToNext += 5 + 3* GetLevel();

        MessageLog::Instance().AddMessage(
            u8"レベルが上がった！ Lv." + std::to_string(m_Level)
        );
    }

    void NaturalRecovery();
    void SetVisible(bool vis) { m_IsVisible = vis; }

    // エフェクトを発生させる
    void PlayLoopEffect(const char* fileName, int rows, int cols, float speed);
    void StopLoopEffect();

    void TakeDamage(int damage, Unit* attacker = nullptr);
    void ConstantDamage(int value, Unit* attacker = nullptr, bool waitForAnimation = true);

    void SetStatus(Status effect, int duration, Unit* source = nullptr);
    Status GetStatus() const { return m_Status; }

    // 毎ターン開始時に状態異常のカウントを下げる
    bool UpdateStatusCount();
    bool ConsumeActionBlockAfterStatusClear();

    void ClearStatus();

    virtual void OnDeath(Unit* attacker =nullptr) = 0;



    bool CheckHit(int acc, int evd);

    int CalcDamage(int atk,int def,float atkRate = 1.0f,float defRate = 1.0f);

    void Heal(int val)
    {
        m_HP += val;
        if (m_HP >= m_MaxHP) { m_HP = m_MaxHP; }
    }

    // ===== 移動補助 =====
    bool IsDiagonalMoveBlocked(Vector2Int cur, Vector2Int dir, MapData* map);

    void PlayAnimation(const std::string& animName, float m_AnimSpeed);
    void UpdateAnimation();
    void SetTriggerAnimation(const std::string& animName, float speed = 1.0f, bool waitForAnimation = true);

    void StartMove(const Vector2Int& target, float moveTime);
    void StartKnockback(const Vector2Int& target, int impactDamage = 0, Unit* attacker = nullptr, Unit* collisionUnit = nullptr, int collisionDamage = 0);
    void StartSummonAppear(float duration = 0.45f);
    void StartWarp(const Vector2Int& targetGrid);
    static void SetSkipMoveAnimation(bool skip) { s_SkipMoveAnimation = skip; }
    static bool IsSkipMoveAnimation() { return s_SkipMoveAnimation; }

    void UpdateLerpMove();
    MoveState GetMoveState(){ return m_MoveState; };
    bool IsAnimatingMove() const { return m_IsAnimatingMove; }
    bool IsActing() const { return m_IsActingAnimation; }
    bool ShouldShowCombatVisual(Unit* other = nullptr) const;

    //ターン終了時の行動を分けたい場合にvirtual
    virtual void OnMoveFinished()
    {
        PlayAnimation("Idle", 1.0f);
        m_MoveState = MoveState::Idle;
        EndTurn();
    }
    virtual void Attack() = 0;

    //行動用API
    void RequestMove(const Vector2Int& target);
    bool RequestMoveBool(const Vector2Int& target);
    bool IsAnimationFinished() const;
};
