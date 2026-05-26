#include "GameRandom.h"
#include "Enemy.h"
#include "UnitManager.h"
#include "MapData.h"
#include "MapManager.h"
#include "animationModel.h"
#include "renderer.h"
#include <cmath>
#include <algorithm>
#include "MessageLog.h"
#include "Player.h"
#include "Ally.h"
#include "UnitAI.h"
#include "BasicPatrolAI.h"
#include "ChaseAI.h"
#include "KeepDistAI.h"
#include "BerserkAI.h"
#include "RunAwayAI.h"
#include "WhimAI.h"
#include "ShopUnitAI.h"
#include "EnemyData.h"
#include "EffectBillboard.h"
#include "PlayerInventoryUI.h"
#include "TurnManager.h"
#include "manager.h"
#include "scene.h"
#include "EnemyModelManager.h"
#include "ItemDataBase.h"
#include "Item.h"
#include "UnitMovementPlanner.h"
// Enemy は敵ユニット本体。
// TurnManager から UpdateActionPhase と UpdateMovePhase が呼ばれ、
// currentAI(巡回/追跡/逃走など)を使って「攻撃・特技」と「移動」を分けて実行する。

namespace
{
    constexpr int kFallbackDungeonVisionRange = 2;

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

    void AwardDefeatExperience(Enemy* defeated, Unit* attacker)
    {
        if (!defeated || !attacker) return;

        const int expReward = defeated->GetExpReward();
        if (expReward <= 0) return;

        // 敵を倒した本人に経験値を渡す。仲間が倒した場合は、従来どおりプレイヤーにも同じ経験値を入れる。
        if (Player* player = dynamic_cast<Player*>(attacker)) {
            MessageLog::Instance().AddMessage(
                player->GetName() + u8"は" + std::to_string(expReward) + u8"経験値を得た。"
            );
            player->AddExp(expReward);
            return;
        }

        if (Ally* ally = dynamic_cast<Ally*>(attacker)) {
            MessageLog::Instance().AddMessage(
                ally->GetName() + u8"は" + std::to_string(expReward) + u8"経験値を得た。"
            );
            ally->AddExp(expReward);

            Player* player = UnitManager::Instance() ? UnitManager::Instance()->GetPlayer() : nullptr;
            if (player) {
                MessageLog::Instance().AddMessage(
                    player->GetName() + u8"は" + std::to_string(expReward) + u8"経験値を得た。"
                );
                player->AddExp(expReward);
            }
        }
    }

    enum class RecognitionReason
    {
        None,
        SameRoom,
        RoomMargin,
        VisionRange
    };

    int GetDungeonVisionRange()
    {
        MapManager* mapManager = MapManager::Instance();
        if (!mapManager) return kFallbackDungeonVisionRange;

        // プレイヤーの視界表示と同じ、現在フロアの viewDistance を敵の部屋外索敵にも使う。
        return (std::max)(0, mapManager->GetCurrentFloorData().viewDistance);
    }
    int GetChebyshevDistance(const Vector2Int& from, const Vector2Int& to)
    {
        int dx = abs(to.x - from.x);
        int dy = abs(to.y - from.y);
        return (std::max)(dx, dy);
    }

    bool IsWithinVisionRange(const Vector2Int& centerPos, const Vector2Int& targetPos, int visionRange)
    {
        return GetChebyshevDistance(centerPos, targetPos) <= visionRange;
    }

