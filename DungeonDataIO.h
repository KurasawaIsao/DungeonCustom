#pragma once
#include "DungeonData.h"
class DungeonDataIO
{
public:
    static bool LoadFromFile(
        const std::string& path,
        DungeonData& outData
    );

    static bool SaveToFile(
        const std::string& path,
        const DungeonData& data
    );
};
