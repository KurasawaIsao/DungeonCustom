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
    static UnitManager* instance;

    Player* player = nullptr;
    std::vector<Enemy*> enemies;
    std::vector<Ally*> allies;

    UnitManager() {}

public:
    static UnitManager* Instance()
    {
        if (!instance) instance = new UnitManager();
        return instance;
    }


    std::vector<Enemy*> GetAdjacentEnemies(Unit& self) const;
    Unit* GetNearestHostileToEnemy(const Vector2Int& pos) const;


    void ClearAllEnemies();
    void ClearAllAllies();
    void ClearSceneReferences();
   
    void SetPlayer(Player* p) { player = p; }
    Player* GetPlayer() const { return player; }

    void RegisterEnemy(Enemy* enemy);

    const std::vector<Enemy*>& GetEnemies() const
    {
        return enemies;
    }

    void RegisterAlly(Ally* a);
    const std::vector<Ally*>& GetAllies() const 
    { 
        return allies; 
    }
    int GetEnemyCount() const { return static_cast<int>(enemies.size()); }
 

    bool HasEnemy(const Vector2Int& pos) const;
    Unit* GetUnitAt(const Vector2Int& pos) const;

    void RemoveEnemy(Enemy* enemy);
    void RemoveAlly(Ally* ally);

};
