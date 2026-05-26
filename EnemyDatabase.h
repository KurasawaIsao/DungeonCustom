#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include "EnemyData.h"

class EnemyDatabase
{
public:
    static void Init();

    static const EnemyData* Get(
        const std::string& id);

    static const EnemyData* DrawFromTable(const std::string& tableId);

    static const std::unordered_map<std::string,EnemyData>& GetAll()
    {
        return m_Data;
    }

private:

    static std::unordered_map<std::string,EnemyData> m_Data;
};
