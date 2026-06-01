#pragma once
#include <unordered_map>
#include <vector>
#include "TrapData.h"

class TrapDatabase {
public:
    static void Init();
    static const TrapData* GetRandom();
    static const TrapData* DrawFromTable(const std::string& tableId);
    static const TrapData* Get(const std::string& id);
    static const std::unordered_map<std::string, TrapData>& GetAll() { return m_Traps; }
    static const std::vector<std::string>& GetAllIds() { return m_TrapIDs; }

private:
    static std::unordered_map<std::string, TrapData> m_Traps;
    static std::vector<std::string> m_TrapIDs;
};
