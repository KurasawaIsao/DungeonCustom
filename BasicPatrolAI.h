#pragma once
#include "UnitAI.h"
#include <vector>

class MapData;

// BasicPatrolAI は部屋と通路を巡回するための基本AI。
// 部屋内では出口を選び、通路では分岐点や次の部屋まで進むことで、同じ場所を往復しにくくする。
class BasicPatrolAI : public UnitAI
{
public:
    void Update(Unit& self, MapData* map) override;
    // AI切り替え後に、現在位置を基準として巡回状態を作り直す。
    void ResetFromCurrentPos(Unit& self, MapData* map);

private:
    // 内部ロジック：部屋判定と移動
    void UpdateCurrentRoom(Unit& self, MapData* map);
    void RandomMove(Unit& self, MapData* map);

    // 経路生成
    std::vector<Vector2Int> GeneratePathToNextJunction(Unit& self, MapData* map);
    std::vector<Vector2Int> GenerateCorridorPath(Unit& self, MapData* map);

    // 補助判定
    Vector2Int FindAdjacentCorridor(const Vector2Int& entrancePos, MapData* map);
    bool IsDecisionPoint(const Vector2Int& pos, MapData* map);

private:
    // 状態管理
    int currentRoomId = -1;
    Vector2Int lastPos = { -1, -1 };          // 1歩前の物理座標（逆流防止用）
    Vector2Int lastEntrancePos = { -1, -1 };  // 最後に利用した/詰まった入り口座標

    // スタック対策
    int moveFailureCount = 0;
    const int MAX_FAILURE_RETRY = 2;

    // 現在の予定ルート
    std::vector<Vector2Int> patrolRoute;
};