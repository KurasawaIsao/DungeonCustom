#include "Ally.h"
#include "UnitManager.h"
#include "MapManager.h"
#include "MapData.h"
#include "AnimationModel.h"
#include "Renderer.h"
#include "MessageLog.h"
#include "Enemy.h"
#include "ChaseAI.h"
#include "RunAwayAI.h"
#include "BasicPatrolAI.h"
#include "Player.h"
#include "TurnManager.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include "EffectBillboard.h"
#include "EffectManager.h"
#include "UnitMovementPlanner.h"

// Ally は味方ユニット本体。
// 敵と同じ Unit のターン予算を使うが、目標は「敵」、行動方針は AllyAIMode で切り替える。
// TurnManager 上では敵側フェーズに一緒に参加し、Action -> Move の順で処理される。

namespace
{
    TurnSpeed ToTurnSpeed(EnemyTurnSpeedType speed)
    {
        switch (speed)
        {
        case EnemyTurnSpeedType::Slow: return TurnSpeed::Slow;
        case EnemyTurnSpeedType::Fast: return TurnSpeed::Fast;
        case EnemyTurnSpeedType::Triple: return TurnSpeed::Triple;
        case EnemyTurnSpeedType::Normal:
        default: return TurnSpeed::Normal;
        }
    }

    bool IsAllyHostileEnemy(Enemy* enemy)
    {
        if (!enemy || enemy->GetHP() <= 0) return false;
        if (enemy->IsShopKeeper() && !TurnManager::Instance()->IsShopTheftMode()) return false;
        return true;
    }
}

Ally::Ally() {}
Ally::~Ally() {}

void Ally::Init()
{
    m_AnimationModel = new AnimationModel();
    m_AnimNow = "Idle";
    m_AnimNext = "Idle";
    m_AnimSpeed = 0.5f;
    m_AnimSpeedCnt = 0.0f;
    m_AnimSpeedCntMax = 1.0f;
    m_Frame = 0;
    m_Scale = { 0.5f,0.5f,0.5f };


    chaseAI = std::make_unique<ChaseAI>();
    runAwayAI = std::make_unique<RunAwayAI>();
    patrolAI = std::make_unique<BasicPatrolAI>();

    InitToonShader();

    PlayAnimation("Idle", 0.5f);
    m_AllyMark= EffectManager::CreateSpriteEffect(m_Position, "Asset\\Texture\\Heart.png");
    m_AllyMark->SetScale({ 0.5f, 0.5f, 0.5f });
    for (int i = 0; i < 3; i++)
    {
        m_TalkMessage[i] = "a";
    }
}

void Ally::InitFromEnemy(Enemy* source) {
    if (!source) return;

    // Enemyが持っているEnemyDataを取得
    const EnemyData& d = source->GetEnemyData();

    // 敵データの最大能力値をそのまま引き継ぎ、仲間化で火力や防御が落ちないようにする。
    m_MaxHP = source->GetMaxHP();
    m_MaxATK = source->GetMaxATK();
    m_MaxDEF = source->GetMaxDEF();
    m_MaxACC = source->GetMaxACC();
    m_MaxEVD = source->GetMaxEVD();
    m_HP = m_MaxHP;
    m_ATK = m_MaxATK;
    m_DEF = m_MaxDEF;
    m_ACC = m_MaxACC;
    m_EVD = m_MaxEVD;
    m_Name = source->GetName();
    SetBaseActionSpeed(ToTurnSpeed(d.actionSpeed));
    SetBaseMoveSpeed(ToTurnSpeed(d.moveSpeed));

    // ビジュアル設定
    m_Scale = d.visual.scale;
    yoffset = d.visual.yOffset;

    if (!m_AnimationModel) m_AnimationModel = new AnimationModel();
    m_AnimationModel->Load(d.visual.modelPath.c_str());
    m_AnimationModel->LoadAnimation(d.visual.animIdle.c_str(), "Idle");
    m_AnimationModel->LoadAnimation(d.visual.animRun.c_str(), "Run");
    m_AnimationModel->LoadAnimation(d.visual.animAttack.c_str(), "Attack");
    m_AnimationModel->LoadAnimation(d.visual.animDamaged.c_str(), "Damaged");
    m_AnimationModel->LoadAnimation(d.visual.animSkill.c_str(), "Skill");
    m_AnimationModel->LoadAnimation(d.visual.animSleep.c_str(), "Sleep");

    for (int i = 0; i < 3;i++)
    {
        m_TalkMessage[i] = d.talkMessage[i];
    }
   

    // スキル引き継ぎ
    this->m_Skills = source->GetSkills();
}

