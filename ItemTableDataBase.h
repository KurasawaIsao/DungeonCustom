#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include "ItemData.h"
class ItemTableDatabase
{
public:
    static void Init();

    static const ItemSpawnTable* Get(const std::string& id);
    static bool Exists(const std::string& id);

    static void LoadAll(const std::string& dir);
    static std::vector<std::string> GetAllIds();
    static const std::unordered_map<std::string, ItemSpawnTable>& GetAll()
    {
        return tables;
    }


private:
    static std::unordered_map<std::string, ItemSpawnTable> tables;
};