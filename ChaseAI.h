#pragma once
#include "UnitAI.h"
class MapData;
class Player;

// ChaseAI は指定した相手の隣接マスまで近づき、隣接していれば攻撃するための基本追跡AI。
// 敵ならプレイヤー/味方、味方ならプレイヤーを標準ターゲットにするが、UpdateWithTargetで任意の相手にも使える。
// 無差別攻撃のような「誰を狙うか」が特殊なAIでも、接近移動部分はこのAIを流用できる。
class ChaseAI : public UnitAI
{
public:
    void Reset()
    {
        // ターゲットや自分の位置が変わったとき、古い経路を使い続けないようにする。
        path.clear();
        m_LastTargetPos = { -999, -999 };
        m_LastSelfPos = { -999, -999 };
    }

    void Update(Unit& self, MapData* map) override;
    void UpdateWithTarget(Unit& self, Unit* target, MapData* map);
    // 攻撃はせず、ターゲットに近づく移動だけを行う。味方の追従や移動フェーズ分離で使う。
    void MoveOnlyWithTarget(Unit& self, Unit* target, MapData* map);

private:
    // 直前に計算した経路を保持し、ターゲット位置が変わらない間は再計算を抑える。
    std::vector<Vector2Int> path;

    Vector2Int m_LastTargetPos = { -999, -999 };
    Vector2Int m_LastSelfPos = { -999, -999 };

    // ターゲット周囲の「到達したいマス」候補を作る。プレイヤー相手の場合は前後位置を少し優先する。
    std::vector<Vector2Int> BuildAdjacentGoals(Unit& self, Unit* target, MapData* map) const;
    std::vector<Vector2Int> BuildPlayerFollowGoals(Player* player, bool allowExtendedLine = false) const;
    std::vector<Vector2Int> BFSPathToAny(Unit& self, const Vector2Int& start, const std::vector<Vector2Int>& goals, MapData* map) const;
    bool CanStepOn(Unit& self, const Vector2Int& pos, MapData* map) const;
    bool TryStartMove(Unit& self, const Vector2Int& next, MapData* map);
    void AttackTarget(Unit& self, Unit& target);
};
