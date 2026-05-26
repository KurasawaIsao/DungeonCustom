#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include "EnemyData.h"

class EnemyTableDatabase
{
public:
    static void Init();

    static const EnemySpawnTable* Get(const std::string& id);
    static bool Exists(const std::string& id);

    static void LoadAll(const std::string& dir);
    static std::vector<std::string> GetAllIds();

    static const std::unordered_map<
        std::string,
        EnemySpawnTable>& GetAll()
    {
        return tables;
    }

private:
    static std::unordered_map<
        std::string,
        EnemySpawnTable> tables;
};
