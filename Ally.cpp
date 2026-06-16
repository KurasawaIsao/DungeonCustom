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
    m_Scale = { 0.5f,0.5f,0.5f };


    // AIは最初に一度だけ生成し、以降はm_CurrentAIの差し替えで使い分ける。
    m_ChaseAI = std::make_unique<ChaseAI>();
    m_RunAwayAI = std::make_unique<RunAwayAI>();
    m_PatrolAI = std::make_unique<BasicPatrolAI>();
    SwitchAI(GetCommandAI());

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
    m_YOffset = d.visual.yOffset;

    if (!m_AnimationModel) m_AnimationModel = new AnimationModel();
    m_AnimationModel->Load(d.visual.modelPath.c_str());
    m_AnimationModel->LoadAnimation(d.visual.animIdle.c_str(), "Idle");
    m_AnimationModel->LoadAnimation(d.visual.animRun.c_str(), "Run");
    m_AnimationModel->LoadAnimation(d.visual.animAttack.c_str(), "Attack");
    m_AnimationModel->LoadAnimation(d.visual.animDamaged.c_str(), "Damaged");
    m_AnimationModel->LoadAnimation(d.visual.animSkill.c_str(), "Skill");
    m_AnimationModel->LoadAnimation(d.visual.animSleep.c_str(), "Sleep");

    // 仲間化後も元の敵種別と同じNotifyタイミングを使用する。
    m_AnimationModel->ClearAllAnimationNotifies();
    for (const EnemyAnimationNotifyData& notify : d.visual.animationNotifies)
    {
        m_AnimationModel->AddAnimationNotifyNormalized(
            notify.animationName,
            notify.normalizedTime,
            notify.notifyName);
    }

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
        if (moveDir.x != 0 || moveDir.y != 0) m_FacingDir = moveDir.normalized();
    }
    else
    {
        MapData* map = MapManager::Instance()->GetCurrentMap();
        const bool patrolFacing = (m_AIMode == AllyAIMode::Patrol || m_IsLostPatrolling);
        if (patrolFacing) {
            if (Enemy* enemy = FindVisibleEnemy(map)) {
                m_FacingDir = (enemy->GetGridPos() - GetGridPos()).normalized();
            }
        }
        else {
            Enemy* enemyTarget = FindAdjacentHostileEnemy();
            if (enemyTarget) {
                m_FacingDir = (enemyTarget->GetGridPos() - GetGridPos()).normalized();
            }
            else {
                Player* p = UnitManager::Instance()->GetPlayer();
                if (p) m_FacingDir = (p->GetGridPos() - GetGridPos()).normalized();
            }
        }
    }

    UpdateFacingRotation();
    this->LookAt(m_FacingDir);
    UpdateAnimation();
    // 攻撃・特技・被ダメージの単発演出が終わったら、次の行動へ進める状態に戻す。
    if (m_IsActingAnimation && (!m_AnimationModel || !m_AnimationModel->IsOneShotPlaying())) {
        m_IsActingAnimation = false;
        if (m_MoveState == MoveState::Idle) {
            // 戦闘演出の直前がRunでも、終了後は現在状態の待機モーションへ戻す。
            PlayAnimation(GetMoveEndAnimation(), 1.0f);
        }
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
    SetActionPhaseChecked(true);
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

    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (m_Status == Status::Confusion) {
        if (map) UnitAI::ExecuteConfusion(*this, map);
        else ConsumeAllActions();
        return;
    }

    if (m_AIMode == AllyAIMode::Retreat) {
        ConsumeAllActions();
        return;
    }
    Enemy* adjacentTarget = FindAdjacentHostileEnemy();

    Enemy* skillTarget = adjacentTarget;
    if (!skillTarget && m_AIMode == AllyAIMode::Wait) {
        // 待機AIはその場から動かないため、視界内の敵を特技だけの候補として拾う。
        skillTarget = FindVisibleEnemy(map);
    }

    if (m_CanUseSkill && skillTarget && m_ChaseAI && m_ChaseAI->ExecuteSkill(*this, skillTarget)) {
        EndTurn();
        if (!CanActThisTurn()) ConsumeAllMoves();
        return;
    }

    if (!adjacentTarget) return;

    int budgetBefore = GetActionBudget();
    m_FacingDir = (adjacentTarget->GetGridPos() - GetGridPos()).normalized();
    Attack();
    if (GetActionBudget() == budgetBefore) EndTurn();
}
void Ally::UpdateMovePhase()
{
    // 味方の移動フェーズ。命令と周囲の状況から、保持済みAIのどれを使うか決める。
    if (IsMovePhaseChecked() || IsAnimatingMove()) return;
    SetMovePhaseChecked(true);
    SetTurnConsumeType(TurnConsumeType::Move);
    if (!CanMoveThisTurn()) return;
    if (m_Status == Status::Sleep || m_Status == Status::Paralysis) {
        ConsumeAllMoves();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }
    if (m_AIMode == AllyAIMode::Wait) {
        SwitchAI(nullptr);
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
        // プレイヤーを再発見したら、本来の命令に対応するAIへ戻す。
        m_IsLostPatrolling = false;
        SwitchAI(GetCommandAI());
    }

    // 撤退時は敵を追跡対象ではなく、RunAwayAIへ渡す脅威として利用する。
    Enemy* visibleEnemy = FindVisibleEnemy(map);
    bool attackableEnemy = visibleEnemy && m_ChaseAI && m_ChaseAI->IsAttackAdjacent(*this, visibleEnemy, map);
    if (attackableEnemy && m_AIMode != AllyAIMode::Retreat) {
        ConsumeAllMoves();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }
    if (visibleEnemy && m_AIMode != AllyAIMode::Counter && m_AIMode != AllyAIMode::NoSkill
        && m_AIMode != AllyAIMode::Patrol && m_AIMode != AllyAIMode::Retreat) {
        visibleEnemy = nullptr;
    }

    if (m_AIMode == AllyAIMode::Patrol) {
        if (visibleEnemy && m_ChaseAI && map) {
            SwitchAI(m_ChaseAI.get());
            m_ChaseAI->MoveOnlyWithTarget(*this, visibleEnemy, map);
        }
        else if (m_PatrolAI && map) {
            SwitchAI(m_PatrolAI.get());
            m_CurrentAI->Update(*this, map);
        }
        else {
            ConsumeAllMoves();
        }
        if (oldPos != GetGridPos()) ConsumeActionForMoveIfPossible();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }

    if (!playerRecognized && chaseMode) {
        // プレイヤーを見失った間だけ巡回AIへ切り替え、再発見を待つ。
        if (m_PatrolAI && map) {
            m_IsLostPatrolling = true;
            SwitchAI(m_PatrolAI.get());
            m_CurrentAI->Update(*this, map);
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

    if (map && player) {
        switch (m_AIMode) {
        case AllyAIMode::Counter:
        case AllyAIMode::NoSkill:
            SwitchAI(m_ChaseAI.get());
            if (visibleEnemy) m_ChaseAI->MoveOnlyWithTarget(*this, visibleEnemy, map);
            else UnitMovementPlanner::MoveToTargetByAreaAndEndTurn(*this, player, map, *m_ChaseAI, 3, 3, false);
            break;
        case AllyAIMode::Retreat:
            if (visibleEnemy && m_RunAwayAI) {
                // 見えている敵から離れつつ、安全目標であるプレイヤー側へ寄る。
                SwitchAI(m_RunAwayAI.get());
                m_RunAwayAI->MoveAwayFromTarget(*this, visibleEnemy, player, map);
            }
            else {
                // 脅威が見えない時は孤立しないよう、プレイヤーの近くへ戻る。
                SwitchAI(m_ChaseAI.get());
                UnitMovementPlanner::MoveToTargetByAreaAndEndTurn(*this, player, map, *m_ChaseAI, 3, 3, false);
            }
            break;
        case AllyAIMode::Follow:
        default:
            SwitchAI(m_ChaseAI.get());
            UnitMovementPlanner::MoveToTargetByAreaAndEndTurn(*this, player, map, *m_ChaseAI, 3, 3, false);
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
    m_CanUseSkill = (mode != AllyAIMode::NoSkill);
    m_IsLostPatrolling = false;
    m_PlayerRecognized = false;
    SwitchAI(GetCommandAI());
    MessageLog::Instance().AddMessage(m_Name + u8"のAIを「" + GetAIModeName() + u8"」にした。");
}

void Ally::SwitchAI(UnitAI* nextAI)
{
    if (m_CurrentAI == nextAI) return;

    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (m_CurrentAI) {
        // 切替前のAIへ終了を通知し、必要なら経路などを整理させる。
        m_CurrentAI->OnExit(*this, map);
    }

    m_CurrentAI = nextAI;
    if (m_CurrentAI) {
        // 新しいAIへ開始を通知し、現在位置を基準に行動準備をさせる。
        m_CurrentAI->OnEnter(*this, map);
    }
}

UnitAI* Ally::GetCommandAI() const
{
    switch (m_AIMode)
    {
    case AllyAIMode::Follow:
    case AllyAIMode::Counter:
    case AllyAIMode::NoSkill:
        return m_ChaseAI.get();
    case AllyAIMode::Patrol:
        return m_PatrolAI.get();
    case AllyAIMode::Retreat:
        return m_RunAwayAI.get();
    case AllyAIMode::Wait:
    default:
        return nullptr;
    }
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

void Ally::StartAttackWithNotify(Unit* target)
{
    if (!target || target->GetHP() <= 0) return;

    // 敵側と同じく、回避判定を先に確定して攻撃前メッセージをNotifyより先に出す。
    m_PendingAttackTarget = target;
    m_PendingAttackHit = CheckHit(GetACC(), target->GetEVD());
    if (m_PendingAttackHit)
    {
        MessageLog::Instance().AddMessage(m_Name + u8"から");
    }

    const bool showVisual = ShouldShowCombatVisual(target);
    if (showVisual &&
        m_AnimationModel &&
        m_AnimationModel->HasAnimationNotify("Attack", "AttackHit"))
    {
        // ダメージ処理はAttackHit Notifyまで保留し、見た目と判定の瞬間を一致させる。
        SetTriggerAnimation("Attack", 1.0f);
        return;
    }

    // Notify未登録時は即時処理へ戻すが、表示可能なら攻撃モーション自体は再生する。
    ResolvePendingAttack();
    if (showVisual) SetTriggerAnimation("Attack", 1.0f);
}

void Ally::ResolvePendingAttack()
{
    Unit* target = m_PendingAttackTarget;
    const bool attackHit = m_PendingAttackHit;
    m_PendingAttackTarget = nullptr;
    m_PendingAttackHit = false;
    if (!target || target->GetHP() <= 0) return;

    if (!attackHit)
    {
        MessageLog::Instance().AddMessage(m_Name + u8"の攻撃は外れた。");
        return;
    }

    const int damage = CalcDamage(GetATK(), target->GetDEF());
    target->TakeDamage(damage, this);
}

bool Ally::QueueSkillForNotify(
    const Skill& skill,
    const EffectContext& context,
    const std::vector<Unit*>& targets)
{
    if (!m_AnimationModel ||
        !m_AnimationModel->HasAnimationNotify("Skill", "SkillEffect"))
    {
        return false;
    }

    m_PendingSkillEffect = skill.effect;
    m_PendingSkillContext = context;
    m_PendingSkillTargets = targets;
    return true;
}

void Ally::ResolvePendingSkill()
{
    std::shared_ptr<EffectBase> effect = m_PendingSkillEffect;
    EffectContext context = m_PendingSkillContext;
    std::vector<Unit*> targets = m_PendingSkillTargets;

    m_PendingSkillEffect.reset();
    m_PendingSkillContext = {};
    m_PendingSkillTargets.clear();
    if (!effect) return;

    if (context.targetType == EffectTargetType::Single)
    {
        if (context.target && context.target->GetHP() > 0)
            effect->Apply(context);
        return;
    }

    for (Unit* target : targets)
    {
        if (!target || target->GetHP() <= 0) continue;
        EffectContext each = context;
        each.target = target;
        each.pos = target->GetGridPos();
        effect->Apply(each);
    }
}

void Ally::ClearPendingCombatActions()
{
    m_PendingAttackTarget = nullptr;
    m_PendingAttackHit = false;
    m_PendingSkillEffect.reset();
    m_PendingSkillContext = {};
    m_PendingSkillTargets.clear();
}

void Ally::OnAnimationNotify(
    const std::string& animationName,
    const std::string& notifyName)
{
    if (animationName == "Attack" && notifyName == "AttackHit")
    {
        ResolvePendingAttack();
        return;
    }

    if (animationName == "Skill" && notifyName == "SkillEffect")
    {
        ResolvePendingSkill();
    }
}

void Ally::Attack()
{
    // 攻撃ごとに表示中ログをリセットする。履歴は MessageLog 側に残す。
    MessageLog::Instance().Clear();
    Vector2Int targetPos = m_GridPos + m_FacingDir;
    if (m_FacingDir.Chebyshev(Vector2Int(0, 0)) == 1 && m_FacingDir.Manhattan(Vector2Int(0, 0)) == 2) {
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
    StartAttackWithNotify(target);
}

void Ally::OnDeath(Unit*) {
    MessageLog::Instance().AddMessage(m_Name + u8"はちからつきた。");
    DismissFromParty();
}

void Ally::DismissFromParty()
{
    // 仲間一覧から外し、仲間専用の表示物も同時に破棄して安全に退場させる。
    ClearPendingCombatActions();
    UnitManager::Instance()->RemoveAlly(this);
    if (m_AllyMark)
    {
        m_AllyMark->SetDestroy();
        m_AllyMark = nullptr;
    }
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
    ClearPendingCombatActions();
    UnitManager::Instance()->RemoveAlly(this);
    if (m_AllyMark)
    {
        m_AllyMark->SetDestroy();
        m_AllyMark = nullptr;
    }
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
    int keepRange = kPlayerRecognizeRange + kPlayerRecognizedBonusRange;
    return Vector2Int::ChebyshevDistance(playerPos, selfPos) <= keepRange;
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
Enemy* Ally::FindAdjacentHostileEnemy()
{
    UnitManager* units = UnitManager::Instance();
    if (!units) return nullptr;

    // UnitManager の隣接敵リストから、店主など味方が攻撃しない相手を除外する。
    for (Enemy* enemy : units->GetAdjacentEnemies(*this)) {
        if (IsAllyHostileEnemy(enemy)) return enemy;
    }
    return nullptr;
}

Enemy* Ally::FindVisibleEnemy(MapData* map)
{
    if (!map) return nullptr;
    UnitManager* units = UnitManager::Instance();
    if (!units) return nullptr;

    const Vector2Int selfPos = GetGridPos();
    Enemy* nearest = nullptr;
    float nearestDist = 1e9f;

    for (Enemy* enemy : units->GetEnemies()) {
        if (!IsAllyHostileEnemy(enemy)) continue;
        if (!UnitAI::CanSee(*this, enemy, map, 8)) continue;

        float score = Vector2Int::Distance(selfPos, enemy->GetGridPos());
        if (score < nearestDist) {
            nearestDist = score;
            nearest = enemy;
        }
    }
    return nearest;
}