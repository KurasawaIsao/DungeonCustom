#pragma once
#include <string>
#include <vector>
#include "EffectBase.h"

enum class ItemType
{
    Herb,
    Food,
    Pot,
    Staff,
    Arrow,
    Stone,
    Weapon, 
    Shield
};

enum class ItemUseType
{
    Drink,
    Eat,
    Zap,
    Shoot,
    Put,
    Equip,
    Throw,
};

struct ItemSpawnEntry
{
    std::string id;   
    int weight;
};

struct ItemSpawnTable
{
    std::string tableId;
    std::vector<ItemSpawnEntry> entries;
};

struct ItemData
{
    ItemData(const std::string& Id,
        ItemType i,
        const std::string& n,
        ItemUseType t,
        bool stack,
        bool ident,
        bool option,
		int buyprice,
		int sellprice,
        EffectBase* eff)
        : id(Id),
        type(i),
        name(n),
        useType(t),
        stackable(stack),
        identifiable(ident),
        BlessOrCurse(option),
		price(buyprice),
		sellPrice(sellprice),
        effect(eff)
    {}
    //武器の場合
    ItemData(const std::string& Id,
        ItemType i,
        const std::string& n,
        ItemUseType t,
        bool stack,
        bool ident,
        bool option,
        int buyprice,
        int sellprice,
        int basevalue,
        EffectBase* eff)
        : id(Id),
        type(i),
        name(n),
        useType(t),
        stackable(stack),
        identifiable(ident),
        BlessOrCurse(option),
		price(buyprice),
		sellPrice(sellprice),
        baseValue(basevalue),
        effect(eff)
    {}

    std::string id;     
    ItemType type;
    std::string name;
    ItemUseType useType;

    bool stackable;
    bool identifiable;
    bool BlessOrCurse;
    bool canRename = true;

    EffectBase* effect;
    int baseValue = 0;
    // 店の売買価格。sellPriceはインスタンスの強化値や回数を見ず、ItemData単位で固定する。
    int price = 0;
    int sellPrice = 0;
    std::string itemTableId;
};


