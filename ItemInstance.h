// ItemInstance.h
#pragma once
#include "ItemData.h"
#include "Vector2Int.h"
#include "ItemIdentificationManager.h"
#include "imgui.h" // 色指定用
#include <memory>
#include <optional>
enum class BlessState
{
    Normal,
    Blessed,
    Cursed
};


struct PotData
{
    int capacity;
    bool canTakeOut;
    std::vector<ItemInstance> items;
};

enum class IdentifyState
{
    Unidentified,
    Identified
};

class MapData;
// アイテムの詳細情報を持つクラス。アイテムデータの参照を行い、呪い・祝福の状態や、スタック数、杖の残り回数など
// また、アイテムを投げる、使用するなどアイテムの共通処理もここに記述。
class ItemInstance
{
public:
    ItemInstance() : m_Data(nullptr) {}
    // または ItemInstance() = default; (ただしm_Dataがnullptrになるよう注意)

    ItemInstance(const ItemData* data)
        : m_Data(data)
    {
        if (data->identifiable)
        {
            m_Identify = IdentifyState::Unidentified;
        }
        else
        {
            m_Identify = IdentifyState::Identified;
        }
    }
    void InitIdentify(bool applyBlessOrCurse = true);
    bool HasCustomName() const { return !m_CustomName.empty(); }
    bool CanRename() { return m_Data->canRename; }

    void SetCustomName(const std::string& name)
    {
        m_CustomName = name;
    }

    void SetPlusValue(int v) { m_PlusValue = v; }
    int GetPlusValue() const { return m_PlusValue; }
    //アイテム名は大体ここから参照する
    std::string GetDisplayName() const;
    ImU32 GetNameColor() const;

    const std::string& GetTrueName() const
    {
        return m_Data->name;
    }
    const std::string& GetUnidetifiedName() const
    {
        auto& info = ItemIdentificationManager::Instance().GetInfo(m_Data->name);
        if (!info.unidentifiedName.empty()) {
            return info.unidentifiedName;
        }

        return m_Data->name;
    }


    void Use(Player* player, int inventoryIndex);
    void Throw(Player* player, Unit* hit, Vector2Int& pos);
    void CreatePot(int cap, bool takeOut);
    bool PutItem(ItemInstance&& item);
    std::optional<ItemInstance>TakeOutItem();
    void OnBreak(const Vector2Int& center);

    void TryIdentify(ItemCommandType cmd);
    void SetIdentified() { m_Identify = IdentifyState::Identified; }
    IdentifyState GetIdentifiedState()const{ return m_Identify; }


    bool IsBlessed() const
    {
        return m_Bless == BlessState::Blessed;
    }

    bool IsCursed() const
    {
        return m_Bless == BlessState::Cursed;
    }
    //　名前がそもそもつけられるかどうか
    bool IsOptionRevealed() const { return m_IsOptionRevealed; }

    // 状態を判明させる（装備した時や巻物を読んだ時）
    void RevealOption() { m_IsOptionRevealed = true; }

    bool IsPot() const
    {
        return m_Pot != nullptr;
    }
    PotData* GetPot() const
    {
        return m_Pot.get();
    }


    void SetBless(BlessState b) { m_Bless = b; }
    BlessState GetBless() const { return m_Bless; }
    void SetCharge(int value) { m_Charges = value; }
    int GetCharge() const { return m_Charges; }
    void SetStackCount(int value) { m_StackCount = value < 1 ? 1 : value; }
    int GetStackCount() const { return m_StackCount; }

    const ItemData* GetData() const { return m_Data; }
	int GetBuyPrice() const { return  m_Data->price; }
	int GetSellPrice() const { return m_Data->sellPrice; }
    void SetUnpaidShopItem(bool unpaid, int price = 0) { m_IsUnpaidShopItem = unpaid; m_UnpaidShopPrice = unpaid ? price : 0; }
    bool IsUnpaidShopItem() const { return m_IsUnpaidShopItem; }
    int GetUnpaidShopPrice() const { return m_UnpaidShopPrice; }

private:
    const ItemData* m_Data;

    bool m_IsOptionRevealed = false; // 呪い・祝福の状態が判明しているか
    int m_Charges = 0;
    int m_StackCount = 1;
    int m_UnidentifiedUsageCount = 0;//未識別時の杖の回数
    std::unique_ptr<PotData> m_Pot;//つぼ?
    int m_PlusValue = 0;//武器修正値

    std::string m_CustomName;

    BlessState m_Bless = BlessState::Normal;
    IdentifyState m_Identify = IdentifyState::Unidentified;
    bool m_IsUnpaidShopItem = false;
    int m_UnpaidShopPrice = 0;
};