    bool IsInsideRoomMargin(const Room& room, const Vector2Int& targetPos, MapData* map, int visionRange)
    {
        if (!map || !map->IsInBounds(targetPos) || visionRange < 0) return false;

        Vector2Int roomPos = room.GetPosition();
        Vector2Int roomSize = room.GetSize();
        int left = roomPos.x;
        int top = roomPos.y;
        int right = roomPos.x + roomSize.x - 1;
        int bottom = roomPos.y + roomSize.y - 1;

        int dx = 0;
        if (targetPos.x < left) dx = left - targetPos.x;
        else if (targetPos.x > right) dx = targetPos.x - right;

        int dy = 0;
        if (targetPos.y < top) dy = top - targetPos.y;
        else if (targetPos.y > bottom) dy = targetPos.y - bottom;

        // 部屋外周からの Chebyshev 距離で見る。大部屋内の敵位置に左右されず、入口付近の通路を拾うため。
        return (std::max)(dx, dy) <= visionRange;
    }
    RecognitionReason GetRecognitionReason(const Room* selfRoom, const Vector2Int& selfPos, const Vector2Int& targetPos, MapData* map, int visionRange)
    {
        if (!map || !map->IsInBounds(targetPos)) return RecognitionReason::None;

        const Room* targetRoom = map->GetRoomAt(targetPos);

        // 同じ部屋にいる対象は、ダンジョン視界距離に関係なく認識する。
        if (selfRoom && targetRoom == selfRoom) return RecognitionReason::SameRoom;

        // 敵が部屋にいる時は、別部屋の中は見えないが、部屋外周から視界マス以内の通路は見える。
        if (selfRoom) {
            if (targetRoom && targetRoom != selfRoom) return RecognitionReason::None;
            if (IsInsideRoomMargin(*selfRoom, targetPos, map, visionRange)) return RecognitionReason::RoomMargin;
        }

        if (IsWithinVisionRange(selfPos, targetPos, visionRange)) return RecognitionReason::VisionRange;
        return RecognitionReason::None;
    }
    Vector2Int WorldToGridForView(const Vector3& position)
    {
        return Vector2Int(
            static_cast<int>(std::round(position.x / static_cast<float>(TILE_DISTANCE))),
            static_cast<int>(std::round(position.z / static_cast<float>(TILE_DISTANCE))));
    }

    bool IsEnemyHostileTarget(Unit* unit)
    {
        return dynamic_cast<Player*>(unit) || dynamic_cast<Ally*>(unit);
    }

    bool IsTurnBlockedByStatus(Status status)
    {
        return status == Status::Sleep || status == Status::Paralysis || status == Status::Nap;
    }

    bool UsesHostileRecognitionState(EnemyAIType aiType)
    {
        switch (aiType)
        {
        case EnemyAIType::PatrolAndChase:
        case EnemyAIType::KeepDistance:
        case EnemyAIType::RunAway:
        case EnemyAIType::WhimAlwaysAttack:
        case EnemyAIType::WhimRandomAttack:
            return true;
        default:
            return false;
        }
    }
}

Enemy::Enemy()
{
}

Enemy::~Enemy()
{
}
void Enemy::Init()
{   

    m_AnimNow = "Idle";
    m_AnimNext = "Idle";

    m_AnimSpeed = 0.5f;
    m_AnimSpeedCnt = 0.0f;
    m_AnimSpeedCntMax = 1.0f; 
    m_Frame = 0;

    m_Scale = { 0.5f,0.5f,0.5f };

    InitToonShader();

    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);
    SetInitGridPos({ 0,0 });

    PlayAnimation("Idle", 1.0f);
}
void Enemy::Draw()
{
    if (!m_AnimationModel)return;
    Player* player = UnitManager::Instance()->GetPlayer();
    if (player) {
        bool visibleToPlayer = player->IsInView(GetGridPos());
        if (!visibleToPlayer && IsAnimatingMove()) {
            visibleToPlayer =
                player->IsInView(WorldToGridForView(m_MoveStartPos)) ||
                player->IsInView(WorldToGridForView(GetVisualPosition()));
        }
        if (!visibleToPlayer && m_IsActingAnimation) {
            visibleToPlayer = true;
        }

        // 視界外の敵は描画を省き、表示負荷を抑える。
        if (!visibleToPlayer) {
            return;
        }
    }

    Vector3 visualPos = GetVisualPosition();
    Vector3 visualRot = GetVisualRotation();
    XMMATRIX world, trans, rot, scale;
    scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    rot = XMMatrixRotationRollPitchYaw(visualRot.x, visualRot.y, visualRot.z);
    trans = XMMatrixTranslation(visualPos.x, visualPos.y + 1.3f, visualPos.z);
    world = scale * rot * trans;

    DrawToonModel(world);
}
void Enemy::Uninit()
{
	//MapManager及びUnitManagerから自分を削除してから、モデルやエフェクトを解放する。
    UnitManager::Instance()->RemoveEnemy(this);
    StopLoopEffect();

    if (m_AnimationModel)
    {
        m_AnimationModel->Uninit();
        delete m_AnimationModel;
        m_AnimationModel = nullptr;
    }
    ReleaseToonShader();

}
void Enemy::Update()
{
    RepairInvalidGridPos("Enemy::Update");
    Player* player = UnitManager::Instance()->GetPlayer();
    bool isInPlayerView = true;
    if (player) {
        isInPlayerView = player->IsInView(GetGridPos());
        if (!isInPlayerView && IsAnimatingMove()) {
            isInPlayerView =
                player->IsInView(WorldToGridForView(m_MoveStartPos)) ||
                player->IsInView(WorldToGridForView(GetVisualPosition()));
        }

        // 視界外の敵は描画を省き、表示負荷を抑える。
        if (isInPlayerView || m_IsActingAnimation) {
            UpdateAnimation();
        }
    }
    else {
        UpdateAnimation();
    }

    if (m_IsActingAnimation && (m_IsAnimLooping || IsAnimationFinished())) {
        m_IsActingAnimation = false;
    }
   
    // 状態異常のループエフェクトはユニットの頭上に追従させる。
    if (m_LoopEffect)
    {
        // ユニットの座標に追従させる
        Vector3 effectPos = m_Position;
        effectPos.y += 2.0f;
        m_LoopEffect->SetPosition(effectPos);
    }
    if (IsTurnBlockedByStatus(m_Status)) {
        return;
    }
    Unit* visibleTarget = nullptr;
    const bool patrolFacing = (m_Data.aiType == EnemyAIType::Patrol ||
        (m_Data.aiType == EnemyAIType::PatrolAndChase && state == EnemyState::Patrol));
    if (patrolFacing) {
        visibleTarget = FindVisibleNonPlayerHostileTarget();
    }
    else {
        visibleTarget = FindVisibleHostileTarget("UpdateFacing");
    }
    if (visibleTarget) {
        Vector2Int diff = visibleTarget->GetGridPos() - GetGridPos();
        if (diff.x != 0 || diff.y != 0) {
            m_FacingDir = diff;
        }
    }
    // 決定した m_FacingDir に基づいてモデルの向きを更新する。
    UpdateFacingRotation();
    this->LookAt(m_FacingDir);
   
}
// =========================
//   メインAI呼び出し
// =========================
void Enemy::UpdateUnit()
{
    // 旧来の直接更新用入口。現在の通常ターン進行では TurnManager が各フェーズを個別に呼ぶ。
    UpdateActionPhase();
    if (!CanMoveThisTurn() || m_MoveState != MoveState::Idle) return;
    UpdateMovePhase();
}

