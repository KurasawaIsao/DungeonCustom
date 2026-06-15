#pragma once
#include <vector>
#include"Unit.h"
#include "player.h"
#include "Enemy.h"
class Unit;
class Player;
class Enemy;
class Ally;
class UnitManager
{
private:
    static UnitManager* m_Instance;

    Player* m_Player = nullptr;
    std::vector<Enemy*> m_Enemies;
    std::vector<Ally*> m_Allies;

    UnitManager() {}

public:
    static UnitManager* Instance()
    {
        if (!m_Instance) m_Instance = new UnitManager();
        return m_Instance;
    }


    std::vector<Enemy*> GetAdjacentEnemies(Unit& self) const;
    Unit* GetNearestHostileToEnemy(const Vector2Int& pos) const;


    void ClearAllEnemies();
    void ClearAllAllies();
    void ClearSceneReferences();
   
    void SetPlayer(Player* p) { m_Player = p; }
    Player* GetPlayer() const { return m_Player; }

    void RegisterEnemy(Enemy* enemy);

    const std::vector<Enemy*>& GetEnemies() const
    {
        return m_Enemies;
    }

    void RegisterAlly(Ally* a);
    const std::vector<Ally*>& GetAllies() const 
    { 
        return m_Allies; 
    }
    int GetEnemyCount() const { return static_cast<int>(m_Enemies.size()); }
    Unit* GetUnitAt(const Vector2Int& pos) const;

    void RemoveEnemy(Enemy* enemy);
    void RemoveAlly(Ally* ally);

};
