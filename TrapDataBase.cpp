#include "GameRandom.h"
#include "TrapDatabase.h"
#include "TrapTableDatabase.h"
#include "StatusEffect.h"
#include "WarpEffect.h"
#include "KnockbackEffect.h"
#include "SummonEffect.h"
#include "DigEffect.h"
#include "FlyingObjectEffect.h"
#include "MudTrapEffect.h"
#include <algorithm>

namespace
{
    StatusEffect g_Confuse(Status::Confusion, 5);
    StatusEffect g_Sleep(Status::Sleep, 3);
    WarpEffect g_Warp;
    KnockbackEffect g_Knockback(10);
    SummonEffect g_Summon(3);
    FlyingObjectEffect g_FrontLeftProjectile(10);
    MudTrapEffect g_Mud;
}

std::unordered_map<std::string, TrapData> TrapDatabase::m_Traps;
std::vector<std::string> TrapDatabase::m_TrapIDs;

void TrapDatabase::Init() {
    m_Traps.clear();
    m_TrapIDs.clear();

    auto Add = [](TrapData data) {
        m_Traps[data.id] = data;
        m_TrapIDs.push_back(data.id);
        };
    //順に
	//タグ名、表示名、モデルパス、効果、一回使うと壊れるか
    Add({ "Trap_Sleep", u8"睡眠の罠", "Asset\\Model\\Traps\\SleepTrap.obj", &g_Sleep, false });
    Add({ "Trap_Confuse", u8"混乱の罠", "Asset\\Model\\Traps\\ConfuseTrap.obj", &g_Confuse, false });
    Add({ "Trap_Warp", u8"ワープの罠", "Asset\\Model\\Traps\\WarpTrap.obj", &g_Warp, false });
    Add({ "Trap_Mud", u8"泥の罠", "Asset\\Model\\Traps\\MudTrap.obj", &g_Mud, false, 30 });    //30%で壊れる設定
    Add({ "Trap_Knockback", u8"ふきとばしの罠", "Asset\\Model\\Traps\\IronballTrap.obj", &g_Knockback, false });
    Add({ "Trap_Summon", u8"召喚の罠", "Asset\\Model\\Traps\\SummonTrap.obj", &g_Summon, true });
    Add({ "Trap_Arrow", u8"木の矢の罠", "Asset\\Model\\Traps\\ArrowTrap.obj", &g_FrontLeftProjectile, false });
    TrapTableDatabase::LoadAll("DungeonData\\TrapTables\\");
}

const TrapData* TrapDatabase::GetRandom() {
    if (m_TrapIDs.empty()) return nullptr;
    return Get(m_TrapIDs[GameRandom::Index(m_TrapIDs.size())]);
}

const TrapData* TrapDatabase::DrawFromTable(const std::string& tableId) {
    const TrapSpawnTable* table = TrapTableDatabase::Get(tableId);
    if (!table || table->entries.empty()) return nullptr;

    int totalWeight = 0;
    for (const auto& entry : table->entries) {
        totalWeight += (std::max)(0, entry.weight);
    }

    if (totalWeight <= 0) return nullptr;

    int roll = GameRandom::Range(0, totalWeight - 1);
    for (const auto& entry : table->entries) {
        const int weight = (std::max)(0, entry.weight);
        if (roll < weight) {
            return TrapDatabase::Get(entry.id);
        }
        roll -= weight;
    }

    return nullptr;
}

const TrapData* TrapDatabase::Get(const std::string& id) {
    auto it = m_Traps.find(id);
    return (it != m_Traps.end()) ? &it->second : nullptr;
}
