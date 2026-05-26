#pragma once
#include "Vector2Int.h"
#include <vector>

class MapData;
class Player;
class Unit;
class UnitAI;

namespace UnitMovementPlanner
{
    Vector2Int NormalizeDir(const Vector2Int& dir);
    Vector2Int CardinalDir(const Vector2Int& dir);
    int DistanceToGoals(const Vector2Int& pos, const std::vector<Vector2Int>& goals);

    std::vector<Vector2Int> BuildPlayerLinePositions(Player* player, int rearCount, int frontCount);

    bool CanStandAt(Unit& self, const Vector2Int& pos, MapData* map);
    bool TryStartMove(Unit& self, const Vector2Int& next, MapData* map);

    bool TryMoveToAdjacentTarget(Unit& self, Unit* target, MapData* map, UnitAI& pathAI);
    bool MoveAdjacentAndEndTurn(Unit& self, Unit* target, MapData* map, UnitAI& pathAI);
    bool MoveToPlayerLineAndEndTurn(
        Unit& self,
        Player* player,
        MapData* map,
        UnitAI& pathAI,
        int rearCount,
        int frontCount,
        bool fallbackAdjacentWhenNoGoals);
    bool MoveToTargetByAreaAndEndTurn(
        Unit& self,
        Unit* target,
        MapData* map,
        UnitAI& pathAI,
        int rearCount,
        int frontCount,
        bool fallbackAdjacentWhenNoGoals);
}
