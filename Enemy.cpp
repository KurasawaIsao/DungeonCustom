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
#include "EffectTargeting.h"
// Enemy は敵ユニット本体。
// TurnManager から UpdateActionPhase と UpdateMovePhase が呼ばれ、
// m_CurrentAI(巡回/追跡/逃走など)を使って「攻撃・特技」と「移動」を分けて実行する。

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
    bool IsWithinVisionRange(const Vector2Int& centerPos, const Vector2Int& targetPos, int visionRange)
    {
        return Vector2Int::ChebyshevDistance(centerPos, targetPos) <= visionRange;
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

    m_Scale = { 0.5f,0.5f,0.5f };

    InitToonShader();

    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);
    SetInitGridPos({ 0,0 });

    PlayAnimation("Idle", 1.0f);
}
bool Enemy::IsVisibleForPlayerUpdate(Player* player) const
{
    // エディタプレビューやプレイヤー未生成時は、確認用表示を優先して常に更新する。
    if (!player || m_EditorPreviewOnly) return true;

    bool visibleToPlayer = false;
    if (IsAnimatingMove()) {
        // 移動中は到達グリッドだけで判定すると表示が先に消えるため、開始位置と見た目位置を確認する。
        visibleToPlayer =
            player->IsInView(WorldToGridForView(m_MoveStartPos)) ||
            player->IsInView(WorldToGridForView(GetVisualPosition()));
    }
    else {
        visibleToPlayer = player->IsInView(GetGridPos());
    }

    // 攻撃や被弾などの演出中は、視界外でも演出完了まで更新・描画を続ける。
    return visibleToPlayer || m_IsActingAnimation;
}

