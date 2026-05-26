#pragma once
#include <string>
#include "EnemyData.h"

class EnemyTableIO
{
public:
    static bool LoadFromFile(
        const std::string& path,
        EnemySpawnTable& outTable);

    static bool SaveToFile(
        const std::string& path,
        const EnemySpawnTable& table);
};
