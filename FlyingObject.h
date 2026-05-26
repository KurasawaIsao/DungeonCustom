#pragma once
#include "GameObject.h"
#include "Vector2Int.h"
#include <string>
#include "ItemInstance.h"
#include "EffectBase.h"

class Unit;

class FlyingObject : public GameObject {
public:
    void Init() override;
    void Uninit() override {};
    void Update() override;
    void Draw() override;

    void Fire(const std::string& modelPath, ItemInstance* sourceItem, const Vector3& startPos,
        const Vector2Int& startGrid, const Vector2Int& dir, float speed = 0.1f, int maxRange = 15,
        Unit* user = nullptr);

    void FireEffect(const std::string& modelPath, EffectBase* sourceEffect, Unit* user, EffectSourceType sourceType,
        const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& dir,
        float speed = 0.1f, int maxRange = 15, const ItemData* dropItemData = nullptr);

    void FireEffectToTarget(const std::string& modelPath, EffectBase* sourceEffect, Unit* user, EffectSourceType sourceType,
        const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& targetGrid,
        float speed = 0.35f);

    bool GetIsActive() { return m_IsActive; };

private:
    void OnHit();
    void SetupFlight(const std::string& modelPath, const Vector3& startPos,
        const Vector2Int& startGrid, const Vector2Int& dir, float speed, int maxRange);
    void SetupDirectFlight(const std::string& modelPath, const Vector3& startPos,
        const Vector2Int& startGrid, const Vector2Int& targetGrid, float speed);

    class ModelRenderer* m_Model = nullptr;
    ID3D11VertexShader* m_VS = nullptr;
    ID3D11PixelShader* m_PS = nullptr;
    ID3D11InputLayout* m_Layout = nullptr;

    ItemInstance* m_SourceItem = nullptr;
    EffectBase* m_SourceEffect = nullptr;
    const ItemData* m_DropItemData = nullptr;
    Unit* m_User = nullptr;
    EffectSourceType m_SourceType = EffectSourceType::Item;
    Vector2Int m_Dir = { 0, 0 };
    Vector3 m_StartPos;
    Vector3 m_TargetPos;
    Vector2Int m_TargetGrid;

    float m_FlyT = 0.0f;
    float m_Speed = 0.05f;
    bool m_HitWall = false;
    bool m_IsActive = false;
};
