#include "ShopUI.h"
#include "player.h"
#include "Item.h"
#include "ItemData.h"
#include "MapManager.h"
#include "MapData.h"
#include "Room.h"
#include "manager.h"
#include "scene.h"
#include "MessageLog.h"
#include "TurnManager.h"
#include "UnitManager.h"
#include "Enemy.h"
#include "EffectManager.h"
#include "Input.h"
#include "Renderer.h"
#include "UIRenderer.h"
#include "UITextRenderer.h"
#include <algorithm>
#include <string>

namespace
{
    bool GetShopFloorSquare(const Room& room, Vector2Int& topLeft, int& side)
    {
        const Vector2Int pos = room.GetPosition();
        const Vector2Int size = room.GetSize();
        const int innerW = (std::max)(0, size.x - 2);
        const int innerH = (std::max)(0, size.y - 2);
        side = (std::min)(innerW, innerH);
        if (side <= 0) return false;

        // ShopFloorと同じ正方形だけを店タイルとして扱う。
        topLeft.x = pos.x + 1 + (innerW - side) / 2;
        topLeft.y = pos.y + 1 + (innerH - side) / 2;
        return true;
    }

    bool IsInsideSquare(const Vector2Int& pos, const Vector2Int& topLeft, int side)
    {
        return pos.x >= topLeft.x && pos.x < topLeft.x + side
            && pos.y >= topLeft.y && pos.y < topLeft.y + side;
    }

    void RestoreMainRenderTarget()
    {
        ID3D11RenderTargetView* rtv = Renderer::GetMainRenderTargetView();
        Renderer::GetDeviceContext()->OMSetRenderTargets(1, &rtv, nullptr);
    }

    void DrawChoiceLine(const char* text, bool selected, float x, float y, float width)
    {
        std::string line = selected ? u8"・ " : "  ";
        line += text;
        UITextRenderer::DrawOutlinedTextUtf8(
            line,
            x,
            y,
            width,
            36.0f,
            28.0f,
            selected ? D2D1::ColorF(1.0f, 0.92f, 0.45f, 1.0f) : D2D1::ColorF(D2D1::ColorF::White));
    }

    void DrawHorizontalChoice(const char* text, bool selected, float x, float y, float width)
    {
        std::string line = selected ? u8"・ " : "  ";
        line += text;
        UITextRenderer::DrawOutlinedTextUtf8(
            line,
            x,
            y,
            width,
            36.0f,
            28.0f,
            selected ? D2D1::ColorF(1.0f, 0.92f, 0.45f, 1.0f) : D2D1::ColorF(D2D1::ColorF::White));
    }
    int ClearPendingShopSales()
    {
        MapData* map = MapManager::Instance()->GetCurrentMap();
        if (!map) return 0;

        int count = 0;
        for (const Room& room : map->GetRooms())
        {
            if (room.specialType != RoomSpecialType::Shop) continue;

            Vector2Int topLeft;
            int side = 0;
            if (!GetShopFloorSquare(room, topLeft, side)) continue;

            for (int y = topLeft.y; y < topLeft.y + side; ++y)
            {
                for (int x = topLeft.x; x < topLeft.x + side; ++x)
                {
                    Item* item = dynamic_cast<Item*>(map->GetObjectAt(x, y));
                    if (!item || !item->IsPlayerShopItem()) continue;

                    item->SetPlayerShopItem(false);
                    ++count;
                }
            }
        }
        return count;
    }
}

void ShopUI::DrawGameUI()
{
    if (m_Mode == Mode::Closed) return;

    // 店外退出時は店番のワープ演出が終わってから、精算確認を表示する。
    if (m_WaitingKeeper && m_WaitingKeeper->IsAnimatingMove()) return;
    m_WaitingKeeper = nullptr;

    Player* player = Manager::GetScene()->GetGameObject<Player>();
    if (m_Mode == Mode::Buy) DrawShopBuyUI(player);
    else if (m_Mode == Mode::Trade) DrawShopTradeUI(player);
}

void ShopUI::OpenShopBuyMenu(Item* item)
{
    m_ShopItemTarget = item;
    m_ForcedCheckout = false;
    m_SelectedChoice = 0;
    m_Mode = Mode::Buy;
}

void ShopUI::OpenShopTradeMenu(bool forcedCheckout, Enemy* waitForKeeper)
{
    m_ShopItemTarget = nullptr;
    m_ForcedCheckout = forcedCheckout;
    m_ConfirmMessageShown = false;
    m_WaitingKeeper = waitForKeeper;
    m_SelectedChoice = 0;
    m_Mode = Mode::Trade;
}

void ShopUI::Close()
{
    m_Mode = Mode::Closed;
    m_ShopItemTarget = nullptr;
    m_ForcedCheckout = false;
    m_ConfirmMessageShown = false;
    m_WaitingKeeper = nullptr;
    m_SelectedChoice = 0;
}

