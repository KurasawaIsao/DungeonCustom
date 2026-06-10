#pragma once
#include "GameObject.h"
#include "Vector2Int.h"
#include <string>
#include <optional>
#include <vector>
#include "ItemInstance.h"
#include "EffectBase.h"

class Unit;
class MapData;

class FlyingObject : public GameObject {
public:
    void Init() override;
    void Uninit() override {};
    void Update() override;
    void Draw() override;

    void Fire(const std::string& modelPath, ItemInstance* sourceItem, const Vector3& startPos,
        const Vector2Int& startGrid, const Vector2Int& dir, float speed = 0.1f, int maxRange = 15,
        Unit* user = nullptr);

    // 投擲アイテムと矢は同じ直線飛行・着弾処理を使用する。
    void FireItem(ItemInstance&& item, Unit* user,
        const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& dir,
        float speed = 0.1f, int maxRange = 10);

    // 壺が割れた時の中身も、投擲アイテムと同じ着地・罠処理へ渡す。
    void DropItemFromPot(ItemInstance&& item, const Vector2Int& centerGrid);

    void FireEffect(const std::string& modelPath, EffectBase* sourceEffect, Unit* user, EffectSourceType sourceType,
        const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& dir,
        float speed = 0.1f, int maxRange = 15);

    void FireEffectToTarget(const std::string& modelPath, EffectBase* sourceEffect, Unit* user, EffectSourceType sourceType,
        const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& targetGrid,
        float speed = 0.35f);

    bool GetIsActive() { return m_IsActive; };

private:
    void OnHit();
    bool ResolveFlyingItemHit(MapData* map, Unit* hitUnit);
    bool DropFlyingItem(MapData* map);
    void UpdateDropResolution();
    void ContinueDropSearch(MapData* map);
    void HandleDropCandidate(MapData* map, const Vector2Int& grid, bool isTrap, bool allowMove);
    void StartDropMove(const Vector2Int& grid, bool isTrap);
    void StartTrapWait(MapData* map, const Vector2Int& grid);
    void PlaceFlyingItem(MapData* map, const Vector2Int& grid);
    void SetupFlight(const Vector3& startPos,
        const Vector2Int& startGrid, const Vector2Int& dir, float speed, int maxRange);
    void SetupDirectFlight(const std::string& modelPath, const Vector3& startPos,
        const Vector2Int& startGrid, const Vector2Int& targetGrid, float speed);

    class ModelRenderer* m_Model = nullptr;
    ID3D11VertexShader* m_VS = nullptr;
    ID3D11PixelShader* m_PS = nullptr;
    ID3D11InputLayout* m_Layout = nullptr;

    ItemInstance* m_SourceItem = nullptr;
    std::optional<ItemInstance> m_FlyingItem;
    EffectBase* m_SourceEffect = nullptr;
    Unit* m_User = nullptr;
    EffectSourceType m_SourceType = EffectSourceType::Item;
    Vector2Int m_Dir = { 0, 0 };
    Vector3 m_StartPos;
    Vector3 m_TargetPos;
    Vector2Int m_TargetGrid;

    float m_FlyT = 0.0f;
    float m_Speed = 0.05f;
    float m_ItemSpin = 0.0f;
    float m_ArcDuration = 0.5f;
    bool m_RotateItemInFlight = false;
    bool m_UseArcFlight = false;
    enum class DropResolutionState
    {
        None,
        MovingToCandidate,
        WaitingForTrap
    };

    std::vector<Vector2Int> m_CheckedTrapGrids;
    Vector2Int m_PendingDropGrid;
    DropResolutionState m_DropResolutionState = DropResolutionState::None;
    float m_DropMoveSpeed = 0.45f;
    float m_TrapWaitTimer = 0.0f;
    bool m_PendingDropIsTrap = false;
    bool m_HitWall = false;
    bool m_IsActive = false;
};
