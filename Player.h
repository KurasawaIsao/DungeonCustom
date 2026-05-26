#pragma once
#include "main.h"
#include "Unit.h"
#include "Vector2Int.h"
#include "renderer.h"
#include "ItemInstance.h"
#include <string>
#include <vector>
#include <algorithm>

class Enemy;

struct InventoryItem
{
    ItemInstance instance;
    int count;
    bool isEquipped = false;

    InventoryItem(ItemInstance&& inst)
        : instance(std::move(inst)), count(instance.GetStackCount())
    {
        instance.SetStackCount(count);
    }
};

class Player : public Unit {
private:
    // --- ステータス・リソース ---
    int m_Fullness = 100;
    int m_MaxFullness = 100;
    // m_FullnessCounter が m_FullnessInterval に達するたび満腹度を1減らす。
    int m_FullnessCounter = 0;
    int m_FullnessInterval = 10;
    std::string m_PendingDeathCause;
    int m_Gold = 1000;
    int m_Strength = 8;
    int m_MaxStrength = 8;

    // --- インベントリ・装備 ---
    std::vector<InventoryItem> m_Items;
    int m_EquippedWeaponIndex = -1;
    int m_EquippedShieldIndex = -1;
    int m_EquippedArrowIndex = -1;

    // --- 入力・移動制御 ---
    Vector2Int m_LastInputDir = { 0, 0 };
    Vector2Int m_PreviousGridPos = { 0, 0 };
    bool m_IsDash = false;
    // ダッシュ停止直後、Shift を離すまで再ダッシュしないための入力ロック。
    bool m_DashWaitShiftRelease = false;
    // 部屋入口で止まった後、次フレームに同じ方向へ進むための予約情報。
    bool m_HasPendingEntranceDash = false;
    Vector2Int m_PendingEntranceDashPos = { -1, -1 };
    // ダッシュ中に既に検知済みの隣接敵。新規出現だけを停止条件にするため保持する。
    std::vector<Enemy*> m_KnownDashAdjacentEnemies;
    bool m_InputEnable = true;
    bool m_StairConfirmed = false;

    // --- 定数パラメータ ---
    static constexpr float MOVE_TIME_NORMAL = 0.12f;
    static constexpr float MOVE_TIME_DASH = 0.06f;
    static constexpr int CLEAR_VISION_LOD_DISTANCE = 15;

public:
    // 基本ライフサイクル
    void Init() override;
    void Update() override;     // 演出・UI更新
    void UpdateUnit() override; // ターン行動処理
    void Draw() override;
    void Uninit() override;

    // 行動コマンド
    void MoveInput();
    void Move(const Vector2Int& dir);
    void ExecuteConfusionAction();
    bool TryConfusionMove(const Vector2Int& dir);
    bool CanConfusionAttack(const Vector2Int& dir);
    void FaceDirection(const Vector2Int& dir);
    void ExecuteInstantDash(const Vector2Int& dir);
    bool CanInstantDashStep(const Vector2Int& dir);
    bool IsInstantDashStopTile(const Vector2Int& gridPos);
    bool IsCorridorIntersectionTile(const Vector2Int& gridPos);
    bool IsVisibleTrapAt(const Vector2Int& gridPos);
    bool IsItemAt(const Vector2Int& gridPos);
    bool IsItemAdjacent(const Vector2Int& gridPos);
    bool HasNewDashAdjacentEnemy();
    void UpdateKnownDashAdjacentEnemies();
    bool ShouldStopDashForEnemyAdjacent();
    bool ShouldStopDashForRoomEnemyAdjacent();
    bool IsEnemyAdjacent();
    void InstantMoveTo(const Vector2Int& gridPos, bool suppressObjectStep = false);
    void Attack() override;
    void UseItem(int index);
    void ThrowItem(int index);
    void ShootArrow(int index);
    void EquipItem(int index);
    void EndTurn();

    // 装備操作
    void UnequipWeapon();
    void RefreshEquipIndices();

    // ユーティリティ
    void AddItem(ItemInstance&& inst);
    void RemoveItemAt(int index);
    void SortItems();
    void ConsumeFullness();
    void AddFullness(int value);
    void AddGold(int value);
    bool SpendGold(int value);
    int GetGold() const { return m_Gold; }
    int GetFullness() const { return m_Fullness; }
    int GetMaxFullness() const { return m_MaxFullness; }
    int GetStrength() const { return m_Strength; }
    int GetMaxStrength() const { return m_MaxStrength; }
    void SetStrength(int value);
    void SetMaxStrength(int value);
    bool ReduceStrength(int value);
    bool RestoreStrengthToMax();
    void OnDeath(Unit* attacker = nullptr) override;
    void ResetPosition(Vector2Int gridPos);
    void SetInputEnable(bool enable) { m_InputEnable = enable; }
    void SetPendingDeathCause(const std::string& cause) { m_PendingDeathCause = cause; }

    // ゲッター
    int GetATK() const override;
    int GetDEF() const override;
    bool IsInView(const Vector2Int& targetGridPos) const;
    int GetViewDistance() const;
    Vector2Int GetVisionGridPos() const;
    bool IsStairConfirming() const;
    std::vector<InventoryItem>& GetItems() { return m_Items; }
    int GetEquippedWeaponIndex() const { return m_EquippedWeaponIndex; }
    int GetEquippedArrowIndex() const { return m_EquippedArrowIndex; }
    Vector2Int GetPreviousGridPos() const { return m_PreviousGridPos; }
};
