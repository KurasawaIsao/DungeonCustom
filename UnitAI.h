#pragma once
#include <vector>
#include "Vector2Int.h"
class Unit;
class MapData;
// UnitAI は Unit に対する行動方針の共通インターフェース。
// 経路探索・視線判定・攻撃隣接判定・スキル実行など、敵味方どちらからも使える低レベル判断を集める。
// 勢力ごとの敵味方判定や死亡報酬は UnitAI に入れず、呼び出し側または専用ポリシーへ分離する。
class UnitAI
{
public:
    virtual ~UnitAI() = default;

    // 1ターン分の行動判断を行う。派生AIはここで移動・攻撃・待機のどれかを選ぶ。
    virtual void Update(Unit& self, MapData* map) = 0;

    // AIを切り替えた瞬間に必要な初期化がある場合だけ派生先で使う。
    virtual void OnEnter(Unit& self, MapData* map) {}

    // AIを抜ける瞬間に経路キャッシュなどを片付けたい場合だけ派生先で使う。
    virtual void OnExit(Unit& self, MapData* map) {}

   
    // 視界・隣接・経路探索など、複数AIで使う共通判定。
    static bool CanSee(Unit& self, Unit* target, MapData* map, int visionRange);
    static bool HasLineOfSight(Vector2Int from, Vector2Int to, MapData* map);

    bool IsAdjacent(Unit& self, Unit* target);
    bool IsAttackAdjacent(Unit& self, Unit* target, MapData* map = nullptr);
    bool ExecuteSkill(Unit& self, Unit* target);
    bool IsAdjacentToPlayer(Unit& self);
    static void ExecuteConfusion(Unit& self, MapData* map);
    std::vector<Vector2Int> BFSPath(Unit& self, const Vector2Int& start, const Vector2Int& goal, MapData* map);
};
