#include "GameRandom.h"
#include "Unit.h"
#include <string>
#include "animationModel.h"
#include "renderer.h"
#include "MapManager.h"
#include "Time.h"
#include "UnitManager.h"
#include "Player.h"
#include "EffectBillboard.h"
#include "EffectManager.h"
#include <algorithm>
#include <cmath>
bool Unit::s_SkipMoveAnimation = false;

namespace
{
    size_t StatModifierIndex(StatModifierType type)
    {
        return static_cast<size_t>(type);
    }

    const char* GetStatModifierName(StatModifierType type)
    {
        switch (type)
        {
        case StatModifierType::Attack: return u8"攻撃力";
        case StatModifierType::Defense: return u8"守備力";
        default: return u8"能力";
        }
    }

    Vector2Int NormalizeFacingDir(const Vector2Int& dir)
    {
        return Vector2Int(
            (dir.x > 0) ? 1 : (dir.x < 0 ? -1 : 0),
            (dir.y > 0) ? 1 : (dir.y < 0 ? -1 : 0));
    }
}

Unit::Unit()
{
    // 全ユニット共通の段階制補正を初期化する。個別調整したい場合は派生クラスの Init で呼び直す。
    InitStatModifier(StatModifierType::Attack, 4, 0.25f);
    InitStatModifier(StatModifierType::Defense, 4, 0.25f);
}

void Unit::InitStatModifier(StatModifierType type, int maxStage, float ratePerStage)
{
    const size_t index = StatModifierIndex(type);
    if (index >= m_StatModifiers.size()) return;

    StatModifierState& state = m_StatModifiers[index];
    state.maxStage = (std::max)(0, maxStage);
    state.ratePerStage = (std::max)(0.0f, ratePerStage);
    state.stage = std::clamp(state.stage, -state.maxStage, state.maxStage);
}

int Unit::AddStatModifierStage(StatModifierType type, int stageDelta)
{
    const size_t index = StatModifierIndex(type);
    if (index >= m_StatModifiers.size() || stageDelta == 0) return 0;

    StatModifierState& state = m_StatModifiers[index];
    const int before = state.stage;
    state.stage = std::clamp(state.stage + stageDelta, -state.maxStage, state.maxStage);

    if (state.stage == before)
    {
        MessageLog::Instance().AddMessage(m_Name + u8"の" + GetStatModifierName(type) + (stageDelta < 0 ? u8"はもう下がらない。" : u8"はもう上がらない。"));
        return state.stage;
    }

    MessageLog::Instance().AddMessage(m_Name + u8"の" + GetStatModifierName(type) + (state.stage < before ? u8"が下がった。" : u8"が上がった。"));
    return state.stage;
}

int Unit::GetStatModifierStage(StatModifierType type) const
{
    const size_t index = StatModifierIndex(type);
    if (index >= m_StatModifiers.size()) return 0;
    return m_StatModifiers[index].stage;
}

void Unit::ClearStatModifierStage(StatModifierType type)
{
    const size_t index = StatModifierIndex(type);
    if (index >= m_StatModifiers.size()) return;

    StatModifierState& state = m_StatModifiers[index];
    if (state.stage == 0) return;

    const int before = state.stage;
    state.stage = 0;
    MessageLog::Instance().AddMessage(m_Name + u8"の" + GetStatModifierName(type) + (before < 0 ? u8"が元に戻った。" : u8"の強化が消えた。"));
}

int Unit::ApplyStatModifierToValue(StatModifierType type, int value) const
{
    const size_t index = StatModifierIndex(type);
    if (index >= m_StatModifiers.size()) return (std::max)(1, value);

    const StatModifierState& state = m_StatModifiers[index];
    if (state.stage == 0) return (std::max)(1, value);

    // 段階数から倍率を作る。下げすぎても0倍にはせず、最低1ダメージ計算が壊れないようにする。
    float rate = 1.0f + state.ratePerStage * static_cast<float>(state.stage);
    if (rate < 0.1f) rate = 0.1f;
    return (std::max)(1, static_cast<int>(std::round(value * rate)));
}


void Unit::InitToonShader()
{
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\ToonVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "shader\\ToonPS.cso");
    Renderer::CreateVertexShader(&m_OutlineVertexShader, &m_OutlineVertexLayout, "shader\\ToonOutlineVS.cso");
    Renderer::CreatePixelShader(&m_OutlinePixelShader, "shader\\ToonOutlinePS.cso");
}

