#include "GameRandom.h"
#include "ItemDatabase.h"
#include "HealEffect.h"
#include "FoodEffect.h"
#include "StatusEffect.h"
#include "PoisonEffect.h"
#include "CurePoisonEffect.h"
#include "RottenFoodEffect.h"
#include <cstdlib>
#include "ItemIdentificationManager.h"
#include "ItemTableIO.h"
#include "manager.h"
#include "ItemTableDataBase.h"
#include "WarpEffect.h"
#include "scene.h"
#include "DamageEffect.h"
#include "AttackDamageEffect.h"
#include "ChangeAIEffect.h"
#include "KnockbackEffect.h"
#include "SpeedChangeEffect.h"
#include "StoneEffect.h"
#include "JsonIO.h"
#include "MessageLog.h"
#include <algorithm>

std::unordered_map<std::string, ItemData> ItemDatabase::m_Items;
std::vector<std::string> ItemDatabase::m_ItemIDs;

namespace
{
    HealEffect g_Heal20(20, 1,false);
    HealEffect g_Heal50(50, 2,false);
    HealEffect g_Heal100(100, 4,false);
    FoodEffect g_Food50(50);
    FoodEffect g_Food100(100);
    RottenFoodEffect g_RottenFood;
    StatusEffect g_Confuse(Status::Confusion,5);
    StatusEffect g_Paralysis(Status::Paralysis, -1);
    StatusEffect g_Sleep(Status::Sleep, 3);
    PoisonEffect g_Poison(1, 50);
    CurePoisonEffect g_CurePoison;
    WarpEffect g_Warp;
    DamageEffect g_Damage(25);
    AttackDamageEffect g_ArrowDamage(1.0f, 0);
    StoneEffect g_StoneEffect(0.8f, 0);
    ChangeAIEffect g_ChangeRunAwayAI(EnemyAIType::RunAway, AllyAIMode::Retreat);
	KnockbackEffect g_Knockback(10);
    SpeedChangeEffect g_Haste(TurnSpeed::Fast, u8"は倍速になった。");
    SpeedChangeEffect g_Slow(TurnSpeed::Slow, u8"は鈍足になった。");

    void LogDataWarning(const std::string& message)
    {
        MessageLog::Instance().AddMessage("[Data] " + message);
    }
}

void ItemDatabase::SetShopPriceDefaults()
    {
        struct PriceDefault { const char* id; int buy; int sell; };
        const PriceDefault prices[] =
        {
            { "Herb_Heal20", 120, 60 }, { "Herb_Heal50", 200, 100 }, { "Herb_Heal100", 500, 250 },
            { "ParalysisHerb", 240, 120 }, { "SleepHerb", 240, 120 }, { "ConfuseHerb", 260, 130 }, { "WarpHerb", 360, 180 }, { "PoisonHerb", 300, 150 }, { "AntidoteHerb", 180, 90 },
            { "Food_Bread", 80, 40 }, { "Food_BigBread", 160, 80 }, { "Food_RottenBread", 10, 1 },
            { "Staff_Confuse", 340, 170 }, { "Staff_Sleep", 340, 170 }, { "Staff_Paralysis", 380, 190 },
            { "Staff_Warp", 440, 220 }, { "Staff_Damage", 480, 240 }, { "Staff_AIRunAway", 520, 260 },
            { "Staff_Haste", 650, 325 }, { "Staff_Slow", 650, 325 },
            { "Arrow", 40, 10 }, { "Stone", 20, 5 },
            { "Pot_Storage", 520, 260 },
            { "CopperSword", 800, 400 }, { "IronSword", 1300, 650 },
            { "Sh_LeatherShield", 640, 320 }, { "Sh_IronShield", 1120, 560 },
        };

        for (const auto& p : prices)
        {
            auto it = ItemDatabase::m_Items.find(p.id);
            if (it == ItemDatabase::m_Items.end()) continue;
            // JSONが壊れていても店価格が入るよう、C++側の初期値として最低限の価格表を持つ。
            it->second.price = p.buy;
            it->second.sellPrice = p.sell;
        }
    }