void Enemy::UpdateActionPhase()
{
    // 攻撃・特技だけを判定する。
    // 移動までここで行うと倍速/三倍速の予算管理が崩れるため、移動は UpdateMovePhase に任せる。
    if (IsDead || IsActionPhaseChecked()) return;
    RepairInvalidGridPos("Enemy::UpdateActionPhase");
    MarkActionPhaseChecked();
    SetTurnConsumeType(TurnConsumeType::Action);
    if (!CanActThisTurn()) return;

	//仮眠から起きた場合は行動しない
    if (m_ConsumeTurnAfterNapConditionWake) {
        m_ConsumeTurnAfterNapConditionWake = false;
        ConsumeAllActions();
        ConsumeAllMoves();
        return;
    }

    if (ConsumeActionBlockAfterStatusClear()) {
        ConsumeAllActions();
        ConsumeAllMoves();
        return;
    }

    Status statusBeforeCount = m_Status;
    if (UpdateNap()) {
        m_ConsumeTurnAfterNapConditionWake = false;
        ConsumeAllActions();
        ConsumeAllMoves();
        return;
    }
    bool statusCleared = UpdateStatusCount();
    if (statusCleared && (statusBeforeCount == Status::Sleep || statusBeforeCount == Status::Paralysis || statusBeforeCount == Status::Nap)) {
        ConsumeAllActions();
        ConsumeAllMoves();
        return;
    }
    if (IsTurnBlockedByStatus(m_Status)) {
        ConsumeAllActions();
        ConsumeAllMoves();
        return;
    }

    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) { ConsumeAllActions(); return; }

    if (m_Status == Status::Confusion)
    {
        UnitAI::ExecuteConfusion(*this, map);
        return;
    }

    Unit* target = nullptr;
    if (auto* berserk = dynamic_cast<BerserkAI*>(currentAI)) {
        target = berserk->FindTarget(*this, map);
    }
    else if (m_Data.aiType != EnemyAIType::Passive && m_Data.aiType != EnemyAIType::RunAway) {
        target = FindVisibleHostileTarget("UpdateActionPhase");
    }
    if (!target) {
        // 索敵範囲から外れた時点で追跡状態を終わらせ、後続の移動フェーズは現在位置から巡回を選び直す。
        if (state == EnemyState::Chase && UsesHostileRecognitionState(m_Data.aiType)) {
            ReturnToPatrolFromCurrentPos();
        }
        ConsumeAllActions();
        return;
    }
    const bool isChaseAI = dynamic_cast<ChaseAI*>(currentAI) != nullptr;
    const bool isChaseAdjacent =
        (isChaseAI && currentAI && currentAI->IsAdjacent(*this, target));
    const bool isKeepDistanceAdjacent =
        (m_Data.aiType == EnemyAIType::KeepDistance && currentAI && currentAI->IsAdjacent(*this, target));
    const bool canAttackTarget = currentAI && currentAI->IsAttackAdjacent(*this, target, map);

    // 行動フェーズでは、特技または通常攻撃だけを行う。移動は後続の移動フェーズに回す。
    if (canAttackTarget) {
        if (auto* whim = dynamic_cast<WhimAI*>(currentAI)) {
            if (!whim->ShouldAttackAdjacent()) {
                ConsumeAction();
                ConsumeAllMoves();
                return;
            }
        }
    }

    if (currentAI && currentAI->ExecuteSkill(*this, target)) {
        if (isKeepDistanceAdjacent || isChaseAdjacent) ConsumeAllMoves();
        EndTurn();
        if (!CanActThisTurn()) ConsumeAllMoves();
        return;
    }
    if (m_Data.aiType == EnemyAIType::KeepDistance && currentAI && !currentAI->IsAdjacent(*this, target)) {
        ConsumeAction();
        return;
    }

    if (canAttackTarget) {
        int budgetBefore = GetActionBudget();
        Vector2Int dir = target->GetGridPos() - GetGridPos();
        SetCurrentDir(dir);
        LookAt(dir);
        Attack();
        if (isKeepDistanceAdjacent || isChaseAdjacent) ConsumeAllMoves();
        if (GetActionBudget() == budgetBefore) EndTurn();
        return;
    }
    return;
}

