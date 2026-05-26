#pragma once
#include <string>
#include <vector>

struct TrapSpawnEntry {
    std::string id;
    int weight;
};

struct TrapSpawnTable {
    std::string tableId;
    std::vector<TrapSpawnEntry> entries;
};

struct TrapData {
    std::string id;
    std::string name;
    std::string modelPath;
    class EffectBase* effect;
    bool singleUse;
    int breakChancePercent = 0;
};
