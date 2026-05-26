// ItemTableIO.h
#pragma once
#include <unordered_map>
#include <string>
#include "ItemDatabase.h"

class ItemTableIO
{
public:
    static  bool LoadFromFile(
            const std::string& path,
            ItemSpawnTable& outTable);

    static
        bool SaveToFile(
            const std::string& path,
            const ItemSpawnTable& table);
};
