#include "GameRandom.h"
#include "BasicPatrolAI.h"
#include "MapData.h"
#include "Unit.h"
#include "UnitManager.h"
void BasicPatrolAI::Update(Unit& self, MapData* map)
{
    if (!map) {
        self.EndTurn();
        return;
    }
    self.RepairInvalidGridPos("BasicPatrolAI::Update");
 

    // 1. 経路が空なら新しく生成
    if (patrolRoute.empty())
    {
        // 経路生成の「直前」の座標を保存しておく
        lastPos = self.GetGridPos();
        UpdateCurrentRoom(self, map);
        patrolRoute = GeneratePathToNextJunction(self, map);
    }

    // 2. 経路があるなら一歩進む
    if (!patrolRoute.empty())
    {
        Vector2Int next = patrolRoute.front();
        Vector2Int currentPos = self.GetGridPos();
        Vector2Int step = next - currentPos;
        if (!map->IsInBounds(next) || !map->IsWalkable(next) || abs(step.x) > 1 || abs(step.y) > 1) {
            patrolRoute.clear();
            self.EndTurn();
            return;
        }

        // 他ユニットによるスタック判定
        if (UnitManager::Instance()->GetUnitAt(next) && UnitManager::Instance()->GetUnitAt(next) != &self)
        {
            moveFailureCount++;
            if (moveFailureCount > MAX_FAILURE_RETRY)
            {
                // 入り口付近で詰まったら、その入り口を「既に使用済み」と見なして引き返す
                if (map->IsEntranceTile(currentPos.x, currentPos.y) || map->IsEntranceTile(next.x, next.y))
                {
                    lastEntrancePos = next;
                }
                patrolRoute.clear();
            }
            else {
                self.EndTurn(); // 譲り合い待機
                return;
            }
        }
        else if (self.RequestMoveBool(next)) {
            patrolRoute.erase(patrolRoute.begin());
            lastPos = currentPos;
            moveFailureCount = 0;
        }
        else {
            patrolRoute.clear();
        }
    }
    else {
        RandomMove(self, map);
    }

    self.EndTurn();
}

void BasicPatrolAI::UpdateCurrentRoom(Unit& self, MapData* map)
{
    Room* room = map->GetRoomAt(self.GetGridPos());

    // 通路に出たら部屋IDをリセット（振り返り防止の準備）
    if (!room) {
        currentRoomId = -1;
        return;
    }

    int id = map->GetRoomIndex(room);
    if (id < 0) return;

    // 新しい部屋に入った
    if (id != currentRoomId) {
        currentRoomId = id;
    }

    // 入り口マスを踏んでいる間、その位置を記憶（次の行き先から除外するため）
    if (map->IsEntranceTile(self.GetGridPos().x, self.GetGridPos().y)) {
        lastEntrancePos = self.GetGridPos();
    }
}

std::vector<Vector2Int> BasicPatrolAI::GeneratePathToNextJunction(Unit& self, MapData* map)
{
    Vector2Int start = self.GetGridPos();
    Room* currentRoom = map->GetRoomAt(start);

    // --- A. 部屋の中にいる場合：出口を選択 ---
    if (currentRoom)
    {
        std::vector<Vector2Int> candidates;
        for (const auto& ent : currentRoom->entrances) {
            if (ent != lastEntrancePos) candidates.push_back(ent);
        }

        // 行き止まり部屋なら唯一の出口（今来た道）に戻る
        if (candidates.empty() && (!map->IsInBounds(lastEntrancePos) || !map->IsWalkable(lastEntrancePos))) {
            return {};
        }
        Vector2Int targetEntrance = candidates.empty() ? lastEntrancePos : candidates[GameRandom::Index(candidates.size())];

        if (!map->IsInBounds(targetEntrance) || !map->IsWalkable(targetEntrance)) {
            return {};
        }

        // 出口を決めたら、通路に出た直後に振り返らないよう即座に記憶
        lastEntrancePos = targetEntrance;

        // BFSで部屋内のパスを作成
        std::vector<Vector2Int> path = BFSPath(self, start, targetEntrance, map);

        // 出口の「その先」まで一気に経路に含めて通路へ押し出す
        Vector2Int corridorStep = FindAdjacentCorridor(targetEntrance, map);
        if (corridorStep != Vector2Int(-1, -1)) path.push_back(corridorStep);

        return path;
    }

    // --- B. 通路にいる場合：次の分岐点か部屋を目指す ---
    return GenerateCorridorPath(self, map);
}

