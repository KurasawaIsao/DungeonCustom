#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include "ItemData.h"

class ItemDatabase
{
public:
    static void Init();

    static const ItemData* Get(const std::string& id);

    static const ItemData* GetRandom();
    static const ItemData* DrawFromTable(const std::string& tableId);

    static std::vector<ItemData> GetAllData();

private:
    static void SetShopPriceDefaults();

    static std::unordered_map<std::string, ItemData> m_Items;
    static std::vector<std::string> m_ItemIDs;
};