void Enemy::UpdateMovePhase()
{
    // 移動だけを判定する。
    // AI ごとに目標選択は異なるが、予算消費と移動失敗時の扱いはここで統一する。
    if (IsDead || IsMovePhaseChecked() || IsAnimatingMove()) return;
    RepairInvalidGridPos("Enemy::UpdateMovePhase");
    MarkMovePhaseChecked();
    SetTurnConsumeType(TurnConsumeType::Move);
    if (!CanMoveThisTurn()) return;

	// 仮眠条件に引っかかって起きた場合は行動しない
    if (m_ConsumeTurnAfterNapConditionWake) {
        m_ConsumeTurnAfterNapConditionWake = false;
        ConsumeAllMoves();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }
    if (IsTurnBlockedByStatus(m_Status)) {
        ConsumeAllMoves();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }

    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) {
        ConsumeAllMoves();
        SetTurnConsumeType(TurnConsumeType::Action);
        return;
    }

    int moveBudgetBefore = GetMoveBudget();
    Vector2Int oldPos = GetGridPos();

    if (auto* chase = dynamic_cast<ChaseAI*>(currentAI)) {
        Unit* target = FindVisibleHostileTarget("UpdateMovePhase:Chase");
        if (!target) {
            ReturnToPatrolFromCurrentPos();
            // 見失ったターンも停止せず、現在位置から巡回ルートを選び直す。
            if (currentAI) currentAI->Update(*this, map);
        }
        else {
            UnitMovementPlanner::MoveToTargetByAreaAndEndTurn(*this, target, map, *chase, 3, 3, true);

            // 見失い判定は行動開始時にまとめる。移動直後に部屋/通路の基準を切り替えると、入口や角で不自然に追跡が切れやすい。
        }
    }
    else if (auto* keep = dynamic_cast<KeepDistAI*>(currentAI)) {
        Unit* target = FindVisibleHostileTarget("UpdateMovePhase:KeepDistance");
        keep->UpdateWithTarget(*this, target, map);
    }
    else if (auto* runAway = dynamic_cast<RunAwayAI*>(currentAI)) {
        Unit* target = FindVisibleHostileTarget("UpdateMovePhase:RunAway");
        runAway->MoveAwayFromTarget(*this, target, nullptr, map);
    }
    else if (auto* berserk = dynamic_cast<BerserkAI*>(currentAI)) {
        Unit* target = berserk->FindTarget(*this, map);
        if (target && currentAI->IsAdjacent(*this, target)) {
            ConsumeAllMoves();
        }
        else if (currentAI) {
            currentAI->Update(*this, map);
        }
    }
    else if (auto* whim = dynamic_cast<WhimAI*>(currentAI)) {
        Unit* target = FindVisibleHostileTarget("UpdateMovePhase:Whim");
        if (target && currentAI->IsAdjacent(*this, target)) {
            ConsumeAllMoves();
        }
        else {
            whim->UpdateWithTarget(*this, target, map);
        }
    }
    else if (currentAI) {
        currentAI->Update(*this, map);
    }
    Vector2Int newPos = GetGridPos();
    if (oldPos != newPos)
    {
        Vector2Int moveDir = newPos - oldPos;
        LookAt(moveDir);
        ConsumeActionForMoveIfPossible();
    }

    // 動けなかった場合に倍速移動の残回数で同じ失敗を繰り返さないようにする。
    if (GetMoveBudget() == moveBudgetBefore && m_MoveState == MoveState::Idle)
    {
        ConsumeAllMoves();
    }
    SetTurnConsumeType(TurnConsumeType::Action);
}
void Enemy::DecideNextAction()
{
    // ターン開始時の AI 状態切り替え。
    // 敵対対象を追うタイプは共通で、発見したら追跡状態へ、見失ったら巡回状態へ戻す。
    if (!UsesHostileRecognitionState(m_Data.aiType)) return;

    Unit* visibleTarget = FindVisibleHostileTarget("DecideNextAction");

    if (state == EnemyState::Patrol)
    {
        if (visibleTarget)
        {
            state = EnemyState::Chase;
            if (chaseAI) currentAI = chaseAI.get();
            m_TargetRecognized = true;
            Vector2Int dir = visibleTarget->GetGridPos() - this->GetGridPos();
            this->m_FacingDir = dir;
            this->LookAt(dir);
        }
    }
    else if (state == EnemyState::Chase)
    {
        if (visibleTarget)
        {
            m_TargetRecognized = true;
        }
        else
        {
            ReturnToPatrolFromCurrentPos();
        }
    }
}
bool Enemy::UpdateNap()
{
    if (m_Status != Status::Nap) return false;
    UnitManager* units = UnitManager::Instance();
    if (!units) return false;

    Player* player = units->GetPlayer();
    if (!player) return false;
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return false;

    Vector2Int pPos = player->GetGridPos();
    Vector2Int ePos = GetGridPos();
    const Room* enemyRoom = map->GetRoomAt(ePos);

    auto enteredSameRoom = [&](Unit* unit) -> bool {
        if (!unit || !enemyRoom) return false;
        return map->GetRoomAt(unit->GetGridPos()) == enemyRoom;
    };
    auto enteredAdjacent = [&](Unit* unit) -> bool {
        if (!unit) return false;
        Vector2Int pos = unit->GetGridPos();
        int dx = abs(pos.x - ePos.x);
        int dy = abs(pos.y - ePos.y);
        return (std::max)(dx, dy) <= 1;
    };

    bool shouldWakeUp = false;

    switch (m_Data.sleepType)
    {
    case SleepType::WakeOnRoom:
    {
        if (enteredAdjacent(player)) shouldWakeUp = true;
        if (enteredSameRoom(player)) shouldWakeUp = true;
        if (!shouldWakeUp) {
            for (Ally* ally : units->GetAllies()) {
                if (enteredSameRoom(ally)) {
                    shouldWakeUp = true;
                    break;
                }
                if (enteredSameRoom(player))
                {
                    shouldWakeUp = true;
                    break;
                }
            }
        }
        break;
    }
    case SleepType::WakeOnAdjacent:
        if (enteredAdjacent(player)) shouldWakeUp = true;
        if (!shouldWakeUp) {
            for (Ally* ally : units->GetAllies()) {
                if (enteredAdjacent(ally)) {
                    shouldWakeUp = true;
                    break;
                }
            }
        }
        break;

    case SleepType::WakeOnDamage:
        break;

    default:
        break;
    }

    if (shouldWakeUp)
    {
        ClearNap(true);
        return true;
    }
    return false;
}
void Enemy::ClearNap(bool consumeTurn)
{
    if(GetStatus()== Status::Nap)
    {
        m_ConsumeTurnAfterNapConditionWake = consumeTurn;
        ClearStatus();
	}
}
void Enemy::SuppressHostileRecognitionThisTurn()
{
    // 吹き飛ばしなどの強制移動後は、その敵ターン中だけ索敵を止めて追跡を切る。
    m_SuppressHostileRecognitionThisTurn = true;
    ReturnToPatrolFromCurrentPos();
}
void Enemy::SetShopKeeperMode(bool hostile)
{
    m_IsShopKeeper = true;
    m_IsShopHostile = hostile;
    patrolAI = std::make_unique<ShopUnitAI>(hostile);
    chaseAI.reset();
    currentAI = patrolAI.get();
    ClearStatus();
}
void Enemy::ResetAI(EnemyAIType aiType)
{
    patrolAI.reset();
    chaseAI.reset();
    currentAI = nullptr;
    state = EnemyState::Patrol;
    ClearTargetRecognition();

    switch (aiType)
    {
    case EnemyAIType::Patrol:
        patrolAI = std::make_unique<BasicPatrolAI>();
        currentAI = patrolAI.get();
        break;
    case EnemyAIType::PatrolAndChase:
        patrolAI = std::make_unique<BasicPatrolAI>();
        chaseAI = std::make_unique<ChaseAI>();
        currentAI = patrolAI.get();
        break;
    case EnemyAIType::KeepDistance:
        patrolAI = std::make_unique<KeepDistAI>((std::max)(1, m_Data.keepDistance));
        currentAI = patrolAI.get();
        break;
    case EnemyAIType::Berserk:
    {
        auto berserk = std::make_unique<BerserkAI>();
        berserk->SetVisionRange((std::max)(1, m_Data.visionRange));
        patrolAI = std::move(berserk);
        currentAI = patrolAI.get();
        break;
    }
    case EnemyAIType::RunAway:
        patrolAI = std::make_unique<RunAwayAI>();
        currentAI = patrolAI.get();
        break;
    case EnemyAIType::WhimAlwaysAttack:
        patrolAI = std::make_unique<WhimAI>(true);
        currentAI = patrolAI.get();
        break;
    case EnemyAIType::WhimRandomAttack:
        patrolAI = std::make_unique<WhimAI>(false);
        currentAI = patrolAI.get();
        break;
    case EnemyAIType::Passive:
        // AI思考を持たない。常に待機するだけの敵。
		break;
    default:
        break;
    }
}
void Enemy::ChangeAI(EnemyAIType aiType)
{
    if (m_IsShopKeeper) return;

    m_Data.aiType = aiType;
    ResetAI(aiType);
    MessageLog::Instance().AddMessage(m_Name + u8"の動きが変わった。");
}
void Enemy::OnDeath(Unit* attacker)
{
    IsDead = true;
    const bool canRecruit = m_RecruitByPlayerNormalAttack && dynamic_cast<Player*>(attacker);
    m_RecruitByPlayerNormalAttack = false;

    DropItem();
    AwardDefeatExperience(this, attacker);
    //  勧誘の確率判定
    if (canRecruit)
    {
        int recruitChance = 15 + m_RecruitmentModifier;
        if (GameRandom::Percent(recruitChance))
        {
            StopLoopEffect();
            auto* ui = Manager::GetScene()->GetGameObject<PlayerInventoryUI>();
            if (ui)
            {
                TurnManager::Instance()->PauseTurnProgression();
                ui->OpenRecruitMenu(this);
                return;
            }
        }
    }
    UnitManager::Instance()->RemoveEnemy(this);
    currentAI = nullptr;
    patrolAI.reset();
    chaseAI.reset();
    MessageLog::Instance().AddMessage(
        m_Name + u8"はちからつきた。"
    );
   StopLoopEffect();
   SetDestroy();
}