void Ally::Draw()
{
    if (!m_AnimationModel)return;

    Vector3 visualPos = GetVisualPosition();
    Vector3 visualRot = GetVisualRotation();
    XMMATRIX world, trans, rot, scale;
    scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    rot = XMMatrixRotationRollPitchYaw(visualRot.x, visualRot.y, visualRot.z);
    trans = XMMatrixTranslation(visualPos.x, visualPos.y + 1.3f, visualPos.z);
    world = scale * rot * trans;

    DrawToonModel(world);
}
void Ally::Update()
{
    if (m_AllyMark)
    {
        Vector3 effectPos = m_Position;
        effectPos.y += 2.0f;
        m_AllyMark->SetPosition(effectPos);
    }
    if (m_LoopEffect)
    {
        Vector3 effectPos = m_Position;
        effectPos.y += 2.0f;
        m_LoopEffect->SetPosition(effectPos);
    }

    if (m_MoveState == MoveState::Moving)
    {
        Vector2Int moveDir = m_MoveTarget - m_GridPos;
        if (moveDir.x != 0 || moveDir.y != 0) m_FacingDir = moveDir;
    }
    else
    {
        MapData* map = MapManager::Instance()->GetCurrentMap();
        const bool patrolFacing = (m_AIMode == AllyAIMode::Patrol || m_IsLostPatrolling);
        if (patrolFacing) {
            if (Enemy* enemy = FindVisibleEnemy(map)) {
                m_FacingDir = enemy->GetGridPos() - GetGridPos();
            }
        }
        else {
            auto enemies = UnitManager::Instance()->GetAdjacentEnemies(*this);
            Enemy* enemyTarget = nullptr;
            for (Enemy* enemy : enemies) {
                if (IsAllyHostileEnemy(enemy)) {
                    enemyTarget = enemy;
                    break;
                }
            }
            if (enemyTarget) {
                m_FacingDir = enemyTarget->GetGridPos() - GetGridPos();
            }
            else {
                Player* p = UnitManager::Instance()->GetPlayer();
                if (p) m_FacingDir = p->GetGridPos() - GetGridPos();
            }
        }
    }

    UpdateFacingRotation();
    this->LookAt(m_FacingDir);
    UpdateAnimation();
    // 攻撃・特技・被ダメージの単発演出が終わったら、次の行動へ進める状態に戻す。
    if (m_IsActingAnimation && (m_IsAnimLooping || IsAnimationFinished())) {
        m_IsActingAnimation = false;
    }
}


void Ally::UpdateUnit() {
    // 直接更新用入口。通常は TurnManager が Action/Move を分けて呼ぶ。
    UpdateActionPhase();
    if (!CanMoveThisTurn() || m_MoveState != MoveState::Idle) return;
    UpdateMovePhase();
}