void Unit::ReleaseToonShader()
{
    if (m_OutlinePixelShader) { m_OutlinePixelShader->Release(); m_OutlinePixelShader = nullptr; }
    if (m_OutlineVertexShader) { m_OutlineVertexShader->Release(); m_OutlineVertexShader = nullptr; }
    if (m_OutlineVertexLayout) { m_OutlineVertexLayout->Release(); m_OutlineVertexLayout = nullptr; }
    if (m_PixelShader) { m_PixelShader->Release(); m_PixelShader = nullptr; }
    if (m_VertexShader) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_VertexLayout) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
}

void Unit::DrawToonModel(XMMATRIX world)
{
    if (!m_AnimationModel) return;

    Renderer::SetWorldMatrix(world);
    ID3D11DeviceContext* context = Renderer::GetDeviceContext();

    if (m_OutlineVertexShader && m_OutlinePixelShader && m_OutlineVertexLayout)
    {
        const XMFLOAT4 outlineOffsets[] =
        {
            XMFLOAT4(-2.0f,  0.0f, 0.0f, 0.0f),
            XMFLOAT4( 2.0f,  0.0f, 0.0f, 0.0f),
            XMFLOAT4( 0.0f, -2.0f, 0.0f, 0.0f),
            XMFLOAT4( 0.0f,  2.0f, 0.0f, 0.0f),
            XMFLOAT4(-1.4f, -1.4f, 0.0f, 0.0f),
            XMFLOAT4( 1.4f, -1.4f, 0.0f, 0.0f),
            XMFLOAT4(-1.4f,  1.4f, 0.0f, 0.0f),
            XMFLOAT4( 1.4f,  1.4f, 0.0f, 0.0f),
        };

        Renderer::SetDepthEnable(false);
        Renderer::SetCullMode(D3D11_CULL_BACK);
        context->IASetInputLayout(m_OutlineVertexLayout);
        context->VSSetShader(m_OutlineVertexShader, NULL, 0);
        context->PSSetShader(m_OutlinePixelShader, NULL, 0);

        for (const XMFLOAT4& offset : outlineOffsets)
        {
            Renderer::SetParameter(offset);
            m_AnimationModel->Draw();
        }
    }

    Renderer::SetParameter(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
    Renderer::SetDepthEnable(true);
    Renderer::SetCullMode(D3D11_CULL_BACK);
    context->IASetInputLayout(m_VertexLayout);
    context->VSSetShader(m_VertexShader, NULL, 0);
    context->PSSetShader(m_PixelShader, NULL, 0);
    m_AnimationModel->Draw();
}
void Unit::BeginTurnActions()
{
    m_ActionPhaseChecked = false;
    m_MovePhaseChecked = false;
    m_TurnConsumeType = TurnConsumeType::Action;
    m_ActionConsumedByMoveThisTurn = false;

    m_MovedThisTurn = false;

    m_ActionEnergy += GetTurnSpeedEnergy(m_ActionSpeed);
    m_MoveEnergy += GetTurnSpeedEnergy(m_MoveSpeed);
    m_ActionBudget = m_ActionEnergy / 2;
    m_MoveBudget = m_MoveEnergy / 2;
    m_ActionEnergy %= 2;
    m_MoveEnergy %= 2;
    m_HasActed = (m_ActionBudget <= 0);
    if (m_ConsumeActionOnNextTurn) {
        ConsumeAllActions();
        m_ConsumeActionOnNextTurn = false;
    }
    if (m_ConsumeMoveOnNextTurn) {
        ConsumeMove();
        m_ConsumeMoveOnNextTurn = false;
    }
}
void Unit::PlayAnimation(const std::string& animName, float Speed)
{
    if (this == nullptr) return;
    m_AnimSpeed = Speed;

    // 現在再生中、または次に再生予定のものが同じならリセットしない
    if (m_AnimNext == animName || (m_AnimNext.empty() && m_AnimNow == animName))
        return;

    m_AnimNext = animName;
    m_Frame = 0; // ここでリセットされるのを防ぐ
    m_AnimationBlend = 0.0f;
}
void Unit::SetTriggerAnimation(const std::string& animName, float speed, bool waitForAnimation)
{
    if (s_SkipMoveAnimation) {
        m_IsActingAnimation = false;
        m_IsAnimLooping = true;
        PlayAnimation(m_DefaultAnim, 1.0f);
        return;
    }

    // 攻撃などはターン進行を待たせるが、鉄球の衝突ダメージなどは見た目だけ再生できるようにする。
    if (waitForAnimation) m_IsActingAnimation = true;
    m_IsAnimLooping = false; 
    PlayAnimation(animName, speed);
}



void Unit::SetInitGridPos(const Vector2Int& g)
{
    m_GridPos = g;
    m_LastValidGridPos = g;
    m_MoveStartGridPos = g;
    m_Position = GridToWorld(g);
}

void Unit::SetGridPos(const Vector2Int& g)
{
    MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
    if (map && (!map->IsInBounds(g) || !map->IsWalkable(g))) return;

    m_GridPos = g;
    m_LastValidGridPos = g;
    m_MoveStartGridPos = g;
    m_Position = GridToWorld(g);

}

bool Unit::RepairInvalidGridPos(const char* /*context*/)
{
    MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
    if (!map) return false;

    if (map->IsInBounds(m_GridPos) && map->IsWalkable(m_GridPos)) {
        m_LastValidGridPos = m_GridPos;
        return false;
    }
    if (!map->IsInBounds(m_LastValidGridPos) || !map->IsWalkable(m_LastValidGridPos)) return false;

    m_GridPos = m_LastValidGridPos;
    m_MoveStartGridPos = m_GridPos;
    m_Position = GridToWorld(m_GridPos);
    m_MoveTarget = m_GridPos;
    m_MoveStartPos = m_Position;
    m_MoveEndPos = m_Position;
    m_MoveTimer = 0.0f;
    m_MoveState = MoveState::Idle;
    m_IsAnimatingMove = false;
    m_VisualRotationOffset = Vector3(0.0f, 0.0f, 0.0f);

    return true;
}
void Unit::SetWorldPosition(const Vector3& w)
{
    m_Position = w;
}

void Unit::UpdateFacingRotation()
{
    if (m_FacingDir.x == 0 && m_FacingDir.y == 0)
        return;

    float rad = std::atan2((float)m_FacingDir.x, (float)m_FacingDir.y);
    m_Rotation.y = rad;
}       

void Unit::LookAt(const Vector2Int& targetGrid)
{
    Vector2Int dir = NormalizeFacingDir(targetGrid);
    if (dir.x == 0 && dir.y == 0)
        return;

    m_FacingDir = dir;
    UpdateFacingRotation();
}
bool Unit::ShouldShowCombatVisual(Unit* other) const
{
    if (s_SkipMoveAnimation) return false;

    Player* player = UnitManager::Instance() ? UnitManager::Instance()->GetPlayer() : nullptr;
    if (!player) return true;

    if (player->IsInView(GetGridPos())) return true;
    return other && player->IsInView(other->GetGridPos());
}
void Unit::TakeDamage(int amount, Unit* attacker)
{
    if (amount < 1) amount = 1;
    const bool showVisual = ShouldShowCombatVisual(attacker);
    if (showVisual) SetTriggerAnimation("Damaged", 1.0f);
    m_HP -= amount;

    if (m_Status == Status::Paralysis || m_Status == Status::Nap)  ClearStatus();

    if (showVisual) EffectManager::Play(GetPosition(), "Asset\\Texture\\Damage.png", "Asset\\Sound\\Hit-Punch08-3(High).wav");
    MessageLog::Instance().AddMessage(
        m_Name + u8"は" +
        std::to_string(amount) +
        u8"ダメージを受けた。"
    );

    if (m_HP <= 0)
    {
        m_HP = 0;

        OnDeath(attacker);

    }
}


void Unit::ConstantDamage(int value, Unit* attacker, bool waitForAnimation)
{
    // 鉄球の衝突など、移動演出内のダメージは見た目だけ出してターン進行の待ち対象から外せる。
    const bool showVisual = ShouldShowCombatVisual(attacker);
    if (showVisual) SetTriggerAnimation("Damaged", 1.0f, waitForAnimation);
    if (showVisual) EffectManager::Play(GetPosition(), "Asset\\Texture\\Damage.png", "Asset\\Sound\\Hit-Punch08-3(High).wav");
    else EffectManager::PlaySE("Asset\\Sound\\Hit-Punch08-3(High).wav");
    MessageLog::Instance().AddMessage(
        m_Name + u8"は" +
        std::to_string(value) +
        u8"ダメージを受けた。"
    );
    m_HP -= value;
    if (m_Status == Status::Paralysis || m_Status == Status::Nap)  ClearStatus();
    if (m_HP <= 0)
    {
        m_HP = 0;
        OnDeath(attacker);
    }
}
void Unit::NaturalRecovery()
{
    if (m_HP >= m_MaxHP) return;

    //  毎ターン、最大HPに応じた値をカウンターに蓄積する
    m_RegenCounter += m_MaxHP;

    //  蓄積値が閾値（m_RegenConst）を超えたら、その分だけHPを回復
    if (m_RegenCounter >= m_RegenConst)
    {
        int heal = m_RegenCounter / m_RegenConst; // 蓄積から回復量を計算
        m_RegenCounter %= m_RegenConst;           // 余りを次回に持ち越し

        m_HP += heal;
        if (m_HP > m_MaxHP)
            m_HP = m_MaxHP;
    }
}
void Unit::PlayLoopEffect(const char* fileName, int rows, int cols, float speed)
{
    // すでに別のエフェクトが出ていたら消す
    StopLoopEffect();

    // 自分の頭上あたりに生成
    Vector3 pos = m_Position;
    pos.y += 2.0f; // ユニットの高さに合わせて調整

    m_LoopEffect = EffectBillboard::Create(pos, fileName, rows, cols, speed);

    // エフェクト側に「ループフラグ」があるならセット
    m_LoopEffect->SetLoop(true);
}
void Unit::StopLoopEffect()
{
    if (m_LoopEffect)
    {
        m_LoopEffect->SetDestroy(); // 消去リクエスト
        m_LoopEffect = nullptr;
    }
}

bool Unit::UpdateStatusCount()
{
    if (m_Status == Status::None) return false;

    // 期間設定がある場合のみカウントダウン
    if (m_StatusDuration > 0) {
        m_StatusDuration--;
        if (m_StatusDuration <= 0) {
            ClearStatus();
            return true;
        }
    }
    return false;
}
bool Unit::ConsumeActionBlockAfterStatusClear()
{
    if (!m_BlockActionOnceAfterStatusClear) return false;
    m_BlockActionOnceAfterStatusClear = false;
    return true;
}
void Unit::SetStatus(Status effect, int duration)
{
    if (m_Status == effect) {
        switch (effect) {
        case Status::Confusion:
            MessageLog::Instance().AddMessage(m_Name + u8"はすでに混乱している！");
            break;
        case Status::Sleep:
            MessageLog::Instance().AddMessage(m_Name + u8"はすでに眠っている！");
            break;
        case Status::Nap:
            
        case Status::Paralysis:
            MessageLog::Instance().AddMessage(m_Name + u8"はすでに動けない！");
            break;
        case Status::Poison:
            // 毒の重ねがけは共通の攻撃低下段階として処理する。
            AddStatModifierStage(StatModifierType::Attack, -1);
            if (duration != 0) m_StatusDuration = duration;
            break;
        default:
            break;
        }
        return;
    }
   

    switch (effect) {
    case Status::Confusion:
        StopLoopEffect();
        EffectManager::PlaySE("Asset\\Sound\\Confuse.wav");
        MessageLog::Instance().AddMessage(m_Name + u8"は混乱した。");
        m_LoopEffect = EffectManager::CreateLoopEffect(m_Position, "Asset\\Texture\\Confuse.png");
        m_Status = effect;
        m_StatusDuration = duration;
        break;

       
    case Status::Sleep:
        StopLoopEffect();
        EffectManager::PlaySE("Asset\\Sound\\Sleep.wav");
        MessageLog::Instance().AddMessage(m_Name + u8"は眠ってしまった。");
        m_LoopEffect = EffectManager::CreateLoopEffect(m_Position, "Asset\\Texture\\Sleep.png");
        m_Status = effect;
        m_StatusDuration = duration;
        PlayAnimation("Sleep", 1.0f);
        m_DefaultAnim = "Sleep";
        break;
      
    case Status::Paralysis:
        StopLoopEffect();
        EffectManager::PlaySE("Asset\\Sound\\Paralysis.wav");
        MessageLog::Instance().AddMessage(m_Name + u8"は動けなくなった。");
        m_Status = effect;
        m_StatusDuration = duration;
        break;
    case Status::Nap:
        StopLoopEffect();
        m_LoopEffect = EffectManager::CreateLoopEffect(m_Position, "Asset\\Texture\\Sleep.png");
        m_Status = effect;
        m_StatusDuration = duration;
        PlayAnimation("Sleep", 1.0f);
        m_DefaultAnim = "Sleep";
        break;
    case Status::Poison:
        MessageLog::Instance().AddMessage(m_Name + u8"は毒を受けた。");
        // 毒は攻撃低下として扱い、同じ処理で他の攻撃デバフと重ねられるようにする。
        AddStatModifierStage(StatModifierType::Attack, -1);
        m_Status = effect;
        m_StatusDuration = duration;
        break;
    default:
        break;
    }

}

void Unit::ClearStatus()
{
    if (m_Status == Status::None) return;

    std::string msg = m_Name;

    // 状態異常解除時は待機アニメへ戻す。ただし攻撃・被ダメージ演出中は上書きせず、演出待ちだけ残るディレイを防ぐ。
    auto playIdleIfFree = [&]()
    {
        m_DefaultAnim = "Idle";
        if (!m_IsActingAnimation && m_MoveState == MoveState::Idle) {
            PlayAnimation("Idle", 1.0f);
        }
    };

    switch (m_Status) {
    case Status::Confusion:
        MessageLog::Instance().AddMessage(msg + u8"の混乱が解けた。");
        playIdleIfFree();
        break;
    case Status::Sleep:
        m_BlockActionOnceAfterStatusClear = true;
        MessageLog::Instance().AddMessage(msg + u8"は目を覚ました。");
        playIdleIfFree();
        break;
    case Status::Paralysis:
        m_BlockActionOnceAfterStatusClear = true;
        MessageLog::Instance().AddMessage(msg + u8"のかなしばりが解けた。");
        playIdleIfFree();
        break;
    case Status::Nap:
        playIdleIfFree();
        break;
    case Status::Poison:
        MessageLog::Instance().AddMessage(msg + u8"の毒が消えた。");
        ClearStatModifierStage(StatModifierType::Attack);
        break;
    default:
        break;
    }

    m_Status = Status::None;
    m_StatusDuration = 0;
    StopLoopEffect();
}

bool Unit::CheckHit(int acc, int evd)//外した?
{
    int rate = acc - evd;
    if (rate < 5) rate = 5;
    if (rate > 95) rate = 95;

    int r = GameRandom::Range(0, 99);
    return r < rate;
}

int Unit::CalcDamage(int atk, int def, float atkRate, float defRate)
{
    float randRange = GameRandom::Range(0.875f, 1.125f);

    float baseAtk = atk * atkRate * randRange;

    float defenseMultiplier = pow(0.94f, def * defRate);
    int dmg = static_cast<int>(baseAtk * defenseMultiplier + 0.5f); // 四捨五入

    // 最低ダメージ保証
    if (dmg < 1) dmg = 1;

    return dmg;
}

// ===============================
// 斜め移動の角抜けチェック
// ===============================
bool Unit::IsDiagonalMoveBlocked(Vector2Int cur, Vector2Int dir, MapData* map)
{
    // 斜め移動でなければ通す
    if (abs(dir.x) + abs(dir.y) != 2)
        return false;

    Vector2Int check1(cur.x + dir.x, cur.y);
    Vector2Int check2(cur.x, cur.y + dir.y);

    if (!map->IsWalkable(check1.x, check1.y)) return true;
    if (!map->IsWalkable(check2.x, check2.y)) return true;

    return false;
}
void Unit::UpdateAnimation()
{
    if (!m_AnimationModel) return;
    if (m_Status == Status::Paralysis)return;

    m_Frame += m_AnimSpeed;
    m_AnimationBlend += 0.1f; // ブレンド速度（固定値または変数）

    if (m_AnimationBlend >= 1.0f)
    {
        m_AnimationBlend = 1.0f;
        if (!m_AnimNext.empty())
        {
            m_AnimNow = m_AnimNext;
            m_AnimNext.clear();
        }
    }

    // ---  Trigger (単発再生) の終了判定 ---
    if (!m_IsAnimLooping)
    {
        // モデルから現在の最大フレーム数を取得
        int maxFrame = m_AnimationModel->GetAnimationFrameCount(m_AnimNow);

        // 最終フレームに達したか判定
        if (m_Frame >= (float)(maxFrame - 1))
        {
            m_IsAnimLooping = true; // ループ(Idle)に戻る
            PlayAnimation(m_DefaultAnim, 1.0f); // Idleへ遷移
        }
    }
    m_AnimationModel->Update(
        m_AnimNow.c_str(), (int)m_Frame,
        m_AnimNext.empty() ? m_AnimNow.c_str() : m_AnimNext.c_str(),
        (int)m_Frame,
        m_AnimationBlend
    );
}
void Unit::StartMove(const Vector2Int& target, float moveTime)
{
    MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
    Vector2Int dir = target - m_GridPos;
    bool invalidTarget = false;

    if (!map) {
        invalidTarget = true;
    }
    else if (!map->IsInBounds(target) || !map->IsWalkable(target)) {
        invalidTarget = true;
    }
    else if (dir.x == 0 && dir.y == 0) {
        invalidTarget = true;
    }
    else if (abs(dir.x) > 1 || abs(dir.y) > 1) {
        invalidTarget = true;
    }
    else if (IsDiagonalMoveBlocked(m_GridPos, dir, map)) {
        invalidTarget = true;
    }

    if (invalidTarget) {
        return;
    }

    // 視界/LODは移動演出が終わるまで移動開始マスを基準にする。
    m_MoveStartGridPos = m_GridPos;
    m_MoveTarget = target;
    m_MoveStartPos = m_Position;
    m_MoveEndPos = GridToWorld(target);
    m_MoveDuration = moveTime;
    m_MoveTimer = 0.0f;
    m_VisualRotationOffset = Vector3(0.0f, 0.0f, 0.0f);
    m_MovedThisTurn = true;

    //プレイヤーの視界外では演出をカット
    bool skipMoveAnimation = s_SkipMoveAnimation;
    if (!skipMoveAnimation) {
        Player* player = UnitManager::Instance() ? UnitManager::Instance()->GetPlayer() : nullptr;
        if (player && player != this && !player->IsInView(m_GridPos) && !player->IsInView(target)) {
            skipMoveAnimation = true;
        }
    }

    m_GridPos = target;
    m_LastValidGridPos = target;

    if (skipMoveAnimation) {
        m_Position = m_MoveEndPos;
        m_MoveState = MoveState::Idle;
        m_IsAnimatingMove = false;
        m_VisualRotationOffset = Vector3(0.0f, 0.0f, 0.0f);
        MapManager::Instance()->AfterUnitMoved(this);
        PlayAnimation("Idle", 1.0f);
        return;
    }

    m_MoveState = MoveState::Moving;
    m_IsAnimatingMove = true;
    PlayAnimation("Run", 1.0f);
}
void Unit::RequestMove(const Vector2Int& target)
{
    UnitManager* um = UnitManager::Instance();
    MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
    Vector2Int dir = target - m_GridPos;
    if (dir.x != 0 || dir.y != 0)
    {
        LookAt(dir);
    }
    if (!map || !map->IsInBounds(target) || !map->IsWalkable(target)) return;
    if (abs(dir.x) > 1 || abs(dir.y) > 1) return;
    if (IsDiagonalMoveBlocked(m_GridPos, dir, map)) return;
    if (um && um->GetUnitAt(target)) return;

    StartMove(target, m_MoveDuration);
}
bool Unit::RequestMoveBool(const Vector2Int& target)
{
    UnitManager* um = UnitManager::Instance();
    MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;

    // 方向だけ向くのはOK
    Vector2Int dir = target - m_GridPos;
    if (dir.x != 0 || dir.y != 0)
    {
        LookAt(dir);
    }

    if (!map || !map->IsInBounds(target) || !map->IsWalkable(target)) return false;
    if (abs(dir.x) > 1 || abs(dir.y) > 1) return false;
    if (IsDiagonalMoveBlocked(m_GridPos, dir, map)) return false;

    Unit* unit = um ? um->GetUnitAt(target) : nullptr;
    if (unit && unit != this) return false;

    StartMove(target, m_MoveDuration);
    return true;
}
bool Unit::HasKnockbackImpactDamage() const
{
    return m_KnockbackImpactDamage > 0 || m_KnockbackCollisionDamage > 0;
}

void Unit::ClearKnockbackImpactDamage()
{
    m_KnockbackImpactDamage = 0;
    m_KnockbackImpactAttacker = nullptr;
    m_KnockbackCollisionUnit = nullptr;
    m_KnockbackCollisionDamage = 0;
}

void Unit::ApplyKnockbackImpactDamage()
{
    if (!HasKnockbackImpactDamage()) return;

    int damage = m_KnockbackImpactDamage;
    Unit* attacker = m_KnockbackImpactAttacker;
    Unit* collisionUnit = m_KnockbackCollisionUnit;
    int collisionDamage = m_KnockbackCollisionDamage;

    ClearKnockbackImpactDamage();

    if (damage > 0) ConstantDamage(damage, attacker, false);
    if (collisionUnit && collisionDamage > 0 && collisionUnit->GetHP() > 0)
    {
        collisionUnit->ConstantDamage(collisionDamage, this, false);
    }
}

void Unit::StartKnockback(const Vector2Int& target, int impactDamage, Unit* attacker, Unit* collisionUnit, int collisionDamage)
{
    MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
    if (map && (!map->IsInBounds(target) || !map->IsWalkable(target))) {
        return;
    }
    m_KnockbackImpactDamage = (std::max)(0, impactDamage);
    m_KnockbackImpactAttacker = attacker;
    m_KnockbackCollisionUnit = (collisionUnit != this) ? collisionUnit : nullptr;
    m_KnockbackCollisionDamage = m_KnockbackCollisionUnit ? (std::max)(0, collisionDamage) : 0;

    // 吹き飛び中も視界/LODは移動開始マスで固定する。
    m_MoveStartGridPos = m_GridPos;
    m_MoveTarget = target;
    m_MoveStartPos = m_Position;
    m_MoveEndPos = GridToWorld(target);
    float worldDistance = (m_MoveEndPos - m_MoveStartPos).LengthSqrt();
    m_MoveDuration = worldDistance / 30.0f;
    if (m_MoveDuration < 0.05f) m_MoveDuration = 0.05f;
    m_MoveTimer = 0.0f;
    m_MovedThisTurn = true;
    m_VisualRotationOffset = Vector3(0.0f, 0.0f, 0.0f);

    bool skipMoveAnimation = s_SkipMoveAnimation;
    if (!skipMoveAnimation) {
        Player* player = UnitManager::Instance() ? UnitManager::Instance()->GetPlayer() : nullptr;
        if (player && player != this && !player->IsInView(m_GridPos) && !player->IsInView(target)) {
            skipMoveAnimation = true;
        }
    }

    m_GridPos = target;
    m_LastValidGridPos = target;

    if (skipMoveAnimation) {
        m_Position = m_MoveEndPos;
        m_MoveState = MoveState::Idle;
        m_IsAnimatingMove = false;
        m_VisualRotationOffset = Vector3(0.0f, 0.0f, 0.0f);
        ApplyKnockbackImpactDamage();
        MapManager::Instance()->AfterUnitMoved(this);
        PlayAnimation("Idle", 1.0f);
        return;
    }

    m_MoveState = MoveState::Knockback;
    m_IsAnimatingMove = true;
    PlayAnimation("Damaged", 1.0f);
}

void Unit::StartSummonAppear(float duration)
{
    m_MoveStartGridPos = m_GridPos;
    m_MoveTarget = m_GridPos;
    m_MoveEndPos = GridToWorld(m_GridPos);
    m_MoveStartPos = m_MoveEndPos;
    m_MoveStartPos.y -= 4.0f;
    m_MoveDuration = duration;
    if (m_MoveDuration < 0.05f) m_MoveDuration = 0.05f;
    m_MoveTimer = 0.0f;
    m_VisualRotationOffset = Vector3(0.0f, 0.0f, 0.0f);

    bool skipMoveAnimation = s_SkipMoveAnimation;
    if (!skipMoveAnimation) {
        Player* player = UnitManager::Instance() ? UnitManager::Instance()->GetPlayer() : nullptr;
        if (player && player != this && !player->IsInView(m_GridPos)) {
            skipMoveAnimation = true;
        }
    }

    if (skipMoveAnimation) {
        m_Position = m_MoveEndPos;
        m_MoveState = MoveState::Idle;
        m_IsAnimatingMove = false;
        PlayAnimation("Idle", 1.0f);
        return;
    }

    m_Position = m_MoveStartPos;
    m_MoveState = MoveState::Summoning;
    m_IsAnimatingMove = true;
    PlayAnimation("Idle", 1.0f);
}
void Unit::StartWarp(const Vector2Int& targetGrid)
{
    MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
    if (map && (!map->IsInBounds(targetGrid) || !map->IsWalkable(targetGrid))) {
        return;
    }
    // ワープ中も到着演出が終わるまで視界/LODの基準を移動前に残す。
    m_MoveStartGridPos = m_GridPos;
    m_MoveTarget = targetGrid;
    m_MoveStartPos = m_Position; // 現在の世界座標から開始
    m_MoveEndPos = GridToWorld(targetGrid); // 目的地の世界座標
    m_MoveTimer = 0.0f;
    m_MoveDuration = 1.0f;
    m_MovedThisTurn = true;
    m_VisualRotationOffset = Vector3(0.0f, 0.0f, 0.0f);
    m_GridPos = targetGrid;
    m_LastValidGridPos = targetGrid;

    if (s_SkipMoveAnimation) {
        m_Position = m_MoveEndPos;
        m_MoveState = MoveState::Idle;
        m_IsAnimatingMove = false;
        MapManager::Instance()->AfterUnitMoved(this);
        PlayAnimation("Idle", 1.0f);
        return;
    }

    m_MoveState = MoveState::WarpFloating;
    m_IsAnimatingMove = true;
}


void Unit::UpdateLerpMove() {
    if (m_MoveState == MoveState::Idle) return;

    m_MoveTimer += Time::DeltaTime();

    float duration = m_MoveDuration;
    float t = m_MoveTimer / duration;
    float animT = Vector3::Clamp(t, 0.0f, 1.0f);

    switch (m_MoveState) {
    case MoveState::Moving:
        m_Position = Vector3::Lerp(m_MoveStartPos, m_MoveEndPos, animT);
        break;

    case MoveState::WarpFloating:
        if (animT < 0.5f) {
            float upT = animT * 2.0f;
            m_Position.y = sinf(upT * 1.57f) * 20.0f;
        }
        else {
            float downT = (animT - 0.5f) * 2.0f;
            m_Position.x = m_MoveEndPos.x;
            m_Position.z = m_MoveEndPos.z;
            m_Position.y = (1.0f - sinf(downT * 1.57f)) * 20.0f;
        }
        break;

    case MoveState::Knockback:
        m_Position = Vector3::Lerp(m_MoveStartPos, m_MoveEndPos, animT);
        break;

    case MoveState::KnockbackImpact:
        m_Position = m_MoveEndPos;
        break;

    case MoveState::Summoning:
        m_Position = Vector3::Lerp(m_MoveStartPos, m_MoveEndPos, animT);
        m_VisualRotationOffset.y = animT * 6.2831855f;
        break;
    }

    if (t >= 1.0f) {
        MoveState finishedState = m_MoveState;
        m_Position = m_MoveEndPos;
        m_Position.y = 0.0f;
        m_VisualRotationOffset = Vector3(0.0f, 0.0f, 0.0f);

        // 鉄球の衝突ダメージは、吹き飛び到達後に停止フレームを挟まずその場で確定させる。
        if (finishedState == MoveState::Knockback) ApplyKnockbackImpactDamage();

        m_MoveDuration = 0.1f;
        m_MoveState = MoveState::Idle;
        m_MoveTimer = 0.0f;
        m_IsAnimatingMove = false;
        if (finishedState != MoveState::Summoning) MapManager::Instance()->AfterUnitMoved(this);
        PlayAnimation("Idle", 1.0f);
    }
}
bool Unit::IsAnimationFinished() const
{
    int maxFrame = m_AnimationModel->GetAnimationFrameCount(m_AnimNow);
    return m_Frame >= maxFrame - 1;
}