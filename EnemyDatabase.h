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
    static std::vector<std::string> GetAllIds();

private:
    // EnemyData自身のIDをキーとして登録し、キーの二重指定を防ぐ。
    static void Register(EnemyData data);

    static std::unordered_map<std::string,EnemyData> m_Data;
};
