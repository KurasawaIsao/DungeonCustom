#include "GameRandom.h"
#include "player.h"
#include "input.h"
#include "camera.h"
#include "manager.h"
#include "scene.h"
#include "MapManager.h"
#include "UnitManager.h"
#include "TurnManager.h"
#include "Item.h"
#include "PlayerInventoryUI.h"
#include "ShopUI.h"
#include "LightManager.h"
#include "FloorTransitionUI.h"
#include "ConfirmWindow.h"
#include "MiniMapRenderer.h"
#include "MessageLog.h"
#include "Enemy.h"
#include "Ally.h"
#include "MapObject.h"
#include "FlyingObject.h"
#include "EffectBillboard.h"
#include "DungeonEndingUI.h"
#include "AnimationModel.h"
#include "Trap.h"

// Player は入力をターン行動へ変換する Unit。
// Update はアニメーション・階段確認など毎フレーム必要な処理、
// UpdateUnit は TurnManager の PlayerTurn 中だけ呼ばれる実際の行動受付を担当する。

namespace
{
    const Vector2Int CONFUSION_DIRS[8] = {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
        { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
    };

    Vector2Int RandomConfusionDir()
    {
        return CONFUSION_DIRS[GameRandom::Range(0, 7)];
    }

    bool IsNormalOrSlowerAction(Unit* unit)
    {
        if (!unit) return false;
        TurnSpeed speed = unit->GetActionSpeed();
        return speed == TurnSpeed::Slow || speed == TurnSpeed::Normal;
    }
}

void Player::Init() {
    m_AnimationModel = new AnimationModel();
    m_AnimationModel->Load("Asset\\Model\\T.fbx");
    m_AnimationModel->LoadAnimation("Asset\\Model\\TIdle.fbx", "Idle");
    m_AnimationModel->LoadAnimation("Asset\\Model\\TRun.fbx", "Run");
    m_AnimationModel->LoadAnimation("Asset\\Model\\TAttack.fbx", "Attack");
    m_AnimationModel->LoadAnimation("Asset\\Model\\TDamaged.fbx", "Damaged");
    m_AnimationModel->LoadAnimation("Asset\\Model\\TSleep.fbx", "Sleep");

    SetInitGridPos({ 0,0 });
    FaceDirection({ 0, 1 });
    m_PreviousGridPos = m_GridPos;
    m_Position = Vector3(m_GridPos.x * TILE_DISTANCE, TILE_DISTANCE, m_GridPos.y * TILE_DISTANCE);
    m_AnimNow = m_AnimNext = "Idle";
    PlayAnimation("Idle", 1.0f);

    m_Scale = { 0.5f, 0.5f, 0.5f };

    InitToonShader();

    UnitManager::Instance()->SetPlayer(this);

    // ステータス初期化
    m_HP = m_MaxHP;
	m_ATK = m_MaxATK;
    m_DEF = m_MaxDEF;
    m_Name = u8"プレイヤー";
}

void Player::Update() {
    if (!m_AnimationModel) return;
    UpdateAnimation();

    // ループエフェクトを追従
    if (m_LoopEffect) {
        m_LoopEffect->SetPosition(m_Position + Vector3(0, 3.0f, 0));
    }

    // 行動演出の終了待ち
    if (m_IsActingAnimation) {
        bool itemFlying = false;
        auto items = Manager::GetScene()->GetGameObjects<Item>();
        for (auto* it : items) if (it->GetIsFlying()) { itemFlying = true; break; }

        auto flyObjects = Manager::GetScene()->GetGameObjects<FlyingObject>();
        bool isFlying = std::any_of(flyObjects.begin(), flyObjects.end(), [](auto* f) { return f->GetIsActive(); });

        if ((m_IsAnimLooping || IsAnimationFinished()) && !itemFlying && !isFlying) {
            m_IsActingAnimation = false;
        }
    }
    if (m_MoveState != MoveState::Idle || m_IsActingAnimation) return;

    // 階段確認
    MapManager* mm = Manager::GetScene()->GetGameObject<MapManager>();
    MapData* map = mm ? mm->GetCurrentMap() : nullptr;
    if (map && map->GetTile(m_GridPos.x, m_GridPos.y) == TileType::Stair) {
        ConfirmWindow* con = Manager::GetScene()->GetGameObject<ConfirmWindow>();
    
    // 初回確認時はウィンドウを開き、前回の「進まない」選択後は再表示しない。
    if (!m_StairConfirmed) {
        con->SetEnable(true);
    }

    // プレイヤーが「進む」か「進まない」を選ぶまで待つ。
    if (!con->GetDecided()) return;

    // 選択後はウィンドウを閉じる。
    con->SetEnable(false);

    if (con->GetConfirm()) {
        // 「進む」を選択したので次のフロアへ進む。
        m_StairConfirmed = true; // 再確認を防ぐためのフラグ
        con->SetDecided(false);
        mm->RequestNextFloor();
    } else {
        // 「進まない」を選択した。
        m_StairConfirmed = true; // 同じ階段上で確認を繰り返さないためのフラグ
        con->SetDecided(false);    }
    }
    else {
        m_StairConfirmed = false;
    }
}

void Player::UpdateUnit() {
    if (auto* miniMap = Manager::GetScene()->GetGameObject<MiniMapRenderer>())
    {
        if (miniMap->IsLookMode()) return;
    }

    if (m_HasActed) return;

    // プレイヤーターンの処理順
    // 1. 行動不能/確認UI/メニューを先に処理
    // 2. 移動アニメーション中なら完了待ち
    // 3. 状態異常入力
    // 4. 通常入力(移動・攻撃・足踏み)
    // 状態異常・メニューによる行動不可チェック
    if (m_Status == Status::Sleep || m_Status == Status::Paralysis) { EndTurn(); return; }
    if (IsStairConfirming()) return;

    auto* ui = Manager::GetScene()->GetGameObject<PlayerInventoryUI>();
    auto* shopUi = Manager::GetScene()->GetGameObject<ShopUI>();
    if ((ui && ui->IsAnyMenuOpen()) || (shopUi && shopUi->IsAnyMenuOpen())) {
        if (Input::GetKeyTrigger(VK_SPACE)) SortItems();
        return;
    }

    if (Input::GetKeyTrigger('X')) { if (ui) ui->OpenItemMenu(); return; }

    // 移動中なら補間を進める。
    if (m_MoveState != MoveState::Idle) {
        UpdateLerpMove();
        if (m_MoveState == MoveState::Idle) EndTurn();
        return;
    }

    if (Input::GetKeyTrigger(VK_CONTROL))
    {
        RefreshEquipIndices();
        if (m_EquippedArrowIndex >= 0 && m_EquippedArrowIndex < (int)m_Items.size())
        {
            ShootArrow(m_EquippedArrowIndex);
        }
        return;
    }

    if (m_Status == Status::Confusion) {
        ExecuteConfusionAction();
        return;
 
    }

    // 通常入力
    MoveInput();

    if (Input::GetKeyTrigger('Z')) Attack();
    if (Input::GetKeyPress('C')) EndTurn();
}

void Player::MoveInput() {

    // 方向キーは「向き変更」「通常移動」「ダッシュ」の共通入力として扱う。
    // Space 押下中は移動せず向きだけ変更し、Shift 押下中は ExecuteInstantDash に渡す。
    Vector2Int inputDir(0, 0);
    if (Input::GetKeyPress(VK_UP))    inputDir.y += 1;
    if (Input::GetKeyPress(VK_DOWN))  inputDir.y -= 1;
    if (Input::GetKeyPress(VK_LEFT))  inputDir.x -= 1;
    if (Input::GetKeyPress(VK_RIGHT)) inputDir.x += 1;

    if (Input::GetKeyPress(VK_SPACE)) {
        bool inputAdded = (inputDir.x != 0 && m_LastInputDir.x == 0) || (inputDir.y != 0 && m_LastInputDir.y == 0);
        if (inputAdded) {
            FaceDirection(inputDir);
        }
        m_LastInputDir = inputDir;
        return;
    }

    if (inputDir.x == 0 && inputDir.y == 0)
    {
        m_DashWaitShiftRelease = false;
        return;
    }
    m_IsDash = Input::GetKeyPress(VK_SHIFT);
    if (m_IsDash && m_DashWaitShiftRelease) return;

    if (inputDir.x != 0 && inputDir.y != 0) {
        MapData* map = MapManager::Instance()->GetCurrentMap();
        if (map && IsDiagonalMoveBlocked(m_GridPos, inputDir, map)) return;
    }

    if (m_IsDash && m_Status != Status::Confusion) {
        ExecuteInstantDash(inputDir);
        return;
    }

    m_HasPendingEntranceDash = false;
    Move(inputDir);
}

void Player::ExecuteConfusionAction() {
    Vector2Int inputDir(0, 0);
    if (Input::GetKeyPress(VK_UP))    inputDir.y += 1;
    if (Input::GetKeyPress(VK_DOWN))  inputDir.y -= 1;
    if (Input::GetKeyPress(VK_LEFT))  inputDir.x -= 1;
    if (Input::GetKeyPress(VK_RIGHT)) inputDir.x += 1;

    if (Input::GetKeyPress(VK_SPACE)) {
        if (inputDir.x != 0 || inputDir.y != 0) {
            FaceDirection(inputDir);
        }
        return;
    }

    if (inputDir.x != 0 || inputDir.y != 0)
    {
        MapData* map = MapManager::Instance()->GetCurrentMap();
        Vector2Int next = m_GridPos + inputDir;
        Unit* targetUnit = UnitManager::Instance()->GetUnitAt(next);
        if (map && map->IsInside(next) && map->IsWalkable(next.x, next.y) &&
            !IsDiagonalMoveBlocked(m_GridPos, inputDir, map) &&
            dynamic_cast<Ally*>(targetUnit)) {
            Move(inputDir);
            return;
        }

        Vector2Int dir = RandomConfusionDir();
        if (!TryConfusionMove(dir)) {
            FaceDirection(dir);
        }
        return;
    }

    if (Input::GetKeyTrigger('Z'))
    {
        Vector2Int dir = RandomConfusionDir();
        FaceDirection(dir);
        Attack();
        return;
    }

    if (Input::GetKeyPress('C')) EndTurn();

}

bool Player::TryConfusionMove(const Vector2Int& dir) {
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return false;

    Vector2Int next = m_GridPos + dir;
    if (!map->IsInside(next) || !map->IsWalkable(next.x, next.y)) return false;
    if (IsDiagonalMoveBlocked(m_GridPos, dir, map)) return false;
    if (UnitManager::Instance()->GetUnitAt(next)) return false;

    Move(dir);
    return true;
}

bool Player::CanConfusionAttack(const Vector2Int& dir) {
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (map && IsDiagonalMoveBlocked(m_GridPos, dir, map)) return false;

    Vector2Int targetPos = m_GridPos + dir;
    return UnitManager::Instance()->GetUnitAt(targetPos) != nullptr;
}

void Player::FaceDirection(const Vector2Int& dir) {
    if (dir.x == 0 && dir.y == 0) return;

    LookAt(dir);
}

void Player::ExecuteInstantDash(const Vector2Int& dir) {
    if (dir.x == 0 && dir.y == 0) return;

    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return;

    FaceDirection(dir);

    bool stopAfterOneStep = HasNewDashAdjacentEnemy();
    int maxSteps = map->GetWidth() * map->GetHeight();
    for (int i = 0; i < maxSteps; ++i) {
        if (!CanInstantDashStep(dir)) break;

        Vector2Int next = m_GridPos + dir;
        if (IsVisibleTrapAt(next)) break;

        bool nextIsEntrance = map->IsEntranceTile(next.x, next.y);
        bool canEnterPendingEntrance = m_HasPendingEntranceDash && m_PendingEntranceDashPos == next;
        if (!stopAfterOneStep && nextIsEntrance && !canEnterPendingEntrance) {
            m_HasPendingEntranceDash = true;
            m_PendingEntranceDashPos = next;
            break;
        }

        if (canEnterPendingEntrance) {
            m_HasPendingEntranceDash = false;
            m_PendingEntranceDashPos = { -1, -1 };
        }

        const bool nextHasItem = IsItemAt(next);
        InstantMoveTo(next, nextHasItem);
        EndTurn();
        TurnManager::Instance()->ResolveAfterPlayerInstantMove();

        if (IsDead()) break;
        if (IsStairConfirming()) break;
        if (m_Status == Status::Sleep || m_Status == Status::Paralysis) break;
        if (stopAfterOneStep) {
            UpdateKnownDashAdjacentEnemies();
            break;
        }
        if (IsInstantDashStopTile(m_GridPos)) break;
        if (nextHasItem || IsItemAdjacent(m_GridPos)) break;
        if (ShouldStopDashForRoomEnemyAdjacent()) break;
    }

    m_DashWaitShiftRelease = true;
}

bool Player::CanInstantDashStep(const Vector2Int& dir) {
    if (dir.x == 0 && dir.y == 0) return false;

    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return false;
    if (dir.x != 0 && dir.y != 0 && IsDiagonalMoveBlocked(m_GridPos, dir, map)) return false;

    Vector2Int next = m_GridPos + dir;
    if (!map->IsInside(next) || !map->IsWalkable(next.x, next.y)) return false;
    if (UnitManager::Instance()->GetUnitAt(next)) return false;

    return true;
}

bool Player::IsInstantDashStopTile(const Vector2Int& gridPos) {
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return false;
    return map->IsEntranceTile(gridPos.x, gridPos.y) || IsCorridorIntersectionTile(gridPos) || IsVisibleTrapAt(gridPos);
}


bool Player::IsCorridorIntersectionTile(const Vector2Int& gridPos) {
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return false;
    if (map->GetRoomAt(gridPos)) return false;
    if (map->GetTile(gridPos.x, gridPos.y) != TileType::Corridor) return false;

    static const Vector2Int dirs[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    int walkableCount = 0;
    for (const auto& d : dirs) {
        if (map->IsWalkable(gridPos + d)) ++walkableCount;
    }

    return walkableCount >= 3;
}

bool Player::IsVisibleTrapAt(const Vector2Int& gridPos) {
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return false;

    Trap* trap = dynamic_cast<Trap*>(map->GetObjectAt(gridPos));
    return trap && trap->IsVisible();
}

bool Player::IsItemAt(const Vector2Int& gridPos) {
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map || !map->IsInside(gridPos)) return false;

    return dynamic_cast<Item*>(map->GetObjectAt(gridPos)) != nullptr;
}

bool Player::IsItemAdjacent(const Vector2Int& gridPos) {
    // Shiftダッシュ中は、周囲8マスにアイテムがあれば停止する。
    static const Vector2Int dirs[8] = {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
        { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
    };

    for (const Vector2Int& dir : dirs) {
        if (IsItemAt(gridPos + dir)) return true;
    }
    return false;
}

bool Player::HasNewDashAdjacentEnemy() {
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return false;

    auto enemies = UnitManager::Instance()->GetAdjacentEnemies(*this);
    if (enemies.empty()) {
        m_KnownDashAdjacentEnemies.clear();
        return false;
    }

    if (!map->GetRoomAt(m_GridPos)) {
        m_KnownDashAdjacentEnemies = enemies;
        return false;
    }

    for (Enemy* enemy : enemies) {
        if (std::find(m_KnownDashAdjacentEnemies.begin(), m_KnownDashAdjacentEnemies.end(), enemy) == m_KnownDashAdjacentEnemies.end()) {
            return true;
        }
    }
    return false;
}

void Player::UpdateKnownDashAdjacentEnemies() {
    m_KnownDashAdjacentEnemies = UnitManager::Instance()->GetAdjacentEnemies(*this);
}

bool Player::ShouldStopDashForEnemyAdjacent() {
    return HasNewDashAdjacentEnemy();
}


bool Player::ShouldStopDashForRoomEnemyAdjacent() {
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map || !map->GetRoomAt(m_GridPos)) return false;
    return IsEnemyAdjacent();
}

bool Player::IsEnemyAdjacent() {
    return !UnitManager::Instance()->GetAdjacentEnemies(*this).empty();
}

void Player::InstantMoveTo(const Vector2Int& gridPos, bool suppressObjectStep) {
    m_PreviousGridPos = m_GridPos;
    m_MoveState = MoveState::Idle;
    m_IsAnimatingMove = false;
    m_MoveTarget = gridPos;
    m_GridPos = gridPos;
    m_Position = GridToWorld(gridPos);
    if (auto* scene = Manager::GetScene())
    {
        if (auto* miniMap = scene->GetGameObject<MiniMapRenderer>())
        {
            miniMap->RevealFromPosition(m_GridPos, GetViewDistance());
        }
    }
    MapManager::Instance()->AfterUnitMoved(this, suppressObjectStep);
    PlayAnimation("Idle", 1.0f);
}

void Player::Move(const Vector2Int& dir) {
    // 1マス移動の本体。
    // 仲間が移動先にいる場合は位置入れ替え、敵がいる場合は移動せず、空きマスなら StartMove で補間移動する。
    if (dir.x != 0 || dir.y != 0) {
        FaceDirection(dir);
    }
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return;

    Vector2Int next = m_GridPos + dir;
    if (!map->IsInside(next) || !map->IsWalkable(next.x, next.y)) return;

    Unit* targetUnit = UnitManager::Instance()->GetUnitAt(next);
    if (Ally* ally = dynamic_cast<Ally*>(targetUnit)) {
        Vector2Int oldPos = m_GridPos;
        m_PreviousGridPos = oldPos;
        StartMove(next, MOVE_TIME_DASH);
        ally->RequestMove(oldPos);
        ally->ReserveMoveConsumedOnNextTurn();
        ally->LookAt(-dir);
        // 入れ替わりで押し出された等速以下の仲間は、敵が隣接していても同じ敵ターン内に即攻撃させない。
        if (IsNormalOrSlowerAction(ally) || UnitManager::Instance()->GetAdjacentEnemies(*ally).empty()) {
            ally->ReserveActionConsumedOnNextTurn();
        }
        EndTurn();
        return;
    }

    if (UnitManager::Instance()->HasEnemy(next)) return;

    m_PreviousGridPos = m_GridPos;
    StartMove(next, MOVE_TIME_DASH);
    EndTurn();
}

void Player::Attack() {
    // 攻撃ごとに表示中ログをリセットする。履歴は MessageLog 側に残す。
    MessageLog::Instance().Clear();
    // 正面1マスへの通常攻撃。
    // Unit がいない場合でも、罠の発見や足元オブジェクトへの攻撃があるため MapObject も確認する。
    m_IsActingAnimation = true;
    SetTriggerAnimation("Attack", 1.0f);

    Vector2Int targetPos = m_GridPos + m_FacingDir;
    MapData* map = MapManager::Instance()->GetCurrentMap();

    if (map && IsDiagonalMoveBlocked(m_GridPos, m_FacingDir, map))
    {
        EndTurn();
        return;
    }

    Unit* target = UnitManager::Instance()->GetUnitAt(targetPos);
    if (!target) {
        if (map) {
            MapObject* obj = map->GetObjectAt(targetPos);

            if (Trap* trap = dynamic_cast<Trap*>(obj)) {
                trap->SetVisible(true);
            }

            if (obj) {
                auto* ui = Manager::GetScene()->GetGameObject<PlayerInventoryUI>();
                const bool wasMenuOpen = ui && ui->IsAnyMenuOpen();
                obj->OnAttacked();
                const bool openedMenu = ui && ui->IsAnyMenuOpen() && !wasMenuOpen;
                if (!openedMenu) EndTurn();
                return;
            }
        }

        EndTurn();
        return;
    }

    if (Enemy* shopkeeper = dynamic_cast<Enemy*>(target)) {
        if (shopkeeper->IsShopKeeper() && !shopkeeper->IsShopHostile()) {
            if (auto* shopUi = Manager::GetScene()->GetGameObject<ShopUI>()) shopUi->OpenShopTradeMenu();
            EndTurn();
            return;
        }
    }

    if (Ally* ally = dynamic_cast<Ally*>(target)) {
        ally->Talk();
    }
    else if (CheckHit(GetACC(), target->GetEVD())) {
        int dmg = CalcDamage(GetATK(), target->GetDEF());
        Enemy* enemyTarget = dynamic_cast<Enemy*>(target);
        if (enemyTarget) enemyTarget->MarkPlayerNormalAttackDamage();
        target->TakeDamage(dmg, this);
        if (enemyTarget && !target->IsDead()) enemyTarget->ClearPlayerNormalAttackDamage();
    }
    else {
        MessageLog::Instance().AddMessage(m_Name + u8"の攻撃は外れた。");
    }
    EndTurn();
}

void Player::EndTurn() {
    m_HasActed = true;
    ConsumeFullness();
    UpdateStatusCount();
    if (m_Fullness == 0)
    {
        m_HP--;
        if (m_HP <= 0)
        {
            m_HP = 0;
            m_PendingDeathCause = u8"満腹度";
            OnDeath(nullptr);
        }
    }
    else NaturalRecovery();
}

int Player::GetATK() const {
    int total = m_ATK + (m_Strength - m_MaxStrength);
    if (total < 1) total = 1;
    if (m_EquippedWeaponIndex != -1) total += m_Items[m_EquippedWeaponIndex].instance.GetData()->baseValue;
    return total;
}

int Player::GetDEF() const {
    int total = m_DEF;
    if (m_EquippedShieldIndex != -1) total += m_Items[m_EquippedShieldIndex].instance.GetData()->baseValue;
    return total;
}

void Player::SortItems() {
    std::sort(m_Items.begin(), m_Items.end(), [](const InventoryItem& a, const InventoryItem& b) {
        if (a.instance.GetData()->type != b.instance.GetData()->type)
            return (int)a.instance.GetData()->type < (int)b.instance.GetData()->type;
        return a.instance.GetTrueName() < b.instance.GetTrueName();
        });
    RefreshEquipIndices();
}

void Player::Uninit() {
    if (UnitManager::Instance()->GetPlayer() == this) {
        UnitManager::Instance()->SetPlayer(nullptr);
    }
    StopLoopEffect();

    if (m_AnimationModel) { m_AnimationModel->Uninit(); delete m_AnimationModel; }
    ReleaseToonShader();
}

void Player::OnDeath(Unit* attacker) {
    std::string cause = attacker ? (attacker->GetName() + u8"の攻撃") : m_PendingDeathCause;
    if (cause.empty()) cause = u8"原因不明";

    std::string dungeonName = u8"ダンジョン";
    if (MapManager::Instance())
    {
        dungeonName = MapManager::Instance()->GetDungeonData().GetDungeonId();
    }

    if (auto* ending = Manager::GetScene()->GetGameObject<DungeonEndingUI>())
    {
        SetVisible(false);
        ending->StartDeath(m_Name, dungeonName, cause);
    }
}

void Player::ConsumeFullness() 
{ 
    if (++m_FullnessCounter >= m_FullnessInterval) 
    { 
        m_FullnessCounter = 0; 
        if (m_Fullness > 0) 
            m_Fullness--;
    } }
void Player::AddFullness(int v) 
{
    m_Fullness = (std::min)(m_Fullness + v, m_MaxFullness); 
}
void Player::AddGold(int value)
{
    m_Gold = (std::max)(0, m_Gold + value);
}
bool Player::SpendGold(int value)
{
    if (value < 0) return false;
    if (m_Gold < value) return false;
    m_Gold -= value;
    return true;
}
void Player::SetMaxStrength(int value)
{
    const bool wasFull = (m_Strength >= m_MaxStrength);
    m_MaxStrength = (std::max)(1, value);
    if (wasFull) m_Strength = m_MaxStrength;
    else if (m_Strength > m_MaxStrength) m_Strength = m_MaxStrength;
}
void Player::SetStrength(int value)
{
    m_Strength = std::clamp(value, 0, m_MaxStrength);
}
bool Player::ReduceStrength(int value)
{
    if (value <= 0) return false;
    const int before = m_Strength;
    SetStrength(m_Strength - value);
    if (m_Strength < before)
    {
        MessageLog::Instance().AddMessage(m_Name + u8"は毒を受け、ちからが下がった。");
        return true;
    }
    MessageLog::Instance().AddMessage(m_Name + u8"のちからはもう下がらない。");
    return false;
}
bool Player::RestoreStrengthToMax()
{
    if (m_Strength >= m_MaxStrength) return false;
    m_Strength = m_MaxStrength;
    MessageLog::Instance().AddMessage(m_Name + u8"のちからが元に戻った。");
    return true;
}
void Player::ResetPosition(Vector2Int gp) 
{
    m_PreviousGridPos = gp;
    m_GridPos = m_MoveTarget = gp; m_Position = Vector3(gp.x * TILE_DISTANCE, 0, gp.y * TILE_DISTANCE); 
    m_StairConfirmed = false;
}
bool Player::IsInView(const Vector2Int& tp) const 
{
    const int dx = std::abs(m_GridPos.x - tp.x);
    const int dy = std::abs(m_GridPos.y - tp.y);

    MapManager* mapManager = MapManager::Instance();
    if (mapManager && mapManager->GetCurrentFloorData().playerVisionClear) {
        return dx <= CLEAR_VISION_LOD_DISTANCE && dy <= CLEAR_VISION_LOD_DISTANCE;
    }

    MapData* map = mapManager ? mapManager->GetCurrentMap() : nullptr;
    if (map) {
        const Room* playerRoom = map->GetRoomAt(m_GridPos);
        if (playerRoom && playerRoom == map->GetRoomAt(tp)) {
            return true;
        }
    }

    const int viewDistance = GetViewDistance();
    return dx <= viewDistance && dy <= viewDistance;
}
int Player::GetViewDistance() const
{
    MapManager* mapManager = MapManager::Instance();
    if (!mapManager) return 2;

    return (std::max)(0, mapManager->GetCurrentFloorData().viewDistance);
}
bool Player::IsStairConfirming() const
{ 
    MapData* map = MapManager::Instance()->GetCurrentMap();
    return map && map->GetTile(m_GridPos.x, m_GridPos.y) == TileType::Stair && !m_StairConfirmed;
}

void Player::RefreshEquipIndices() {
    m_EquippedWeaponIndex = m_EquippedShieldIndex = m_EquippedArrowIndex = -1;
    for (int i = 0; i < (int)m_Items.size(); ++i) {
        if (!m_Items[i].isEquipped) continue;
        const ItemData* data = m_Items[i].instance.GetData();
        if (!data) { m_Items[i].isEquipped = false; continue; }
        if (data->stackable && m_Items[i].count <= 0) { m_Items[i].isEquipped = false; continue; }
        ItemType t = data->type;
        if (t == ItemType::Weapon) m_EquippedWeaponIndex = i;
        else if (t == ItemType::Shield) m_EquippedShieldIndex = i;
        else if (t == ItemType::Arrow) m_EquippedArrowIndex = i;
    }
}

void Player::EquipItem(int index) {
    if (index < 0 || index >= (int)m_Items.size()) return;
    InventoryItem& target = m_Items[index];
    if (target.isEquipped) {
        if (target.instance.IsCursed()) { MessageLog::Instance().AddMessage(u8"呪われていて外せない！"); return; }
        target.isEquipped = false;
    }
    else {
        for (auto& item : m_Items) {
            if (item.isEquipped && item.instance.GetData()->type == target.instance.GetData()->type) {
                if (item.instance.IsCursed()) { MessageLog::Instance().AddMessage(u8"呪われた装備を外せない！"); return; }
                item.isEquipped = false;
            }
        }
        target.isEquipped = true;
    }
    RefreshEquipIndices();
}

void Player::UnequipWeapon() { 
    if (m_EquippedWeaponIndex != -1)
        EquipItem(m_EquippedWeaponIndex); 
}

void Player::AddItem(ItemInstance&& inst) {
    int addCount = inst.GetStackCount();
    if (inst.GetData()->stackable) {
        for (auto& item : m_Items) {
            if (item.instance.GetData() == inst.GetData()) {
                item.count += addCount;
                item.instance.SetStackCount(item.count);
                return;
            }
        }
    }
    inst.SetStackCount(addCount);
    m_Items.emplace_back(std::move(inst));
}
void Player::RemoveItemAt(int index)
{
    if (index < 0 || index >= (int)m_Items.size()) return;
    m_Items.erase(m_Items.begin() + index);
    RefreshEquipIndices();
}

void Player::Draw() {
    if (!m_IsVisible) return;
    Vector3 visualPos = GetVisualPosition();
    Vector3 visualRot = GetVisualRotation();
    XMMATRIX world = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
        XMMatrixRotationRollPitchYaw(visualRot.x, visualRot.y, visualRot.z) *
        XMMatrixTranslation(visualPos.x, visualPos.y + 1.3f, visualPos.z);
    DrawToonModel(world);
}
void Player::UseItem(int index) {
    if (index < 0 || index >= (int)m_Items.size()) return;
    m_IsActingAnimation = true;
    ItemInstance& inst = m_Items[index].instance;
    inst.Use(this, index);
    if (!inst.IsPot() && inst.GetData()->type != ItemType::Staff) m_Items.erase(m_Items.begin() + index);
    RefreshEquipIndices();
    EndTurn();
}

void Player::ShootArrow(int index) {
    if (index < 0 || index >= (int)m_Items.size()) { RefreshEquipIndices(); return; }
    InventoryItem& slot = m_Items[index];
    const ItemData* data = slot.instance.GetData();
    if (!data || data->type != ItemType::Arrow || slot.count <= 0) {
        if (!data || data->type == ItemType::Arrow) slot.isEquipped = false;
        RefreshEquipIndices();
        return;
    }

    m_IsActingAnimation = true;
    SetTriggerAnimation("Attack", 1.0f);
    MessageLog::Instance().AddMessage(m_Name + u8"は" + slot.instance.GetDisplayName() + u8"を撃った。");

    FlyingObject* arrow = Manager::GetScene()->AddGameObject<FlyingObject>(1);
	arrow->SetRotation(Vector3(0, m_Rotation.y-90.0f, 0));
    arrow->FireEffect(
        "Asset\\Model\\Items\\Arrow.obj",
        data->effect,
        this,
        EffectSourceType::Item,
        m_Position + Vector3(m_FacingDir.x * 0.8f, 0.5f, m_FacingDir.y * 0.8f),
        GetGridPos(),
        m_FacingDir,
        0.04f,
        10,
        data);

    if (slot.count > 1) {
        --slot.count;
        slot.instance.SetStackCount(slot.count);
    }
    else m_Items.erase(m_Items.begin() + index);
    RefreshEquipIndices();
    EndTurn();
}

void Player::ThrowItem(int index) {
    if (index < 0 || index >= (int)m_Items.size()) return;
    InventoryItem& slot = m_Items[index];
    const ItemData* data = slot.instance.GetData();
    const bool splitStack = data && data->stackable && slot.count > 1;
    ItemInstance thrown = splitStack
        ? ItemInstance(data)
        : std::move(slot.instance);
    thrown.SetStackCount(1);
    if (splitStack) {
        --slot.count;
        slot.instance.SetStackCount(slot.count);
    }
    else m_Items.erase(m_Items.begin() + index);
    RefreshEquipIndices();
    MessageLog::Instance().AddMessage(m_Name + u8"は" + thrown.GetDisplayName() + u8"を投げた。");

    Item* pItem = Manager::GetScene()->AddGameObject<Item>(1);
    pItem->SetInstance(std::move(thrown));
    pItem->SetupFromInstance();
    pItem->StartThrow(this, m_Position + Vector3(0, 0.5f, 0), GetGridPos(), m_FacingDir);
    EndTurn();
}