bool ShopUI::IsPlayerOnShopTile(Player* player)
{
    if (!player) return false;
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return false;

    const Vector2Int playerPos = player->GetGridPos();
    Room* room = map->GetRoomAt(playerPos);
    if (!room || room->specialType != RoomSpecialType::Shop) return false;

    Vector2Int topLeft;
    int side = 0;
    return GetShopFloorSquare(*room, topLeft, side) && IsInsideSquare(playerPos, topLeft, side);
}

int ShopUI::GetItemPrice(const ItemInstance& inst)
{
	return inst.GetData()->price;
}

int ShopUI::GetItemSellPrice(const ItemInstance& inst)
{
	return inst.GetSellPrice();
}

int ShopUI::GetUnpaidShopTotal(Player* player)
{
    if (!player) return 0;
    int total = 0;
    for (const auto& item : player->GetItems())
    {
        if (item.instance.IsUnpaidShopItem())
            total += item.instance.GetUnpaidShopPrice();
    }
    return total;
}

bool ShopUI::PayForUnpaidShopItems(Player* player)
{
    if (!player) return false;
    const int total = GetUnpaidShopTotal(player);
    if (total <= 0) return true;
    if (!player->SpendGold(total))
    {
        MessageLog::Instance().AddMessage(u8"お金が足りない。");
        return false;
    }

    for (auto& item : player->GetItems())
    {
        if (item.instance.IsUnpaidShopItem())
            item.instance.SetUnpaidShopItem(false);
    }
    MessageLog::Instance().AddMessage(u8"代金" + std::to_string(total) + u8"Gを支払った。");
    return true;
}

int ShopUI::GetPendingShopSaleTotal(Player* player)
{
    if (!player) return 0;
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return 0;

    int total = 0;
    for (const Room& room : map->GetRooms())
    {
        if (room.specialType != RoomSpecialType::Shop) continue;

        Vector2Int topLeft;
        int side = 0;
        if (!GetShopFloorSquare(room, topLeft, side)) continue;

        for (int y = topLeft.y; y < topLeft.y + side; ++y)
        {
            for (int x = topLeft.x; x < topLeft.x + side; ++x)
            {
                Item* item = dynamic_cast<Item*>(map->GetObjectAt(x, y));
                if (item && item->IsPlayerShopItem()) total += item->GetInstance().GetSellPrice();
            }
        }
    }
    return total;
}

int ShopUI::FinalizePendingShopSales(Player* player)
{
    if (!player) return 0;
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return 0;

    int total = 0;
    for (const Room& room : map->GetRooms())
    {
        if (room.specialType != RoomSpecialType::Shop) continue;

        Vector2Int topLeft;
        int side = 0;
        if (!GetShopFloorSquare(room, topLeft, side)) continue;

        for (int y = topLeft.y; y < topLeft.y + side; ++y)
        {
            for (int x = topLeft.x; x < topLeft.x + side; ++x)
            {
                Item* item = dynamic_cast<Item*>(map->GetObjectAt(x, y));
                if (!item || !item->IsPlayerShopItem()) continue;

                total += item->GetInstance().GetSellPrice();
                item->SetPlayerShopItem(false);
                item->SetShopItem(true);

                //map->RemoveMapObject(item);
                //item->SetDestroy();
            }
        }
    }

    if (total > 0)
    {
        player->AddGold(total);
        MessageLog::Instance().AddMessage(u8"売却代金" + std::to_string(total) + u8"Gを受け取った。");
    }
    return total;
}

void ShopUI::SellInventoryItem(Player* player, int inventoryIndex)
{
    if (!player) return;
    auto& items = player->GetItems();
    if (inventoryIndex < 0 || inventoryIndex >= (int)items.size()) return;

    const ItemInstance& inst = items[inventoryIndex].instance;
    const int sellPrice = GetItemSellPrice(inst);
    const std::string name = inst.GetDisplayName();

    player->AddGold(sellPrice);
    player->RemoveItemAt(inventoryIndex);
    MessageLog::Instance().AddMessage(name + u8"を売った。" + std::to_string(sellPrice) + u8"G手に入れた。");
    player->EndTurn();
}

void ShopUI::StartTheftMode()
{
    MessageLog::Instance().AddMessage(u8"ドロボー！！！");
    TurnManager::Instance()->SetShopTheftMode(true);
    EffectManager::PlayBGM("Asset\\Sound\\tataminouenoshitou.wav");
    for (Enemy* enemy : UnitManager::Instance()->GetEnemies())
    {
        if (enemy && enemy->IsShopKeeper()) enemy->SetShopKeeperMode(true);
    }
}

void ShopUI::AngerShopKeeper(Enemy* enemy)
{
    if (!enemy || !enemy->IsShopKeeper() || enemy->IsShopHostile()) return;
    MessageLog::Instance().AddMessage(u8"店番を怒らせた！");
    enemy->SetShopKeeperMode(true);
    //TurnManager::Instance()->SetShopTheftMode(true);
}