std::vector<Vector2Int> BasicPatrolAI::GenerateCorridorPath(Unit& self, MapData* map)
{
    Vector2Int start = self.GetGridPos();
    static const Vector2Int dirs[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

    //  隣接する新しい部屋への優先進入
    for (auto& d : dirs) {
        Vector2Int next = start + d;
        if (map->IsWalkable(next) && map->IsEntranceTile(next.x, next.y)) {
            if (next != lastEntrancePos) return { next };
        }
    }

    // 通路移動の候補選定
    std::vector<Vector2Int> options;
    for (auto& d : dirs) {
        Vector2Int next = start + d;

        // 通行不可、また直前にいた場所は除外
        if (!map->IsWalkable(next) || next == lastPos) continue;

        // 部屋の入り口（さっき出たばかりの入り口）も戻らない
        if (next == lastEntrancePos) continue;

        if (map->GetTile(next.x, next.y) == TileType::Corridor) {
            options.push_back(next);
        }
    }

    // 候補がない場合（行き止まり）のみ、lastPos 解禁
    if (options.empty()) {
        for (auto& d : dirs) {
            Vector2Int next = start + d;
            if (map->IsWalkable(next)) options.push_back(next);
        }
    }

    if (options.empty()) return {};

    // 候補からランダムに決定（これで交差点で来た道以外を選ぶ）
    Vector2Int currentPos = options[GameRandom::Index(options.size())];
    std::vector<Vector2Int> path = { currentPos };

    //  通路を直進（分岐点か新しい部屋の入り口に隣接するまで）
    for (int safety = 0; safety < 100; ++safety) {
        if (IsDecisionPoint(currentPos, map)) break;

        Vector2Int nextStep = { -1, -1 };
        int forwardOptions = 0;
        for (auto& d : dirs) {
            Vector2Int neighbor = currentPos + d;
            if (!map->IsWalkable(neighbor) || neighbor == start) continue;
            if (path.size() >= 2 && neighbor == path[path.size() - 2]) continue;

            nextStep = neighbor;
            forwardOptions++;
        }

        if (forwardOptions == 1) {
            currentPos = nextStep;
            path.push_back(currentPos);
        }
        else break;
    }
    return path;
}

bool BasicPatrolAI::IsDecisionPoint(const Vector2Int& pos, MapData* map)
{
    if (map->GetRoomAt(pos) != nullptr) return false;
    static const Vector2Int dirs[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

    // 隣接マスに「新しい部屋」があるか？
    for (auto& d : dirs) {
        Vector2Int neighbor = pos + d;
        if (map->IsEntranceTile(neighbor.x, neighbor.y) && neighbor != lastEntrancePos) return true;
    }

    // 交差点（3方以上が道）か行き止まりか？
    int walkableCount = 0;
    for (auto& d : dirs) if (map->IsWalkable(pos + d)) walkableCount++;

    return (walkableCount >= 3 || walkableCount == 1);
}

void BasicPatrolAI::RandomMove(Unit& self, MapData* map)
{
    static const Vector2Int dirs[8] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };
    std::vector<Vector2Int> candidates;
    for (auto& d : dirs) {
        Vector2Int next = self.GetGridPos() + d;
        if (map->IsInBounds(next) && map->IsWalkable(next) && !UnitManager::Instance()->GetUnitAt(next)) candidates.push_back(next);
    }
    if (!candidates.empty()) self.RequestMove(candidates[GameRandom::Index(candidates.size())]);
}

void BasicPatrolAI::ResetFromCurrentPos(Unit& self, MapData* map)
{
    patrolRoute.clear();
    lastPos = self.GetGridPos();
    lastEntrancePos = { -1, -1 };
    moveFailureCount = 0;
    currentRoomId = map ? map->GetRoomIndex(map->GetRoomAt(self.GetGridPos())) : -1;
}

Vector2Int BasicPatrolAI::FindAdjacentCorridor(const Vector2Int& entrancePos, MapData* map)
{
    static const Vector2Int dirs[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    for (auto& d : dirs) {
        Vector2Int next = entrancePos + d;
        if (map->IsInBounds(next) && map->GetTile(next.x, next.y) == TileType::Corridor && map->GetRoomAt(next) == nullptr) return next;
    }
    return { -1, -1 };
}