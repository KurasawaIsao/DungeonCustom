#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "TrapData.h"

class TrapTableDatabase
{
public:
    static void Init();
    static const TrapSpawnTable* Get(const std::string& id);
    static bool Exists(const std::string& id);
    static void LoadAll(const std::string& dir);
    static std::vector<std::string> GetAllIds();

    static const std::unordered_map<std::string, TrapSpawnTable>& GetAll()
    {
        return tables;
    }

private:
    static std::unordered_map<std::string, TrapSpawnTable> tables;
};