void Enemy::Attack()
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

	// 無差別攻撃AI取得
    const bool attacksAnyUnit = dynamic_cast<BerserkAI*>(currentAI) != nullptr;

    auto canAttackTarget = [&](Unit* candidate) -> bool {
        if (!candidate || candidate == this || candidate->GetHP() <= 0) return false;
        if (!attacksAnyUnit) return IsEnemyHostileTarget(candidate);

        // 無差別攻撃AIは勢力を見ずに攻撃する。ただし非敵対の店主だけは店の保護対象として除外する。
        if (Enemy* enemy = dynamic_cast<Enemy*>(candidate)) {
            if (enemy->IsShopKeeper() && !enemy->IsShopHostile()) return false;
        }
        return true;
    };

    if (!canAttackTarget(target)) {
        target = nullptr;
    }

    if (!target) {
        MapData* map = MapManager::Instance()->GetCurrentMap();
        Unit* bestTarget = nullptr;
        int bestDist = 999999;

        auto consider = [&](Unit* candidate) {
            if (!canAttackTarget(candidate)) return;
            Vector2Int dir = candidate->GetGridPos() - m_GridPos;
            int adx = abs(dir.x);
            int ady = abs(dir.y);
            if (adx == 0 && ady == 0) return;
            if (adx > 1 || ady > 1) return;
            if (adx == 1 && ady == 1 && map && IsDiagonalMoveBlocked(m_GridPos, dir, map)) return;

            int score = adx + ady;
            if (score < bestDist) {
                bestDist = score;
                bestTarget = candidate;
            }
        };

        UnitManager* units = UnitManager::Instance();
        if (!units) return;
        consider(units->GetPlayer());
        for (Ally* ally : units->GetAllies()) {
            consider(ally);
        }
        if (attacksAnyUnit) {
            for (Enemy* enemy : units->GetEnemies()) {
                consider(enemy);
            }
        }

        target = bestTarget;
        if (!target) {
            return;
        }

        LookAt(target->GetGridPos() - m_GridPos);
    }

    if (ShouldShowCombatVisual(target)) {
        m_IsActingAnimation = true;
        SetTriggerAnimation("Attack", 1.0f);
    }


    if(!CheckHit(GetACC(), target->GetEVD()))
    {
        MessageLog::Instance().AddMessage(
            m_Name + u8"の攻撃は外れた。"
        );
        EndTurn();
        return;
    }

    int damage = CalcDamage(GetATK(), target->GetDEF());
    target->TakeDamage(damage,this);

    if (target->IsDead())
    {
        MessageLog::Instance().AddMessage(
            m_Name + u8"は強くなった！"
        );
        m_ATK *= 2;
    }
}
void Enemy::DropItem()
{
    // ドロップ判定
    float roll = GameRandom::Value();
    if (roll > m_Data.dropRate) return;

    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return;

    // 落とすアイテム判定
    const ItemData* itemToDrop = nullptr;
    if (!m_Data.fixedItemId.empty()) {
        // 固定ドロップがある場合
        itemToDrop = ItemDatabase::Get(m_Data.fixedItemId);
    }
    else {
        // 固定がない場合、現在の階層のテーブルから抽選
        FloorData floor = MapManager::Instance()->GetCurrentFloorData();
        itemToDrop = ItemDatabase::DrawFromTable(floor.itemTableId);
    }

    if (!itemToDrop) return;

    auto* itemObj = Manager::GetScene()->AddGameObject<Item>(1);
    ItemInstance inst(itemToDrop);
    inst.InitIdentify(MapManager::Instance() ? MapManager::Instance()->GetDungeonData().IsBlessOrCurseEnabled() : true);
    //回数設定
    if (itemToDrop->type == ItemType::Weapon || itemToDrop->type == ItemType::Shield)
    {
        int plus = GameRandom::Range(0, 3);
        inst.SetPlusValue(plus);

    }
    // 壺の容量設定
    if (itemToDrop->type == ItemType::Pot)
    {
        int cap = GameRandom::Range(4, 6);
        inst.CreatePot(cap, true);
    }

    // 杖の使用回数設定 
    if (itemToDrop->type == ItemType::Staff)
    {
        int charges = GameRandom::Range(4, 6);
        inst.SetCharge(charges);
    }
    if (itemToDrop->type == ItemType::Arrow || itemToDrop->type == ItemType::Stone)
    {
        inst.SetStackCount(GameRandom::Range(3, 8));
    }

    itemObj->SetInstance(std::move(inst));
    itemObj->SetGridPos(m_GridPos);
    itemObj->SetPosition(Vector3(m_GridPos.x * 2.0f, 0.01f, m_GridPos.y * 2.0f));

    map->AddMapObject(itemObj, m_GridPos.x, m_GridPos.y);
}
// 敵が敵対対象を認識できるか。部屋内、部屋外周、視界マスの共通ルールで判定する。
bool Enemy::CanRecognizeHostileTarget(Unit* target, MapData* map)
{
    if (!target || target->GetHP() <= 0 || !map) return false;
    // 仮眠などで行動できない敵は、このターンの索敵対象を持たせない。
    if (IsTurnBlockedByStatus(m_Status)) return false;

    Vector2Int selfPos = GetGridPos();
    Vector2Int targetPos = target->GetGridPos();
    int visionRange = GetDungeonVisionRange();

    Room* selfRoom = map->GetRoomAt(selfPos);
    return GetRecognitionReason(selfRoom, selfPos, targetPos, map, visionRange) != RecognitionReason::None;
}
// 現在ターンの敵対対象認識をクリアする。
void Enemy::ClearTargetRecognition()
{
    m_TargetRecognized = false;
}