void Enemy::Draw()
{
    if (!m_AnimationModel)return;
    Player* player = UnitManager::Instance()->GetPlayer();
    // 視界外の敵は描画を省き、表示負荷を抑える。
    if (!IsVisibleForPlayerUpdate(player)) return;

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
    ClearPendingCombatActions();
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
    // 描画と同じ視界判定を使い、見えていない敵のアニメ更新を省く。
    if (IsVisibleForPlayerUpdate(player)) {
        UpdateAnimation();
    }

    if (m_IsActingAnimation && (!m_AnimationModel || !m_AnimationModel->IsOneShotPlaying())) {
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
    if (m_EditorPreviewOnly) {
        // エディタの配置プレビューではAIや索敵を動かさず、表示だけ更新する。
        return;
    }
    if (IsTurnBlockedByStatus(m_Status)) {
        return;
    }
    Unit* visibleTarget = nullptr;
    const bool patrolFacing = (m_Data.aiType == EnemyAIType::Patrol ||
        (m_Data.aiType == EnemyAIType::PatrolAndChase && m_State == EnemyState::Patrol));
    if (patrolFacing) {
        visibleTarget = FindVisibleNonPlayerHostileTarget();
    }
    else {
        visibleTarget = FindVisibleHostileTarget("UpdateFacing");
    }
    if (visibleTarget) {
        Vector2Int diff = visibleTarget->GetGridPos() - GetGridPos();
        if (diff.x != 0 || diff.y != 0) {
            m_FacingDir = diff.normalized();
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
    if (m_IsDead || IsActionPhaseChecked()) return;
    RepairInvalidGridPos("Enemy::UpdateActionPhase");
    SetActionPhaseChecked(true);
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
    if (auto* berserk = dynamic_cast<BerserkAI*>(m_CurrentAI)) {
        target = berserk->FindTarget(*this, map);
    }
    else if (m_Data.aiType != EnemyAIType::Passive && m_Data.aiType != EnemyAIType::RunAway) {
        target = FindVisibleHostileTarget("UpdateActionPhase");
    }
    if (!target) {
        // 索敵範囲から外れた時点で追跡状態を終わらせ、後続の移動フェーズは現在位置から巡回を選び直す。
        if (m_State == EnemyState::Chase && UsesHostileRecognitionState(m_Data.aiType)) {
            ReturnToPatrolFromCurrentPos();
        }
        ConsumeAllActions();
        return;
    }
    const bool isChaseAI = dynamic_cast<ChaseAI*>(m_CurrentAI) != nullptr;
    const bool isChaseAdjacent =
        (isChaseAI && m_CurrentAI && m_CurrentAI->IsAdjacent(*this, target));
    const bool isKeepDistanceAdjacent =
        (m_Data.aiType == EnemyAIType::KeepDistance && m_CurrentAI && m_CurrentAI->IsAdjacent(*this, target));
    const bool canAttackTarget = m_CurrentAI && m_CurrentAI->IsAttackAdjacent(*this, target, map);

    // 行動フェーズでは、特技または通常攻撃だけを行う。移動は後続の移動フェーズに回す。
    if (canAttackTarget) {
        if (auto* whim = dynamic_cast<WhimAI*>(m_CurrentAI)) {
            if (!whim->ShouldAttackAdjacent()) {
                ConsumeAction();
                ConsumeAllMoves();
                return;
            }
        }
    }

    if (m_CurrentAI && m_CurrentAI->ExecuteSkill(*this, target)) {
        if (isKeepDistanceAdjacent || isChaseAdjacent) ConsumeAllMoves();
        EndTurn();
        if (!CanActThisTurn()) ConsumeAllMoves();
        return;
    }
    if (m_Data.aiType == EnemyAIType::KeepDistance && m_CurrentAI && !m_CurrentAI->IsAdjacent(*this, target)) {
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
    if (m_IsDead || IsMovePhaseChecked() || IsAnimatingMove()) return;
    RepairInvalidGridPos("Enemy::UpdateMovePhase");

    SetMovePhaseChecked(true);
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

    if (auto* chase = dynamic_cast<ChaseAI*>(m_CurrentAI)) {
        Unit* target = FindVisibleHostileTarget("UpdateMovePhase:Chase");
        if (!target) {
            ReturnToPatrolFromCurrentPos();
            // 見失ったターンも停止せず、現在位置から巡回ルートを選び直す。
            if (m_CurrentAI) m_CurrentAI->Update(*this, map);
        }
        else {
            UnitMovementPlanner::MoveToTargetByAreaAndEndTurn(*this, target, map, *chase, 3, 3, true);

            // 見失い判定は行動開始時にまとめる。移動直後に部屋/通路の基準を切り替えると、入口や角で不自然に追跡が切れやすい。
        }
    }
    else if (auto* keep = dynamic_cast<KeepDistAI*>(m_CurrentAI)) {
        Unit* target = FindVisibleHostileTarget("UpdateMovePhase:KeepDistance");
        keep->UpdateWithTarget(*this, target, map);
    }
    else if (auto* runAway = dynamic_cast<RunAwayAI*>(m_CurrentAI)) {
        Unit* target = FindVisibleHostileTarget("UpdateMovePhase:RunAway");
        runAway->MoveAwayFromTarget(*this, target, nullptr, map);
    }
    else if (auto* berserk = dynamic_cast<BerserkAI*>(m_CurrentAI)) {
        Unit* target = berserk->FindTarget(*this, map);
        if (target && m_CurrentAI->IsAdjacent(*this, target)) {
            ConsumeAllMoves();
        }
        else if (m_CurrentAI) {
            m_CurrentAI->Update(*this, map);
        }
    }
    else if (auto* whim = dynamic_cast<WhimAI*>(m_CurrentAI)) {
        Unit* target = FindVisibleHostileTarget("UpdateMovePhase:Whim");
        if (target && m_CurrentAI->IsAdjacent(*this, target)) {
            ConsumeAllMoves();
        }
        else {
            whim->UpdateWithTarget(*this, target, map);
        }
    }
    else if (m_CurrentAI) {
        m_CurrentAI->Update(*this, map);
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

    if (m_State == EnemyState::Patrol)
    {
        if (visibleTarget)
        {
            m_State = EnemyState::Chase;
            if (m_ChaseAI) m_CurrentAI = m_ChaseAI.get();
            m_TargetRecognized = true;
            Vector2Int dir = visibleTarget->GetGridPos() - this->GetGridPos();
            this->m_FacingDir = dir.normalized();
            this->LookAt(dir);
        }
    }
    else if (m_State == EnemyState::Chase)
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
        return Vector2Int::ChebyshevDistance(pos, ePos) <= 1;
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
void Enemy::SetHostileRecognitionSuppressed(bool suppressed)
{
    // 吹き飛ばしなどの強制移動後は、その敵ターン中だけ索敵を止めて追跡を切る。
    m_SuppressHostileRecognitionThisTurn = suppressed;
    if (suppressed) {
        ReturnToPatrolFromCurrentPos();
    }
}
void Enemy::SetShopKeeperMode(bool hostile)
{
    m_IsShopKeeper = true;
    m_IsShopHostile = hostile;
    m_PatrolAI = std::make_unique<ShopUnitAI>(hostile);
    m_ChaseAI.reset();
    m_CurrentAI = m_PatrolAI.get();
    ClearStatus();
}
void Enemy::ResetAI(EnemyAIType aiType)
{
    m_PatrolAI.reset();
    m_ChaseAI.reset();
    m_CurrentAI = nullptr;
    m_State = EnemyState::Patrol;
    ClearTargetRecognition();

    switch (aiType)
    {
    case EnemyAIType::Patrol:
        m_PatrolAI = std::make_unique<BasicPatrolAI>();
        m_CurrentAI = m_PatrolAI.get();
        break;
    case EnemyAIType::PatrolAndChase:
        m_PatrolAI = std::make_unique<BasicPatrolAI>();
        m_ChaseAI = std::make_unique<ChaseAI>();
        m_CurrentAI = m_PatrolAI.get();
        break;
    case EnemyAIType::KeepDistance:
        m_PatrolAI = std::make_unique<KeepDistAI>((std::max)(1, m_Data.keepDistance));
        m_CurrentAI = m_PatrolAI.get();
        break;
    case EnemyAIType::Berserk:
    {
        auto berserk = std::make_unique<BerserkAI>();
        berserk->SetVisionRange((std::max)(1, m_Data.visionRange));
        m_PatrolAI = std::move(berserk);
        m_CurrentAI = m_PatrolAI.get();
        break;
    }
    case EnemyAIType::RunAway:
        m_PatrolAI = std::make_unique<RunAwayAI>();
        m_CurrentAI = m_PatrolAI.get();
        break;
    case EnemyAIType::WhimAlwaysAttack:
        m_PatrolAI = std::make_unique<WhimAI>(true);
        m_CurrentAI = m_PatrolAI.get();
        break;
    case EnemyAIType::WhimRandomAttack:
        m_PatrolAI = std::make_unique<WhimAI>(false);
        m_CurrentAI = m_PatrolAI.get();
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
    m_IsDead = true;
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
                TurnManager::Instance()->SetTurnProgressionPaused(true);
                ui->OpenRecruitMenu(this);
                return;
            }
        }
    }
    UnitManager::Instance()->RemoveEnemy(this);
    m_CurrentAI = nullptr;
    m_PatrolAI.reset();
    m_ChaseAI.reset();
    MessageLog::Instance().AddMessage(
        m_Name + u8"はちからつきた。"
    );
   StopLoopEffect();
   SetDestroy();
}

void Enemy::StartAttackWithNotify(Unit* target)
{
    if (!target || target->GetHP() <= 0) return;

    // 回避判定を先に確定し、Notifyを待たず行動内容の一文を表示する。
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
        m_IsActingAnimation = true;
        SetTriggerAnimation("Attack", 1.0f);
        return;
    }

    // Notify未登録時は即時処理へ戻すが、表示可能なら攻撃モーション自体は再生する。
    ResolvePendingAttack();
    if (showVisual) SetTriggerAnimation("Attack", 1.0f);
}

void Enemy::ResolvePendingAttack()
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

    if (target->IsDead())
    {
        MessageLog::Instance().AddMessage(m_Name + u8"は強くなった！");
        m_ATK *= 2;
    }
}

bool Enemy::QueueSkillForNotify(
    const Skill& skill,
    const EffectContext& context,
    const std::vector<Unit*>& targets)
{
    // Notify未登録時は効果を保留せず、呼び出し側で即時適用へ戻す。
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

void Enemy::ResolvePendingSkill()
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

    // 複数対象は特技決定時に収集した対象へ適用し、Notify待機中の位置変化で範囲を変えない。
    for (Unit* target : targets)
    {
        if (!target || target->GetHP() <= 0) continue;
        EffectContext each = context;
        each.target = target;
        each.pos = target->GetGridPos();
        effect->Apply(each);
    }
}

void Enemy::ClearPendingCombatActions()
{
    m_PendingAttackTarget = nullptr;
    m_PendingAttackHit = false;
    m_PendingSkillEffect.reset();
    m_PendingSkillContext = {};
    m_PendingSkillTargets.clear();
}

void Enemy::OnAnimationNotify(
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

void Enemy::Attack()
{
    // 攻撃ごとに表示中ログをリセットする。履歴は MessageLog 側に残す。
    MessageLog::Instance().Clear();

    Vector2Int targetPos = m_GridPos + m_FacingDir;
    MapData* map = MapManager::Instance()->GetCurrentMap();
    UnitManager* units = UnitManager::Instance();
    if (!units) return;

    if (m_FacingDir.Chebyshev(Vector2Int(0, 0)) == 1 && m_FacingDir.Manhattan(Vector2Int(0, 0)) == 2) {
        if (map && IsDiagonalMoveBlocked(m_GridPos, m_FacingDir, map)) {
            EndTurn();
            return;
        }
    }

    Unit* target = units->GetUnitAt(targetPos);

	// 無差別攻撃AI取得
    const bool attacksAnyUnit = dynamic_cast<BerserkAI*>(m_CurrentAI) != nullptr;

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
        Unit* bestTarget = nullptr;
        int bestDist = 999999;

        auto consider = [&](Unit* candidate) {
            if (!canAttackTarget(candidate)) return;
            Vector2Int dir = candidate->GetGridPos() - m_GridPos;
            int chebyshev = dir.Chebyshev(Vector2Int(0, 0));
            int manhattan = dir.Manhattan(Vector2Int(0, 0));
            if (chebyshev == 0) return;
            if (chebyshev > 1) return;
            if (chebyshev == 1 && manhattan == 2 && map && IsDiagonalMoveBlocked(m_GridPos, dir, map)) return;

            int score = manhattan;
            if (score < bestDist) {
                bestDist = score;
                bestTarget = candidate;
            }
        };

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

    StartAttackWithNotify(target);
}
void Enemy::DropItem()
{
    // ドロップ判定
    float roll = GameRandom::Value();
    if (roll > m_Data.dropRate) return;

    MapManager* mapManager = MapManager::Instance();
    MapData* map = mapManager ? mapManager->GetCurrentMap() : nullptr;
    if (!map) return;

    // 落とすアイテム判定
    const ItemData* itemToDrop = nullptr;
    if (!m_Data.fixedItemId.empty()) {
        // 固定ドロップがある場合
        itemToDrop = ItemDatabase::Get(m_Data.fixedItemId);
    }
    else {
        // 固定がない場合、現在の階層のテーブルから抽選
        FloorData floor = mapManager->GetCurrentFloorData();
        itemToDrop = ItemDatabase::DrawFromTable(floor.itemTableId);
    }

    if (!itemToDrop) return;

    auto* itemObj = Manager::GetScene()->AddGameObject<Item>(1);
    ItemInstance inst(itemToDrop);
    inst.InitIdentify(mapManager ? mapManager->GetDungeonData().IsBlessOrCurseEnabled() : true);
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
    m_State = EnemyState::Patrol;
    m_CurrentAI = m_PatrolAI.get();
    ClearTargetRecognition();

    if (auto* chase = dynamic_cast<ChaseAI*>(m_ChaseAI.get())) {
        chase->Reset();
    }

    if (auto* patrol = dynamic_cast<BasicPatrolAI*>(m_PatrolAI.get())) {
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
    m_YOffset = d.visual.yOffset;

    AnimationModel* base =
        EnemyModelManager::Instance().GetModel(d.id);

    m_AnimationModel = new AnimationModel();
    m_AnimationModel->CreateClone(*base);

    // 敵データに定義されたタイミングを、この敵個体のAnimationModelへ登録する。
    m_AnimationModel->ClearAllAnimationNotifies();
    for (const EnemyAnimationNotifyData& notify : d.visual.animationNotifies)
    {
        m_AnimationModel->AddAnimationNotifyNormalized(
            notify.animationName,
            notify.normalizedTime,
            notify.notifyName);
    }

    m_RecruitmentModifier = d.recruitmentModifier;
    m_Skills = d.skills;
    m_Data = d;

    // 仮眠アニメーションを確実に開始できるよう、モデル生成後に状態を設定する。
    if (GameRandom::Value() < d.sleepRate)
    {
        SetStatus(Status::Nap, -1);
    }

    SetBaseActionSpeed(ToTurnSpeed(d.actionSpeed));
    SetBaseMoveSpeed(ToTurnSpeed(d.moveSpeed));

    ResetAI(d.aiType);
}