void Ally::UpdateActionPhase()
{
    // 味方の攻撃/特技フェーズ。
    // 通常攻撃は隣接敵だけを対象にし、待機中の特技は移動せず視界内の敵にも使えるようにする。
    if (IsActionPhaseChecked()) return;
    MarkActionPhaseChecked();
    SetTurnConsumeType(TurnConsumeType::Action);
    if (!CanActThisTurn()) return;

    if (ConsumeActionBlockAfterStatusClear()) {
        ConsumeAllActions();
        ConsumeAllMoves();
        return;
    }

    Status statusBeforeCount = m_Status;
    bool statusCleared = UpdateStatusCount();
    if (statusCleared && (statusBeforeCount == Status::Sleep || statusBeforeCount == Status::Paralysis)) {
        ConsumeAllActions();
        ConsumeAllMoves();
        return;
    }
    if (m_Status == Status::Sleep || m_Status == Status::Paralysis) {
        ConsumeAllActions();
        ConsumeAllMoves();
        return;
    }

    if (m_Status == Status::Confusion) {
        MapData* map = MapManager::Instance()->GetCurrentMap();
        if (map) UnitAI::ExecuteConfusion(*this, map);
        else ConsumeAllActions();
        return;
    }

    if (m_AIMode == AllyAIMode::Retreat) {
        ConsumeAllActions();
        return;
    }
    auto enemies = UnitManager::Instance()->GetAdjacentEnemies(*this);
    Enemy* adjacentTarget = nullptr;
    for (Enemy* enemy : enemies) {
        if (IsAllyHostileEnemy(enemy)) {
            adjacentTarget = enemy;
            break;
        }
    }

    Enemy* skillTarget = adjacentTarget;
    if (!skillTarget && m_AIMode == AllyAIMode::Wait) {
        // 待機AIはその場から動かないため、視界内の敵を特技だけの候補として拾う。
        skillTarget = FindVisibleEnemy(MapManager::Instance()->GetCurrentMap());
    }

    if (m_AIMode != AllyAIMode::NoSkill && skillTarget && chaseAI && chaseAI->ExecuteSkill(*this, skillTarget)) {
        EndTurn();
        if (!CanActThisTurn()) ConsumeAllMoves();
        return;
    }

    if (!adjacentTarget) return;

    int budgetBefore = GetActionBudget();
    m_FacingDir = adjacentTarget->GetGridPos() - GetGridPos();
    Attack();
    if (GetActionBudget() == budgetBefore) EndTurn();
}
void Ally::UpdateMovePhase()
{
    // 味方の移動フェーズ。
    // プレイヤー追従、敵への接近/待機/退避などは AllyAIMode と各 AI クラスへ分岐する。
    if (IsMovePhaseChecked() || IsAnimatingMove()) return;
    MarkMovePhaseChecked();
    SetTurnConsumeType(TurnConsumeType::Move);
    if (!CanMoveThisTurn()) return;
    if (m_Status == Status::Sleep || m_Status == Status::Paralysis) {
        ConsumeAllMoves();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }
    if (m_AIMode == AllyAIMode::Wait) {
        ConsumeAllMoves();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }
    int moveBudgetBefore = GetMoveBudget();
    Vector2Int oldPos = GetGridPos();
    MapData* map = MapManager::Instance()->GetCurrentMap();
    Player* player = UnitManager::Instance()->GetPlayer();
    bool playerRecognized = UpdatePlayerRecognition(player, map);
    bool chaseMode = (m_AIMode == AllyAIMode::Follow || m_AIMode == AllyAIMode::Counter || m_AIMode == AllyAIMode::NoSkill);
    if (playerRecognized && m_IsLostPatrolling) {
        m_IsLostPatrolling = false;
        if (chaseAI) chaseAI->Reset();
        if (patrolAI && map) patrolAI->ResetFromCurrentPos(*this, map);
    }
    Enemy* visibleEnemy = FindVisibleEnemy(map);
    bool attackableEnemy = visibleEnemy && chaseAI && chaseAI->IsAttackAdjacent(*this, visibleEnemy, map);
    if (attackableEnemy && m_AIMode != AllyAIMode::Retreat) {
        ConsumeAllMoves();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }
    if (visibleEnemy && m_AIMode != AllyAIMode::Counter && m_AIMode != AllyAIMode::NoSkill && m_AIMode != AllyAIMode::Patrol) {
        visibleEnemy = nullptr;
    }
    if (m_AIMode == AllyAIMode::Patrol) {
        if (visibleEnemy && chaseAI && map) chaseAI->MoveOnlyWithTarget(*this, visibleEnemy, map);
        else if (patrolAI && map) patrolAI->Update(*this, map);
        else ConsumeAllMoves();
        if (oldPos != GetGridPos()) ConsumeActionForMoveIfPossible();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }
    if (!playerRecognized && chaseMode) {
        if (patrolAI && map) {
            if (!m_IsLostPatrolling) {
                if (chaseAI) chaseAI->Reset();
                patrolAI->ResetFromCurrentPos(*this, map);
                m_IsLostPatrolling = true;
            }
            patrolAI->Update(*this, map);
        }
        else {
            ConsumeAllMoves();
        }
        if (oldPos != GetGridPos()) ConsumeActionForMoveIfPossible();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }
    if (!playerRecognized && m_AIMode != AllyAIMode::Retreat) {
        ConsumeAllMoves();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }
    if (chaseAI && map && player) {
        switch (m_AIMode) {
        case AllyAIMode::Counter:
        case AllyAIMode::NoSkill:
            if (visibleEnemy) chaseAI->MoveOnlyWithTarget(*this, visibleEnemy, map);
            else UnitMovementPlanner::MoveToTargetByAreaAndEndTurn(*this, player, map, *chaseAI, 3, 3, false);
            break;
        case AllyAIMode::Retreat:
            UnitMovementPlanner::MoveToTargetByAreaAndEndTurn(*this, player, map, *chaseAI, 3, 3, false);
            break;
        case AllyAIMode::Follow:
        default:
            UnitMovementPlanner::MoveToTargetByAreaAndEndTurn(*this, player, map, *chaseAI, 3, 3, false);
            break;
        }
    }

    if (oldPos != GetGridPos()) {
        ConsumeActionForMoveIfPossible();
    }

    if (GetMoveBudget() == moveBudgetBefore && m_MoveState == MoveState::Idle) {
        ConsumeAllMoves();
    }
    SetTurnConsumeType(TurnConsumeType::Action);
}