void ItemDatabase::Init()
{
    m_Items.clear();
    m_ItemIDs.clear();

    //セットするboolは順に
    // 
    // 複数個スタックできるか/未識別・識別対称か/呪い・祝福が適用されるか
    // 
    // ---- 草 ----
    m_Items.emplace(
        "Herb_Heal20",
        ItemData{
            "Herb_Heal20",
            ItemType::Herb,
            u8"薬草",
            ItemUseType::Drink,
            false, true, true,
			1000, 500,
            &g_Heal20
        }
    );
    m_ItemIDs.push_back("Herb_Heal20");

    m_Items.emplace(
        "Herb_Heal50",
        ItemData{
            "Herb_Heal50",
            ItemType::Herb,
            u8"弟切草",
            ItemUseType::Drink,
            false, true, false,
			5000, 2500,
            &g_Heal50
        }
    );
    m_ItemIDs.push_back("Herb_Heal50");

    m_Items.emplace(
        "Herb_Heal100",
        ItemData{
            "Herb_Heal100",
            ItemType::Herb,
            u8"神草",
            ItemUseType::Drink,
            false, true, true,
             7500,3000,
            &g_Heal100
        }
    );
    m_ItemIDs.push_back("Herb_Heal100");

    m_Items.emplace(
        "ParalysisHerb",
        ItemData{
            "ParalysisHerb",
            ItemType::Herb,
            u8"かなしばりのたね",
            ItemUseType::Drink,
            false, true, true,
            100,50,
            &g_Paralysis
        }
    );
    m_ItemIDs.push_back("ParalysisHerb");

    m_Items.emplace(
        "SleepHerb",
        ItemData{
            "SleepHerb",
            ItemType::Herb,
            u8"眠り草",
            ItemUseType::Drink,
            false, true, true,
            100,50,
            &g_Sleep
        }
    );
    m_ItemIDs.push_back("SleepHerb");

    m_Items.emplace(
        "ConfuseHerb",
        ItemData{
            "ConfuseHerb",
            ItemType::Herb,
            u8"混乱草",
            ItemUseType::Drink,
            false, true, true,
			150,100,
            &g_Confuse
        }
    );
    m_ItemIDs.push_back("ConfuseHerb");

    m_Items.emplace(
        "WarpHerb",
        ItemData{
            "WarpHerb",
            ItemType::Herb,
            u8"ワープ草",
            ItemUseType::Drink,
            false, true, true,
            100,50,
            &g_Warp
        }
    );
    m_ItemIDs.push_back("WarpHerb");

    m_Items.emplace(
        "PoisonHerb",
        ItemData{
            "PoisonHerb",
            ItemType::Herb,
            u8"毒草",
            ItemUseType::Drink,
            false, true, true,
            300,150,
            &g_Poison
        }
    );
    m_ItemIDs.push_back("PoisonHerb");

    m_Items.emplace(
        "AntidoteHerb",
        ItemData{
            "AntidoteHerb",
            ItemType::Herb,
            u8"毒消し草",
            ItemUseType::Drink,
            false, true, true,
            180,90,
            &g_CurePoison
        }
    );
    m_ItemIDs.push_back("AntidoteHerb");
    // ---- パン ----
    m_Items.emplace(
        "Food_Bread",
        ItemData{
            "Food_Bread",
            ItemType::Food,
            u8"パン",
            ItemUseType::Eat,
            false, false, false,
            100,50,
            &g_Food50
        }
    );
    m_ItemIDs.push_back("Food_Bread");

    m_Items.emplace(
        "Food_BigBread",
        ItemData{
            "Food_BigBread",
            ItemType::Food,
            u8"大きなパン",
            ItemUseType::Eat,
            false, false, false,
            200,100,
            &g_Food100
        }
    );
    m_ItemIDs.push_back("Food_BigBread");

    m_Items.emplace(
        "Food_RottenBread",
        ItemData{
            "Food_RottenBread",
            ItemType::Food,
            u8"くさったパン",
            ItemUseType::Eat,
            false, false, false,
            10,1,
            &g_RottenFood
        }
    );
    m_ItemIDs.push_back("Food_RottenBread");

    // ----　杖  ----
    m_Items.emplace(
        "Staff_Confuse",
        ItemData{
            "Staff_Confuse",
            ItemType::Staff,
            u8"混乱の杖",
            ItemUseType::Zap,
            false, true, true,
            800, 400,
            &g_Confuse
        }
    );
    m_ItemIDs.push_back("Staff_Confuse");

    m_Items.emplace(
        "Staff_Sleep",
        ItemData{
            "Staff_Sleep",
            ItemType::Staff,
            u8"睡眠の杖",
            ItemUseType::Zap,
            false, true, true,
			1200, 600,
            &g_Sleep
        }
    );
    m_ItemIDs.push_back("Staff_Sleep");

    m_Items.emplace(
        "Staff_Paralysis",
        ItemData{
            "Staff_Paralysis",
            ItemType::Staff,
            u8"かなしばりの杖",
            ItemUseType::Zap,
            false, true, true,
            1200,850,
            &g_Paralysis
        }
    );
    m_ItemIDs.push_back("Staff_Paralysis");

    m_Items.emplace(
        "Staff_Warp",
        ItemData{
            "Staff_Warp",
            ItemType::Staff,
            u8"ワープの杖",
            ItemUseType::Zap,
            false, true, true,
            1000,450,
            &g_Warp
        }
    );
    m_ItemIDs.push_back("Staff_Warp");

    m_Items.emplace(
        "Staff_Damage",
        ItemData{
            "Staff_Damage",
            ItemType::Staff,
            u8"いかづちの杖",
            ItemUseType::Zap,
            false, true, true,
			1200,600,
            &g_Damage
        }
    );
    m_ItemIDs.push_back("Staff_Damage");

    m_Items.emplace(
        "Staff_KnockBack",
        ItemData{
            "Staff_KnockBack",
            ItemType::Staff,
            u8"ふきとばしの杖",
            ItemUseType::Zap,
            false, true, true,
            1200,600,
            &g_Knockback
        }
    );
    m_ItemIDs.push_back("Staff_KnockBack");
    m_Items.emplace(
        "Staff_Haste",
        ItemData{
            "Staff_Haste",
            ItemType::Staff,
            u8"倍速の杖",
            ItemUseType::Zap,
            false, true, true,
            650,325,
            &g_Haste
        }
    );
    m_ItemIDs.push_back("Staff_Haste");

    m_Items.emplace(
        "Staff_Slow",
        ItemData{
            "Staff_Slow",
            ItemType::Staff,
            u8"鈍足の杖",
            ItemUseType::Zap,
            false, true, true,
            650,325,
            &g_Slow
        }
    );
    m_ItemIDs.push_back("Staff_Slow");

    m_Items.emplace(
        "Staff_AIRunAway",
        ItemData{
            "Staff_AIRunAway",
            ItemType::Staff,
            u8"おびえの杖",
            ItemUseType::Zap,
            false, true, true,
            520,260,
            &g_ChangeRunAwayAI
        }
    );
    m_ItemIDs.push_back("Staff_AIRunAway");

    // ---- 矢 ----
    m_Items.emplace(
        "Arrow",
        ItemData{
            "Arrow",
            ItemType::Arrow,
            u8"木の矢",
            ItemUseType::Shoot,
            true, false, false,
            40,10,
            &g_ArrowDamage
        }
    );
    m_ItemIDs.push_back("Arrow");

    // ---- 石 ----
    m_Items.emplace(
        "Stone",
        ItemData{
            "Stone",
            ItemType::Stone,
            u8"石",
            ItemUseType::Throw,
            true, false, false,
            20,5,
            &g_StoneEffect
        }
    );
    m_ItemIDs.push_back("Stone");
    
    // ---- 壺 ----
    m_Items.emplace(
        "Pot_Storage",
        ItemData{
            "Pot_Storage",
            ItemType::Pot,
            u8"保存の壺",
            ItemUseType::Put,
            false, true, false,
            1500,750,
            nullptr
        }
    );
    m_ItemIDs.push_back("Pot_Storage");

    // ----- 武器 ------
    m_Items.emplace(
        "CopperSword",
        ItemData{
            "CopperSword",
            ItemType::Weapon,
            u8"銅の剣",
            ItemUseType::Equip,
            false, true, true,
            750,300,
            5, // 基本攻撃力
            nullptr
        }
    );
    m_ItemIDs.push_back("CopperSword");

    m_Items.emplace(
        "IronSword",
        ItemData{
            "IronSword",
            ItemType::Weapon,
            u8"鋼鉄の剣",
            ItemUseType::Equip,
            false, true, true,
			2000,1000,
            8, // 基本攻撃力
            nullptr
        }
    );
    m_ItemIDs.push_back("IronSword");

    // ----盾 ----
    m_Items.emplace(
        "Sh_LeatherShield",
        ItemData{
            "Sh_LeatherShield",
            ItemType::Shield,
            u8"皮の盾",
            ItemUseType::Equip,
            false, true, true,
            550,250,
            3, 
            nullptr
        }
    );
    m_ItemIDs.push_back("Sh_LeatherShield");

    m_Items.emplace(
        "Sh_IronShield",
        ItemData{
            "Sh_IronShield",
            ItemType::Shield,
            u8"鉄の盾",
            ItemUseType::Equip,
            false, true, true,
			1200,600,
            6, 
            nullptr
        }
    );
    m_ItemIDs.push_back("Sh_IronShield");

    //SetShopPriceDefaults();

    // ---- SpawnTable読み込み ----
    ItemTableDatabase::LoadAll("DungeonData\\ItemTables\\");
}
// 全アイテムデータを取得
std::vector<ItemData> ItemDatabase::GetAllData() {
    std::vector<ItemData> list;
    for (auto& pair : m_Items) {
        list.push_back(pair.second);
    }
    return list;
}

