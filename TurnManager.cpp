#include "TurnManager.h"
#include "UnitManager.h"
#include "Player.h"
#include "Enemy.h"
#include "MessageLog.h"
#include "MapManager.h"
#include "scene.h"
#include "manager.h"
#include "Item.h"
#include "FlyingObject.h"
#include "Ally.h"
#include "EnemyDatabase.h"
#include "MapData.h"
#include "MiniMapRenderer.h"
#include "DungeonEndingUI.h"
#include <vector>

TurnManager* TurnManager::instance = nullptr;

// ターン進行を扱うステートマシン。
// 基本順序は PlayerTurn -> EnemyAction -> EnemyMove -> MoveResolution -> PlayerTurn。
// 倍速/三倍速は Unit の ActionBudget/MoveBudget を使い、同じフェーズを必要回数だけ回す。

namespace
{
    bool IsAnyFlyingObjectActive()
    {
        for (auto* it : Manager::GetScene()->GetGameObjects<Item>()) {
            if (it->GetIsFlying()) return true;
        }
        for (auto* obj : Manager::GetScene()->GetGameObjects<FlyingObject>()) {
            if (obj->GetIsActive()) return true;
        }
        return false;
    }

    void UpdateMoveAnimation(Unit* unit)
    {
        if (unit && unit->IsAnimatingMove()) unit->UpdateLerpMove();
    }

    void UpdateAllMoveAnimations(Player* player, const std::vector<Enemy*>& enemies, const std::vector<Ally*>& allies)
    {
        UpdateMoveAnimation(player);
        for (Enemy* e : enemies) UpdateMoveAnimation(e);
        for (Ally* a : allies) UpdateMoveAnimation(a);
    }

    bool IsAnyUnitMoving(Player* player, const std::vector<Enemy*>& enemies, const std::vector<Ally*>& allies)
    {
        if (player && player->IsAnimatingMove()) return true;
        for (Enemy* e : enemies) if (e && e->IsAnimatingMove()) return true;
        for (Ally* a : allies) if (a && a->IsAnimatingMove()) return true;
        return false;
    }

    bool IsAnyUnitActing(Player* player, const std::vector<Enemy*>& enemies, const std::vector<Ally*>& allies)
    {
        if (player && player->IsActing()) return true;
        for (Enemy* e : enemies) if (e && e->IsActing()) return true;
        for (Ally* a : allies) if (a && a->IsActing()) return true;
        return false;
    }

    bool IsAnyActionEffectActive(Player* player, const std::vector<Enemy*>& enemies, const std::vector<Ally*>& allies)
    {
        // 攻撃・特技は同時に始めない。誰かの演出や飛び道具が残っている間は次の行動を待たせる。
        return IsAnyUnitActing(player, enemies, allies) || IsAnyFlyingObjectActive();
    }

    template <typename TUnit>
    bool UpdateActionPhaseOneByOne(TUnit* unit, bool& loopHadProgress)
    {
        if (!unit || unit->IsActionPhaseChecked() || unit->IsActing() || unit->IsAnimatingMove()) return false;

        int budgetBefore = unit->GetActionBudget();
        bool wasActing = unit->IsActing();
        unit->UpdateActionPhase();

        // 行動予算の消費は倍速/三倍速の追加処理判定に使うが、それだけではフレームを止めない。
        // 攻撃/特技演出や飛び道具など、見た目として待つ必要がある時だけ次フレームへ回す。
        bool budgetChanged = budgetBefore != unit->GetActionBudget();
        bool shouldWaitVisual =
            (!wasActing && unit->IsActing()) ||
            IsAnyFlyingObjectActive();
        if (budgetChanged || shouldWaitVisual) loopHadProgress = true;
        return shouldWaitVisual;
    }
    bool HasExtraMoveSpeed(Unit* unit)
    {
        if (!unit) return false;
        TurnSpeed speed = unit->GetMoveSpeed();
        return speed == TurnSpeed::Fast || speed == TurnSpeed::Triple;
    }

    bool IsAttackAdjacent(Unit* self, Unit* target, MapData* map)
    {
        if (!self || !target || !map) return false;

        Vector2Int selfPos = self->GetGridPos();
        Vector2Int targetPos = target->GetGridPos();
        int dx = targetPos.x - selfPos.x;
        int dy = targetPos.y - selfPos.y;
        int adx = abs(dx);
        int ady = abs(dy);

        if (adx == 0 && ady == 0) return false;
        if (adx + ady == 1) return true;
        if (adx == 1 && ady == 1) {
            return !self->IsDiagonalMoveBlocked(selfPos, { dx, dy }, map);
        }
        return false;
    }