Unit* Enemy::FindVisibleHostileTarget(const char* context)
{
    // ログ用途だった呼び出し元情報は、ログ削除後も既存呼び出し互換のため受け取るだけにする。
    (void)context;
    // 仮眠中にターン開始処理から認識済み状態へ入らないよう、索敵入口で止める。
    if (IsTurnBlockedByStatus(m_Status)) {
        ClearTargetRecognition();
        return nullptr;
    }
    if (m_SuppressHostileRecognitionThisTurn) {
        ClearTargetRecognition();
        return nullptr;
    }
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) {
        ClearTargetRecognition();
        return nullptr;
    }

    UnitManager* units = UnitManager::Instance();
    if (!units) {
        ClearTargetRecognition();
        return nullptr;
    }

    Unit* nearest = nullptr;
    float nearestDist = 1e9f;

    Vector2Int selfPos = GetGridPos();
    Room* selfRoom = map->GetRoomAt(selfPos);
    int visionRange = GetDungeonVisionRange();

    auto consider = [&](Unit* unit) {
        if (!unit || unit->GetHP() <= 0) return;

        Vector2Int targetPos = unit->GetGridPos();
        RecognitionReason reason = GetRecognitionReason(selfRoom, selfPos, targetPos, map, visionRange);
        if (reason == RecognitionReason::None) return;

        float dist = Vector2Int::Distance(selfPos, targetPos);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = unit;
        }
    };

    // まずプレイヤーを判定し、その後で仲間も同じ索敵条件で候補に入れる。
    consider(units->GetPlayer());
    for (Ally* ally : units->GetAllies()) {
        consider(ally);
    }
    m_TargetRecognized = nearest != nullptr;
    return nearest;
}
// 仲間含む敵対対象を、視界に入る範囲で探す。主に巡回状態での向き決定などに使う。
Unit* Enemy::FindVisibleNonPlayerHostileTarget()
{
    MapData* map = MapManager::Instance()->GetCurrentMap();
    UnitManager* units = UnitManager::Instance();
    if (!map || !units) return nullptr;

    Unit* nearest = nullptr;
    float nearestDist = 1e9f;
    for (Ally* ally : units->GetAllies()) {
        if (!ally || ally->GetHP() <= 0) continue;
        if (!CanRecognizeHostileTarget(ally, map)) continue;

        float dist = Vector2Int::Distance(GetGridPos(), ally->GetGridPos());
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = ally;
        }
    }
    return nearest;
}