void ShopUI::DrawShopBuyUI(Player* player)
{
    if (!player || !m_ShopItemTarget || !m_ShopItemTarget->IsShopItem())
    {
        Close();
        return;
    }

    if (Input::GetKeyTrigger(VK_UP) || Input::GetKeyTrigger(VK_DOWN)) m_SelectedChoice = 1 - m_SelectedChoice;
    if (Input::GetKeyTrigger('X')) m_SelectedChoice = 1;

    const ItemInstance& inst = m_ShopItemTarget->GetInstance();
    const int price = inst.GetBuyPrice();

    if (Input::GetKeyTrigger('Z') || Input::GetKeyTrigger('X'))
    {
        if (m_SelectedChoice == 0)
        {
            auto& items = player->GetItems();
            if ((int)items.size() >= 20)
            {
                MessageLog::Instance().AddMessage(u8"持ち物がいっぱいで拾えない。");
            }
            else
            {
                const std::string name = inst.GetDisplayName();
                // 「拾う」は未払い品として持つだけ。支払いは店番会話か店外退出時に確定する。
                m_ShopItemTarget->GetInstance().SetUnpaidShopItem(true, price);
                m_ShopItemTarget->MarkPurchased();
                player->AddItem(std::move(m_ShopItemTarget->GetInstance()));
                MapManager::Instance()->GetCurrentMap()->RemoveMapObject(m_ShopItemTarget);
                m_ShopItemTarget->SetDestroy();
                MessageLog::Instance().AddMessage(name + u8"を拾った。代金" + std::to_string(price) + u8"Gはあとで支払う。");
                Close();
                return;
            }
        }
        else
        {
            Close();
            return;
        }
    }

    UIRect rect{ SCREEN_WIDTH * 0.5f - 190.0f, SCREEN_HEIGHT * 0.5f - 95.0f, 380.0f, 210.0f };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"商品確認", rect.x + 28.0f, rect.y + 22.0f, rect.w - 56.0f, 34.0f, 28.0f, D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8(inst.GetDisplayName() + u8"  " + std::to_string(price) + u8"G", rect.x + 28.0f, rect.y + 64.0f, rect.w - 56.0f, 34.0f, 24.0f, D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8(u8"拾いますか？", rect.x + 28.0f, rect.y + 96.0f, rect.w - 56.0f, 34.0f, 24.0f, D2D1::ColorF(1.0f, 0.9f, 0.35f, 1.0f));
    DrawChoiceLine(u8"はい", m_SelectedChoice == 0, rect.x + 46.0f, rect.y + 130.0f, rect.w - 80.0f);
    DrawChoiceLine(u8"いいえ", m_SelectedChoice == 1, rect.x + 46.0f, rect.y + 162.0f, rect.w - 80.0f);
    UITextRenderer::End();
    RestoreMainRenderTarget();
}void ShopUI::DrawShopTradeUI(Player* player)
{
    if (!player)
    {
        Close();
        return;
    }

    const int unpaid = GetUnpaidShopTotal(player);
    const int sale = GetPendingShopSaleTotal(player);
    if (unpaid <= 0 && sale <= 0)
    {
        if (!m_ConfirmMessageShown)
        {
            MessageLog::Instance().AddMessage(u8"精算するものはない。");
            m_ConfirmMessageShown = true;
        }
        Close();
        return;
    }

    if (!m_ConfirmMessageShown)
    {
        if (unpaid > 0) MessageLog::Instance().AddMessage(u8"代金" + std::to_string(unpaid) + u8"Gを支払いますか？");
        if (sale > 0) MessageLog::Instance().AddMessage(u8"売却代金" + std::to_string(sale) + u8"Gで売りますか？");
        m_ConfirmMessageShown = true;
    }

    if (Input::GetKeyTrigger(VK_UP) || Input::GetKeyTrigger(VK_DOWN)) m_SelectedChoice = 1 - m_SelectedChoice;
    if (Input::GetKeyTrigger('X')) m_SelectedChoice = 1;

    if (Input::GetKeyTrigger('Z') || Input::GetKeyTrigger('X'))
    {
        if (m_SelectedChoice == 0)
        {
            if (PayForUnpaidShopItems(player))
            {
                FinalizePendingShopSales(player);
                Close();
                return;
            }
        }
        else
        {
            if (sale > 0)
            {
                ClearPendingShopSales();
                MessageLog::Instance().AddMessage(u8"売却をやめた。置いた道具は店内に残っている。");
            }

            if (m_ForcedCheckout && unpaid > 0)
            {
                StartTheftMode();
            }
            Close();
            return;
        }
    }

    UIRect rect{ SCREEN_WIDTH - 310.0f, SCREEN_HEIGHT - 315.0f, 250.0f, 145.0f };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(m_ForcedCheckout ? u8"店外精算" : u8"店番確認", rect.x + 24.0f, rect.y + 20.0f, rect.w - 48.0f, 34.0f, 26.0f, D2D1::ColorF(D2D1::ColorF::White));
    DrawHorizontalChoice(u8"はい", m_SelectedChoice == 0, rect.x + 44.0f, rect.y + 62.0f, rect.w - 80.0f);
    DrawHorizontalChoice(u8"いいえ", m_SelectedChoice == 1, rect.x + 44.0f, rect.y + 96.0f, rect.w - 80.0f);
    UITextRenderer::End();
    RestoreMainRenderTarget();
}
