#include "PotUI.h"
#include "player.h"
#include "MessageLog.h"
#include "ItemInstance.h"

void PotUI::BulkPutInto(Player* player, int potIndex, std::set<int>& selectedIndices) {
    auto& items = player->GetItems();
    if (potIndex < 0 || potIndex >= (int)items.size()) return;

    PotData* pot = items[potIndex].instance.GetPot();
    if (!pot) return;

    bool anyPut = false;
    // インデックス崩れを防ぐため、後ろから処理
    for (auto it = selectedIndices.rbegin(); it != selectedIndices.rend(); ++it) {
        int idx = *it;

        if (items[idx].instance.IsPot()) {
            MessageLog::Instance().AddMessage(u8"壺の中に壺を入れることはできない！");
            continue;
        }

        if (pot->items.size() < (size_t)pot->capacity) {
            items[idx].instance.SetStackCount(items[idx].count);
            pot->items.push_back(std::move(items[idx].instance));
            items.erase(items.begin() + idx);
            // 削除によって操作対象の壺の場所が変わるのを防ぐ
            if (idx < potIndex) potIndex--;
            anyPut = true;
        }
        else {
            MessageLog::Instance().AddMessage(u8"壺がいっぱいだ！");
            break;
        }
    }

    if (anyPut) {
        player->EndTurn();
    }
}

void PotUI::BulkTakeOut(Player* player, int potIndex, std::set<int>& selectedPotIndices) {
    auto& inventory = player->GetItems();
    PotData* pot = inventory[potIndex].instance.GetPot();
    if (!pot) return;

    for (auto it = selectedPotIndices.rbegin(); it != selectedPotIndices.rend(); ++it) {
        if (inventory.size() < 20) { // 最大所持数制限
            inventory.push_back(InventoryItem(std::move(pot->items[*it])));
            pot->items.erase(pot->items.begin() + *it);
        }
        else {
            MessageLog::Instance().AddMessage(u8"持ち物がいっぱいで取り出せない！");
            break;
        }
    }
}
bool PotUI::TryPut(Player* player, int potIndex, int itemIndex) {
    auto& items = player->GetItems();

    // 1. 基本チェック
    if (potIndex < 0 || potIndex >= (int)items.size()) return false;
    if (itemIndex < 0 || itemIndex >= (int)items.size()) return false;

    PotData* pot = items[potIndex].instance.GetPot();
    if (!pot) return false;

    // 2. 制限チェック
    if (items[itemIndex].instance.IsPot()) {
        MessageLog::Instance().AddMessage(u8"壺の中に壺を入れることはできない！");
        return false;
    }

    if (itemIndex == potIndex) {
        MessageLog::Instance().AddMessage(u8"使用中の壺を入れることはできない！");
        return false;
    }

    if (pot->items.size() >= (size_t)pot->capacity) {
        MessageLog::Instance().AddMessage(u8"壺はいっぱいだ！");
        return false;
    }

    // 3. 移動処理
    items[itemIndex].instance.SetStackCount(items[itemIndex].count);
    MessageLog::Instance().AddMessage(items[itemIndex].instance.GetDisplayName() + u8"を壺に入れた。");

    // アイテムを移動
    pot->items.push_back(std::move(items[itemIndex].instance));
    items.erase(items.begin() + itemIndex);

    // 4. 後処理（ターン終了）
    player->EndTurn();
    return true;
}