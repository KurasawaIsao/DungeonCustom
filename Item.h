#pragma once
#include "MapObject.h"
#include "ItemInstance.h"

class Item : public MapObject
{
private:
    class ModelRenderer* m_Model = nullptr;
    ID3D11VertexShader* m_VS = nullptr;
    ID3D11PixelShader* m_PS = nullptr;
    ID3D11InputLayout* m_Layout = nullptr;

    bool m_HitWall = false;
    bool m_IsFlying = false;
    Vector3 m_StartPos;
    Vector3 m_TargetPos;
    Vector2Int m_TargetGrid;
    float m_FlyT = 0.0f;     
    float m_FlyDuration = 0.5f; // 飛翔にかかる秒数
    class Player* m_Thrower = nullptr; // 誰が投げたか（効果計算用）
    bool m_IsShopItem = false;
    bool m_IsPlayerShopItem = false;

protected:
    std::string m_Name;
    std::string m_Description;

    ItemInstance m_Instance;


public:
    void Init() override;
    void Update() override;
    void Draw() override;

    void Uninit() override;

    void StartThrow(Player* thrower, const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& direction);
    void OnArrived();

    bool GetIsFlying() { return m_IsFlying; }

    void SetInstance(ItemInstance&& inst)
    {
        m_Instance = std::move(inst);
        SetupFromInstance();
    }
    void SetupFromInstance();
    ItemInstance& GetInstance(){ return m_Instance; }
    const ItemInstance& GetInstance() const { return m_Instance; }
    void SetShopItem(bool isShopItem) { m_IsShopItem = isShopItem;}
    bool IsShopItem() const { return m_IsShopItem; }
    void SetPlayerShopItem(bool isShopItem) { m_IsPlayerShopItem = isShopItem; }
    bool IsPlayerShopItem() const { return m_IsPlayerShopItem; }
    void MarkPurchased() { m_IsShopItem = false; }
    void OnStepped(Player* player) override;
    void OnHitWall();
   

};