const char* Ally::GetAIModeName() const
{
    switch (m_AIMode)
    {
    case AllyAIMode::Follow:
        return u8"追従";
    case AllyAIMode::Counter:
        return u8"応戦";
    case AllyAIMode::Wait:
        return u8"待機";
    case AllyAIMode::Patrol:
        return u8"巡回";
    case AllyAIMode::NoSkill:
        return u8"特技使用禁止";
    case AllyAIMode::Retreat:
        return u8"撤退";
    default:
        return u8"不明";
    }
}

void Ally::SetAIMode(AllyAIMode mode)
{
    m_AIMode = mode;
    if (chaseAI) chaseAI->Reset();
    if (patrolAI) patrolAI->ResetFromCurrentPos(*this, MapManager::Instance()->GetCurrentMap());
    m_IsLostPatrolling = false;
    m_PlayerRecognized = false;
    MessageLog::Instance().AddMessage(m_Name + u8"のAIを「" + GetAIModeName() + u8"」にした。");
}

void Ally::EndTurn()
{
    int actionBudgetBefore = GetActionBudget();
    int moveBudgetBefore = GetMoveBudget();
    Unit::EndTurn();
    if (actionBudgetBefore > GetActionBudget() || moveBudgetBefore > GetMoveBudget()) {
        NaturalRecovery();
    }
}

void Ally::Attack()
{
    // 攻撃ごとに表示中ログをリセットする。履歴は MessageLog 側に残す。
    MessageLog::Instance().Clear();
    Vector2Int targetPos = m_GridPos + m_FacingDir;
    if (abs(m_FacingDir.x) == 1 && abs(m_FacingDir.y) == 1) {
        MapData* map = MapManager::Instance()->GetCurrentMap();
        if (map && IsDiagonalMoveBlocked(m_GridPos, m_FacingDir, map)) {
            EndTurn();
            return;
        }
    }
    Unit* target = UnitManager::Instance()->GetUnitAt(targetPos);

    if (!target) return;
    if (Enemy* enemy = dynamic_cast<Enemy*>(target)) {
        if (!IsAllyHostileEnemy(enemy)) return;
    }
    if (ShouldShowCombatVisual(target)) SetTriggerAnimation("Attack", 1.0f);
    if (!CheckHit(GetACC(), target->GetEVD())) {
        MessageLog::Instance().AddMessage(m_Name + u8"の攻撃は外れた。");
        EndTurn();
        return;
    }

    int damage = CalcDamage(GetATK(), target->GetDEF());
    target->TakeDamage(damage,this);
}