// 追跡対象を完全に見失った場合(ワープして対象がどこかへ行った場合など)、巡回状態に戻す
void Enemy::ReturnToPatrolFromCurrentPos()
{
    state = EnemyState::Patrol;
    currentAI = patrolAI.get();
    ClearTargetRecognition();

    if (auto* chase = dynamic_cast<ChaseAI*>(chaseAI.get())) {
        chase->Reset();
    }

    if (auto* patrol = dynamic_cast<BasicPatrolAI*>(patrolAI.get())) {
        patrol->ResetFromCurrentPos(*this, MapManager::Instance()->GetCurrentMap());
    }
}
void Enemy::ApplyData(const EnemyData& d)
{
    // ステータスのコピー
    m_MaxHP = d.maxHP;
    m_HP = d.maxHP;
    // 勧誘時は最大能力値をコピーするため、現在値だけでなく最大値も敵データに合わせる。
    m_MaxATK = d.atk;
    m_MaxDEF = d.def;
    m_MaxACC = d.acc;
    m_MaxEVD = d.evd;
    m_ATK = m_MaxATK;
    m_DEF = m_MaxDEF;
    m_ACC = m_MaxACC;
    m_EVD = m_MaxEVD;
    m_Name = d.name;
    m_ExpReward = d.expReward;
    m_Scale = d.visual.scale;
    yoffset = d.visual.yOffset;

    float roll = GameRandom::Value();
    if (roll < d.sleepRate)
    {
        SetStatus(Status::Nap, -1);
    }

    AnimationModel* base =
        EnemyModelManager::Instance().GetModel(d.id);

    m_AnimationModel = new AnimationModel();
    m_AnimationModel->CreateClone(*base);


    m_RecruitmentModifier = d.recruitmentModifier;
    m_Skills = d.skills;
    m_Data = d;
    SetBaseActionSpeed(ToTurnSpeed(d.actionSpeed));
    SetBaseMoveSpeed(ToTurnSpeed(d.moveSpeed));

    ResetAI(d.aiType);
}