const ItemData* ItemDatabase::Get(const std::string& id)
{
    auto it = m_Items.find(id);
    if (it == m_Items.end()) return nullptr;
    return &it->second;
}


const ItemData* ItemDatabase::GetRandom()
{
    if (m_ItemIDs.empty()) return nullptr;
    const std::string& id = m_ItemIDs[GameRandom::Index(m_ItemIDs.size())];
    return Get(id);
}




const ItemData* ItemDatabase::DrawFromTable(const std::string& tableId)
{
    // 1. ItemTableDatabaseから指定されたIDのテーブルを取得
    const ItemSpawnTable* table = ItemTableDatabase::Get(tableId);

    if (!table || table->entries.empty())
    {
        return nullptr;
    }

    // 2. 重みの総和を計算
    int totalWeight = 0;
    for (const auto& entry : table->entries)
    {
        totalWeight += std::max(0, entry.weight); 
    }

    if (totalWeight <= 0) return nullptr;

    // 3. 乱数生成と累積判定
    int roll = GameRandom::Range(0, totalWeight - 1);
    int accumulatedWeight = 0;

    for (const auto& entry : table->entries)
    {
        accumulatedWeight += std::max(0, entry.weight);
        if (roll < accumulatedWeight)
        {
            // 4. 当選したIDのアイテムデータをItemDatabaseから取得
            const ItemData* data = Get(entry.id);
            if (!data)
            {
                // アイテムIDがItemDatabaseに登録されていない場合
                return nullptr;
            }
            return data;
        }
    }
    return nullptr;
}