void Ally::OnDeath(Unit* attacker) {
    MessageLog::Instance().AddMessage(m_Name + u8"はちからつきた。");
    UnitManager::Instance()->RemoveAlly(this);
    m_AllyMark->SetDestroy();
    m_AllyMark = nullptr;
    StopLoopEffect();
    SetDestroy();
}

void Ally::LevelUp()
{
    m_Level++;

    m_MaxHP *= 1.35;
    m_MaxATK *= 1.5;
    m_ATK = m_MaxATK;
    m_MaxDEF *= 1.35;
    m_DEF = m_MaxDEF;


    m_HP = m_MaxHP;

    m_ExpToNext += 5 + 3 * GetLevel();

    MessageLog::Instance().AddMessage(m_Name+
        u8"のレベルが上がった！ Lv." + std::to_string(m_Level)
    );
}

void Ally::Uninit()
{
    //MapManager及びUnitManagerから自分を削除してから、モデルやエフェクトを解放する。
    UnitManager::Instance()->RemoveAlly(this);
    StopLoopEffect();

    if (m_AnimationModel) {
        m_AnimationModel->Uninit();
        delete m_AnimationModel;
    }
    ReleaseToonShader();
}
void Ally::Talk() {
    for (int i = 0; i < 2; i++)
    {
        if (!m_TalkMessage[i].empty()) {
            if (i == 0)
            {
                MessageLog::Instance().AddMessage(m_Name);
                MessageLog::Instance().AddMessage( u8"「" + m_TalkMessage[i]);
            }
            else
            {
                MessageLog::Instance().AddMessage( m_TalkMessage[i] + u8"」");
            }
        }
    }
    
}
bool Ally::CanRecognizePlayer(Player* player, MapData* map)
{
    if (!player || !map) return false;
    return UnitAI::CanSee(*this, player, map, kPlayerRecognizeRange);
}

bool Ally::CanKeepRecognizedPlayer(Player* player, MapData* map)
{
    if (!player || !map) return false;

    Vector2Int selfPos = GetGridPos();
    Vector2Int playerPos = player->GetGridPos();
    int dx = abs(playerPos.x - selfPos.x);
    int dy = abs(playerPos.y - selfPos.y);
    int keepRange = kPlayerRecognizeRange + kPlayerRecognizedBonusRange;
    return (std::max)(dx, dy) <= keepRange;
}

bool Ally::UpdatePlayerRecognition(Player* player, MapData* map)
{
    if (!player || !map) {
        m_PlayerRecognized = false;
        return false;
    }

    if (m_PlayerRecognized) {
        if (CanKeepRecognizedPlayer(player, map)) return true;
        m_PlayerRecognized = false;
    }

    if (CanRecognizePlayer(player, map)) {
        m_PlayerRecognized = true;
        return true;
    }

    return false;
}
Enemy* Ally::FindVisibleEnemy(MapData* map)
{
    if (!map) return nullptr;

    Enemy* nearest = nullptr;
    float nearestDist = 1e9f;

    for (Enemy* enemy : UnitManager::Instance()->GetEnemies()) {
        if (!IsAllyHostileEnemy(enemy)) continue;
        if (!UnitAI::CanSee(*this, enemy, map, 8)) continue;

        float score = Vector2Int::Distance(GetGridPos(), enemy->GetGridPos());
        if (score < nearestDist) {
            nearestDist = score;
            nearest = enemy;
        }
    }
    return nearest;
}