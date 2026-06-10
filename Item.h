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

    void SetInstance(ItemInstance&& inst)
    {
        m_Instance = std::move(inst);
        SetupFromInstance();
    }
    // アイテム種別に対応するモデルを、表示先のレンダラーへ読み込む。
    static void LoadModelForType(ModelRenderer* model, ItemType type);
    void SetupFromInstance();
    ItemInstance& GetInstance(){ return m_Instance; }
    const ItemInstance& GetInstance() const { return m_Instance; }
    void SetShopItem(bool isShopItem) { m_IsShopItem = isShopItem;}
    bool IsShopItem() const { return m_IsShopItem; }
    void SetPlayerShopItem(bool isShopItem) { m_IsPlayerShopItem = isShopItem; }
    bool IsPlayerShopItem() const { return m_IsPlayerShopItem; }
    void MarkPurchased() { m_IsShopItem = false; }
    void OnStepped(Player* player) override;
};