	// プレイヤーの移動完了後に攻撃可能なターゲットがいるか。敵はプレイヤーと味方を、味方は敵を攻撃する想定。
    bool HasPostMoveAttackTarget(Unit* unit, MapData* map)
    {
        if (!unit || !map) return false;
        UnitManager* um = UnitManager::Instance();

        if (dynamic_cast<Enemy*>(unit)) {
            if (IsAttackAdjacent(unit, um->GetPlayer(), map)) return true;
            for (Ally* ally : um->GetAllies()) {
                if (IsAttackAdjacent(unit, ally, map)) return true;
            }
            return false;
        }

        if (dynamic_cast<Ally*>(unit)) {
            for (Enemy* enemy : um->GetEnemies()) {
                if (IsAttackAdjacent(unit, enemy, map)) return true;
            }
            return false;
        }

        return false;
    }

        bool ShouldRunPostMoveAction(Unit* unit, MapData* map)
    {
        if (!unit || !unit->CanActThisTurn()) return false;
        if (!HasPostMoveAttackTarget(unit, map)) return false;
        if (dynamic_cast<Ally*>(unit) && !unit->HasMovedThisTurn()) return true;
        return HasExtraMoveSpeed(unit) || unit->HasActionConsumedByMoveThisTurn();
    }

    bool HasAnyImmediateCombatTarget(MapData* map)
    {
        if (!map) return false;

        UnitManager* um = UnitManager::Instance();
        Player* player = um->GetPlayer();
        for (Enemy* enemy : um->GetEnemies()) {
            if (!enemy || enemy->GetHP() <= 0) continue;
            if (IsAttackAdjacent(enemy, player, map)) return true;
            for (Ally* ally : um->GetAllies()) {
                if (IsAttackAdjacent(enemy, ally, map)) return true;
            }
        }

        for (Ally* ally : um->GetAllies()) {
            if (!ally || ally->GetHP() <= 0) continue;
            for (Enemy* enemy : um->GetEnemies()) {
                if (IsAttackAdjacent(ally, enemy, map)) return true;
            }
        }

        return false;
    }

}

void TurnManager::StartPlayerTurn()
{
    // 1サイクルの開始。プレイヤーのターン予算と状態異常カウントは Unit::OnTurnStart 側で整える。
    m_Phase = Phase::PlayerTurn;

    Player* player = UnitManager::Instance()->GetPlayer();
    if (player)
    {
        player->OnTurnStart();
    }
}

void TurnManager::StartEnemyTurn()
{
    // プレイヤー行動後、敵と仲間の行動フェーズに入る前の準備。
    // 敵はこの時点で巡回/追跡などの AI 状態を決め直す。
    m_Phase = Phase::EnemyAction;
    m_EnemyActionLoopHadProgress = false;

    auto enemies = UnitManager::Instance()->GetEnemies();
    for (Enemy* e : enemies)
    {
        if (e)
        {
            e->DecideNextAction();
            e->OnTurnStart();
        }
    }

    auto allies = UnitManager::Instance()->GetAllies();
    for (Ally* a : allies)
    {
        if (a)
        {
            a->OnTurnStart();
        }
    }
}


void TurnManager::FinishTurnCycle()
{
    // 全ユニットの行動と移動が解決したらターン数を進め、自然湧きを判定して次のプレイヤーターンへ戻る。
    for (Enemy* enemy : UnitManager::Instance()->GetEnemies()) {
        if (enemy) enemy->ClearHostileRecognitionSuppression();
    }
    m_TurnCount++;
    if (HandleWindTurnLimit()) return;
    SpawnEnemy();
    StartPlayerTurn();
}

bool TurnManager::HandleWindTurnLimit()
{
    // 現在フロアの風ターン設定を見て、残りターン警告と制限到達時のゲームオーバーを処理する。
    MapManager* mapManager = MapManager::Instance();
    if (!mapManager) return false;

    const int windTurnLimit = mapManager->GetCurrentFloorData().windTurnLimit;
    if (windTurnLimit <= 0) return false;

    const int remainingTurns = windTurnLimit - m_TurnCount;
    if (remainingTurns <= 0)
    {
        StartWindGameOver();
        return true;
    }

    auto announceWind = [&](int threshold, bool& shown)
    {
        if (shown || remainingTurns != threshold) return;

        MessageLog::Instance().AddMessage(
            u8"強い風が吹き始めた。あと" + std::to_string(threshold) + u8"ターンで飛ばされそうだ。");
        shown = true;
    };

    announceWind(200, m_WindWarning200Shown);
    announceWind(100, m_WindWarning100Shown);
    announceWind(50, m_WindWarning50Shown);
    return false;
}

