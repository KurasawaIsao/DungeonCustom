#pragma once
#include "UnitAI.h"
#include "ChaseAI.h"
#include "BasicPatrolAI.h"
#include <algorithm>

// BerserkAI は勢力を無視して、視界内のユニットを近い順に狙う無差別攻撃AI。
// 「狙う相手の選び方」は専用だが、「相手へ近づく」部分は ChaseAI と共通化できる余地がある。
class BerserkAI : public UnitAI
{
public:
    // 視界距離は敵データ側の visionRange から渡される。最低1マスは見るように補正する。
    void SetVisionRange(int range) { m_VisionRange = (std::max)(1, range); }

    void Update(Unit& self, MapData* map) override;
    // 行動フェーズ側から、BerserkAI が今狙う相手を確認するために使う。
    Unit* FindTarget(Unit& self, MapData* map) const;

private:
    // プレイヤー・味方・敵をすべて候補に入れ、自分自身と死亡済みユニットだけ除外する。
    std::vector<Unit*> FindVisibleTargets(Unit& self, MapData* map) const;
    bool TryMoveTowardTarget(Unit& self, Unit* target, MapData* map);
    int GetDistance(const Vector2Int& a, const Vector2Int& b) const;

    int m_VisionRange = 8;
    // 現状の移動は複数ターゲットを順に試すため専用処理だが、単一ターゲット追跡なら ChaseAI を流用できる。
    ChaseAI m_ChaseAI;
    // 視界内に誰もいない時は通常巡回に戻す。
    BasicPatrolAI m_PatrolAI;
};
