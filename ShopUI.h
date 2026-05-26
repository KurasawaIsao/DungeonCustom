#pragma once
#include "GameObject.h"
#include "GameUIObject.h"
#include "ItemInstance.h"

class Enemy;
class Item;
class Player;
class Vector2Int;

class ShopUI : public GameObject, public GameUIObject
{
public:
    void Init() override { Close(); }
    void Update() override {}
    void Draw() override {}
    void Uninit() override {}
    void DrawGameUI() override;

    void OpenShopBuyMenu(Item* item);
    void OpenShopTradeMenu(bool forcedCheckout = false, Enemy* waitForKeeper = nullptr);
    void Close();
    bool IsAnyMenuOpen() const { return m_Mode != Mode::Closed; }

    static bool IsPlayerOnShopTile(Player* player);
    static int GetItemPrice(const ItemInstance& inst);
    static int GetItemSellPrice(const ItemInstance& inst);
    static int GetUnpaidShopTotal(Player* player);
    static bool PayForUnpaidShopItems(Player* player);
    static int GetPendingShopSaleTotal(Player* player);
    static bool HasPendingShopSales(Player* player) { return GetPendingShopSaleTotal(player) > 0; }
    static int FinalizePendingShopSales(Player* player);
    static void SellInventoryItem(Player* player, int inventoryIndex);
    static void StartTheftMode();
    static void AngerShopKeeper(Enemy* enemy);

private:
    enum class Mode
    {
        Closed,
        Buy,
        Trade
    };

    Mode m_Mode = Mode::Closed;
    Item* m_ShopItemTarget = nullptr;
    bool m_ForcedCheckout = false;
    bool m_ConfirmMessageShown = false;
    Enemy* m_WaitingKeeper = nullptr;
    int m_SelectedChoice = 0;

    void DrawShopBuyUI(Player* player);
    void DrawShopTradeUI(Player* player);
};
