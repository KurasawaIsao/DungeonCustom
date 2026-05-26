#pragma once
#include <vector>
#include <set>

class Player;
struct PotData;

class PotUI {
public:
    // 複数のアイテムを一括で壺に入れる
    static void BulkPutInto(Player* player, int potIndex, std::set<int>& selectedIndices);

    // 複数のアイテムを一括で壺から出す
    static void BulkTakeOut(Player* player, int potIndex, std::set<int>& selectedPotIndices);

    // 単一のアイテムを壺に入れる (検証ロジック付き)
    static bool TryPut(Player* player, int potIndex, int itemIndex);
};