void TurnManager::StartWindGameOver()
{
    // 風ターンが尽きた時は、既存の死亡演出UIに原因を渡してゲームオーバーへ移行する。
    Scene* scene = Manager::GetScene();
    if (!scene) return;

    Player* player = UnitManager::Instance()->GetPlayer();
    const std::string playerName = player ? player->GetName() : std::string(u8"プレイヤー");

    MapManager* mapManager = MapManager::Instance();
    const std::string dungeonName = mapManager ? mapManager->GetDungeonData().GetDungeonId() : std::string(u8"ダンジョン");

    if (auto* ending = scene->GetGameObject<DungeonEndingUI>())
    {
        if (player) player->SetVisible(false);
        ending->StartDeath(playerName, dungeonName, u8"風で飛ばされてしまった");
    }
}

void TurnManager::Update()
{
    // 毎フレーム呼ばれるが、ターンは「演出待ち」と「フェーズ進行」を分けて少しずつ進める。
    // phaseGuard は、同一フレーム内で終わる空フェーズが続いた時の無限ループ保険。
    if (m_IsPaused) return;

    if (Manager::GetScene())
    {
        if (auto* miniMap = Manager::GetScene()->GetGameObject<MiniMapRenderer>())
        {
            if (miniMap->IsLookMode()) return;
        }
    }

    UnitManager* um = UnitManager::Instance();
    Player* player = um->GetPlayer();
    if (!player) return;

    auto& movementEnemies = um->GetEnemies();
    auto& movementAllies = um->GetAllies();
    MapData* currentMap = MapManager::Instance()->GetCurrentMap();
    const bool isActionPhase = (m_Phase == Phase::EnemyAction || m_Phase == Phase::EnemyPostMoveAction);
    const bool shouldResolvePlayerMoveBeforeCombat =
        player->IsAnimatingMove() && isActionPhase && HasAnyImmediateCombatTarget(currentMap);
    const bool hasActionEffectActive =
        isActionPhase && IsAnyActionEffectActive(player, movementEnemies, movementAllies);

    // プレイヤー以外同士の戦闘も含め、戦闘が起きる場合はプレイヤーの移動を先に完了させる。
    // 戦闘が起こらない場合だけ、敵味方の移動フェーズまでプレイヤーを待たせて一斉移動にする。
    // ただし攻撃/特技/飛び道具の演出待ち中は、プレイヤー移動を止めると移動中表示が残るため補間を進める。
    const bool holdPlayerMoveForSimultaneousMove =
        player->IsAnimatingMove() &&
        isActionPhase &&
        !shouldResolvePlayerMoveBeforeCombat &&
        !hasActionEffectActive;
    if (!holdPlayerMoveForSimultaneousMove) UpdateMoveAnimation(player);
    for (Enemy* e : movementEnemies) UpdateMoveAnimation(e);
    for (Ally* a : movementAllies) UpdateMoveAnimation(a);
    for (int phaseGuard = 0; phaseGuard < 8; ++phaseGuard)
    {
        bool advancePhase = false;

        switch (m_Phase)
        {
        case Phase::PlayerTurn:
        {
            // 入力受付やプレイヤーの攻撃/移動は Player::UpdateUnit に集約。
            // 行動済み、演出終了、投擲物なし、の条件が揃ってから敵ターンへ進む。
            if (HandleWindTurnLimit()) return;

            const bool hadActiveFlyingObject = IsAnyFlyingObjectActive();
            if (!hadActiveFlyingObject)
            {
                player->UpdateUnit();
            }

            if (m_IsPaused) return;

            const bool hasActiveFlyingObject = IsAnyFlyingObjectActive();
            if (player->HasActed() && !player->IsActing() && !hasActiveFlyingObject)
            {
                StartEnemyTurn();
                advancePhase = true;
            }
            break;
        }
        case Phase::EnemyAction:
        {
            // 敵/仲間の「攻撃・特技」だけを処理するフェーズ。
            // 移動は EnemyMove へ分けることで、倍速時の行動回数と移動回数を独立して扱える。
            bool allChecked = true;
            auto& enemies = um->GetEnemies();
            auto& allies = um->GetAllies();

            if (player->IsAnimatingMove() && HasAnyImmediateCombatTarget(MapManager::Instance()->GetCurrentMap())) {
                break;
            }
            if (IsAnyActionEffectActive(player, enemies, allies)) {
                break;
            }

            bool actionStarted = false;
            for (Enemy* e : enemies)
            {
                if (!e) continue;
                if (UpdateActionPhaseOneByOne(e, m_EnemyActionLoopHadProgress)) {
                    actionStarted = true;
                }
                if (!e->IsActionPhaseChecked() || e->IsActing() || e->IsAnimatingMove()) {
                    allChecked = false;
                }
                if (actionStarted) break;
            }

            if (!actionStarted) {
                for (Ally* a : allies)
                {
                    if (!a) continue;
                    if (UpdateActionPhaseOneByOne(a, m_EnemyActionLoopHadProgress)) {
                        actionStarted = true;
                    }
                    if (!a->IsActionPhaseChecked() || a->IsActing() || a->IsAnimatingMove()) {
                        allChecked = false;
                    }
                    if (actionStarted) break;
                }
            }

            if (actionStarted) {
                allChecked = false;
            }
            if (allChecked)
            {
                bool hasExtraAction = false;
                for (Enemy* e : enemies)
                {
                    if (e && e->CanActThisTurn()) {
                        hasExtraAction = true;
                    }
                }
                for (Ally* a : allies)
                {
                    if (a && a->CanActThisTurn()) {
                        hasExtraAction = true;
                    }
                }

                if (hasExtraAction && m_EnemyActionLoopHadProgress) {
                    for (Enemy* e : enemies)
                    {
                        if (e && e->CanActThisTurn()) e->ResetActionPhaseCheck();
                    }
                    for (Ally* a : allies)
                    {
                        if (a && a->CanActThisTurn()) a->ResetActionPhaseCheck();
                    }
                    m_EnemyActionLoopHadProgress = false;
                }
                else {
                    m_EnemyActionLoopHadProgress = false;
                    m_Phase = Phase::EnemyMove;
                    advancePhase = true;
                }
            }
            break;
        }
        case Phase::EnemyMove:
        {
            // 攻撃後に移動を処理するフェーズ。
            // まだ移動予算が残っているユニットはチェックを戻し、同じフェーズ内でもう一度動かす。
            bool allChecked = true;
            auto& enemies = um->GetEnemies();
            auto& allies = um->GetAllies();

            for (Enemy* e : enemies)
            {
                if (!e) continue;
                if (!e->IsMovePhaseChecked() && !e->IsActing() && !e->IsAnimatingMove())
                {
                    e->UpdateMovePhase();
                }
                if (!e->IsMovePhaseChecked() || e->IsActing() || e->IsAnimatingMove())
                    allChecked = false;
            }

            for (Ally* a : allies)
            {
                if (!a) continue;
                if (!a->IsMovePhaseChecked() && !a->IsActing() && !a->IsAnimatingMove())
                {
                    a->UpdateMovePhase();
                }
                if (!a->IsMovePhaseChecked() || a->IsActing() || a->IsAnimatingMove())
                    allChecked = false;
            }

            if (allChecked)
            {
                bool hasExtraMove = false;
                for (Enemy* e : enemies)
                {
                    if (e && e->CanMoveThisTurn()) {
                        e->ResetMovePhaseCheck();
                        hasExtraMove = true;
                    }
                }
                for (Ally* a : allies)
                {
                    if (a && a->CanMoveThisTurn()) {
                        a->ResetMovePhaseCheck();
                        hasExtraMove = true;
                    }
                }

                if (!hasExtraMove) {
                    m_Phase = Phase::MoveResolution;
                    advancePhase = true;
                }
            }
            break;
        }
        case Phase::MoveResolution:
        {
            // StartMove で始まった補間移動が全員終わるのを待つ。
            // 倍速ユニットは移動後に追加行動できるため、必要なら EnemyPostMoveAction に入る。
            auto& enemies = um->GetEnemies();
            auto& allies = um->GetAllies();

            if (!IsAnyUnitMoving(player, enemies, allies))
            {
                // 鉄球の吹き飛びが壁/ユニットに当たった時は、移動完了後に被ダメージ演出が始まる。
                // その演出を待たずに次のプレイヤーターンへ入ると、次の行動後に遅れて待ち時間が出る。
                if (IsAnyActionEffectActive(player, enemies, allies)) break;

                bool hasPostMoveAction = false;
                for (Enemy* e : enemies)
                {
                    if (ShouldRunPostMoveAction(e, MapManager::Instance()->GetCurrentMap())) {
                        e->ResetActionPhaseCheck();
                        hasPostMoveAction = true;
                    }
                }
                for (Ally* a : allies)
                {
                    if (ShouldRunPostMoveAction(a, MapManager::Instance()->GetCurrentMap())) {
                        a->ResetActionPhaseCheck();
                        hasPostMoveAction = true;
                    }
                }

                if (hasPostMoveAction) {
                    m_EnemyActionLoopHadProgress = false;
                    m_Phase = Phase::EnemyPostMoveAction;
                    advancePhase = true;
                }
                else {
                    FinishTurnCycle();
                }
            }
            break;
        }
        case Phase::EnemyPostMoveAction:
        {
            // 倍速/三倍速の「移動後攻撃」専用フェーズ。
            // 通常速度の敵や仲間はここでは処理しない。
            bool allChecked = true;
            auto& enemies = um->GetEnemies();
            auto& allies = um->GetAllies();

            if (IsAnyUnitMoving(player, enemies, allies)) break;
            if (IsAnyActionEffectActive(player, enemies, allies)) break;

            bool actionStarted = false;
            for (Enemy* e : enemies)
            {
                if (!ShouldRunPostMoveAction(e, MapManager::Instance()->GetCurrentMap())) continue;
                if (UpdateActionPhaseOneByOne(e, m_EnemyActionLoopHadProgress)) {
                    actionStarted = true;
                }
                if (!e->IsActionPhaseChecked() || e->IsActing() || e->IsAnimatingMove()) {
                    allChecked = false;
                }
                if (actionStarted) break;
            }

            if (!actionStarted) {
                for (Ally* a : allies)
                {
                    if (!ShouldRunPostMoveAction(a, MapManager::Instance()->GetCurrentMap())) continue;
                    if (UpdateActionPhaseOneByOne(a, m_EnemyActionLoopHadProgress)) {
                        actionStarted = true;
                    }
                    if (!a->IsActionPhaseChecked() || a->IsActing() || a->IsAnimatingMove()) {
                        allChecked = false;
                    }
                    if (actionStarted) break;
                }
            }

            if (actionStarted) {
                allChecked = false;
            }
            if (allChecked)
            {
                bool hasExtraAction = false;
                for (Enemy* e : enemies)
                {
                    if (ShouldRunPostMoveAction(e, MapManager::Instance()->GetCurrentMap())) {
                        hasExtraAction = true;
                    }
                }
                for (Ally* a : allies)
                {
                    if (ShouldRunPostMoveAction(a, MapManager::Instance()->GetCurrentMap())) {
                        hasExtraAction = true;
                    }
                }

                if (hasExtraAction && m_EnemyActionLoopHadProgress) {
                    for (Enemy* e : enemies)
                    {
                        if (ShouldRunPostMoveAction(e, MapManager::Instance()->GetCurrentMap())) e->ResetActionPhaseCheck();
                    }
                    for (Ally* a : allies)
                    {
                        if (ShouldRunPostMoveAction(a, MapManager::Instance()->GetCurrentMap())) a->ResetActionPhaseCheck();
                    }
                    m_EnemyActionLoopHadProgress = false;
                }
                else {
                    m_EnemyActionLoopHadProgress = false;
                    FinishTurnCycle();
                }
            }
            break;
        }
        case Phase::AllyAction:
        case Phase::AllyMove:
        {
            m_Phase = Phase::MoveResolution;
            advancePhase = true;
            break;
        }
        }

        if (!advancePhase) break;
    }
}
void TurnManager::ResolveAfterPlayerInstantMove()
{
    // ダッシュなど即時移動でプレイヤーだけ複数マス進んだ時、
    // 敵ターンをアニメーションなしで最後まで解決して入力待ちへ戻す。
    bool prevSkip = Unit::IsSkipMoveAnimation();
    Unit::SetSkipMoveAnimation(true);

    StartEnemyTurn();

    for (int guard = 0; guard < 1000 && m_Phase != Phase::PlayerTurn; ++guard)
    {
        Update();
    }

    Unit::SetSkipMoveAnimation(prevSkip);
}
void TurnManager::SpawnEnemy()
{
    // 一定ターンごとの自然湧き。通常時と泥棒時で上限・間隔・湧く敵を変える。
    UnitManager* um = UnitManager::Instance();

    // 上限チェック
    const int enemyLimit = m_ShopTheftMode ? 30 : 15;
    if (um->GetEnemyCount() >= enemyLimit)
        return;

    // ターン間隔チェック
    const int spawnInterval = m_ShopTheftMode ? THEFT_SPAWN_INTERVAL : SPAWN_INTERVAL;
    if (m_TurnCount % spawnInterval != 0)
        return;

    // 現在フロア情報取得
    const FloorData& floor =
        MapManager::Instance()->GetCurrentFloorData();

    MapGenerator generator;
    generator.SetEnemiesOne(floor);
}
