#include "GameRandom.h"
#include "UnitAI.h"
#include "SkillData.h"
#include "Unit.h"
#include "UnitManager.h"
#include "MapManager.h"
#include "MapData.h"
#include <queue>
#include "Ally.h"
#include "EffectTargeting.h"
#include "MessageLog.h"
namespace
{
    int CountUnitsInUserRoom(Unit& self)
    {
        MapData* map = MapManager::Instance() ? MapManager::Instance()->GetCurrentMap() : nullptr;
        UnitManager* units = UnitManager::Instance();
        if (!map || !units) return 1;

        Room* room = map->GetRoomAt(self.GetGridPos());
        if (!room) return 1;

        int count = 0;
        auto countIfInRoom = [&](Unit* unit)
        {
            if (!unit || unit->GetHP() <= 0) return;
            if (map->GetRoomAt(unit->GetGridPos()) == room) ++count;
        };

        countIfInRoom(units->GetPlayer());
        for (Enemy* enemy : units->GetEnemies()) countIfInRoom(enemy);
        for (Ally* ally : units->GetAllies()) countIfInRoom(ally);
        return (std::max)(1, count);
    }

    float GetSkillUseProbability(Unit& self, const Skill& skill)
    {
        if (skill.probabilityType != Skill::ProbabilityType::UserRoomUnitCount) {
            return skill.useProbability;
        }

        // 部屋全体特技は基本5%を起点に、部屋内ユニットが増えるほど使用確率を上げる。
        float probability = skill.useProbability + (CountUnitsInUserRoom(self) - 1) * skill.roomUnitProbabilityStep;
        return (std::min)(1.0f, probability);
    }

    bool IsSkillConditionMet(UnitAI& ai, Unit& self, Unit* target, const Skill& skill)
    {
        switch (skill.condition) {
        case Skill::Condition::Always:
            return true;
        case Skill::Condition::AdjacentToTarget:
            return ai.IsAttackAdjacent(self, target);
        case Skill::Condition::HPBelowHalf:
            return self.GetHP() <= self.GetMaxHP() / 2;
        default:
            return false;
        }
    }
}
bool UnitAI::CanSee(Unit& self, Unit* target, MapData* map, int visionRange)
{
    if (!target || !map || visionRange <= 0) return false;

    Vector2Int selfPos = self.GetGridPos();
    Vector2Int targetPos = target->GetGridPos();
    int dx = abs(targetPos.x - selfPos.x);
    int dy = abs(targetPos.y - selfPos.y);
    int dist = (std::max)(dx, dy);
    if (dist > visionRange) return false;

    Room* selfRoom = map->GetRoomAt(selfPos);
    if (selfRoom && map->GetRoomAt(targetPos) == selfRoom) return true;

    return HasLineOfSight(selfPos, targetPos, map);
}

bool UnitAI::HasLineOfSight(Vector2Int from, Vector2Int to, MapData* map)
{
    if (!map) return false;

    int x0 = from.x;
    int y0 = from.y;
    int x1 = to.x;
    int y1 = to.y;
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int ax = abs(x1 - x0);
    int ay = abs(y1 - y0);
    int err = (ax > ay ? ax : -ay) / 2;

    while (true) {
        if ((x0 != from.x || y0 != from.y) && map->GetTile(x0, y0) == TileType::Wall) return false;
        if (x0 == x1 && y0 == y1) break;

        int e2 = err;
        if (e2 > -ax) { err -= ay; x0 += sx; }
        if (e2 < ay) { err += ax; y0 += sy; }
    }
    return true;
}
bool UnitAI::ExecuteSkill(Unit& self, Unit* target) {
    if (!target) return false;
    if (!EffectTargeting::IsValidSkillTargetForUser(&self, target)) return false;

    for (auto& skill : self.GetSkills()) {
        if (!IsSkillConditionMet(*this, self, target, skill)) continue;
        if (GameRandom::Value() > GetSkillUseProbability(self, skill)) continue;

        if (skill.effect) {
            EffectContext ctx;
            ctx.source = EffectSourceType::Skill;
            ctx.user = &self;
            ctx.target = target; // 攻撃なら敵、回復なら自分など、AIの文脈で決まる
            ctx.pos = target->GetGridPos();
            ctx.direction = target->GetGridPos() - self.GetGridPos();
            ctx.targetType = skill.targetType;
            ctx.targetRadius = skill.targetRadius;

            std::vector<Unit*> collectedTargets;
            if (ctx.targetType != EffectTargetType::Single) {
                collectedTargets = EffectTargeting::CollectUnits(ctx);
                if (collectedTargets.empty()) continue;
            }

            MessageLog::Instance().AddMessage(self.GetName() + u8"の" + skill.name + u8"！");
            if (ctx.targetType == EffectTargetType::Single) skill.effect->Apply(ctx);
            else {
                for (Unit* unit : collectedTargets) {
                    EffectContext each = ctx;
                    each.target = unit;
                    each.pos = unit->GetGridPos();
                    skill.effect->Apply(each);
                }
            }

            // アニメーション等のトリガー
            if (self.ShouldShowCombatVisual(target)) self.SetTriggerAnimation("Skill");
            return true;
        }
    }
    return false;
}
bool UnitAI::IsAdjacent(Unit& self, Unit* target)
{
    if (!target) return false;

    Vector2Int p = target->GetGridPos();
    Vector2Int e = self.GetGridPos();

    int dx = p.x - e.x;
    int dy = p.y - e.y;

    int adx = abs(dx);
    int ady = abs(dy);

    if (adx == 0 && ady == 0) return false;

    // 直交隣接 (上下左右)
    if (adx + ady == 1) return true;

    if (adx == 1 && ady == 1) {
        MapData* map = MapManager::Instance()->GetCurrentMap();
        if (map && self.IsDiagonalMoveBlocked(e, { dx, dy }, map)) return false;
        return true;
    }

    return false;
}

bool UnitAI::IsAttackAdjacent(Unit& self, Unit* target, MapData* map)
{
    if (!target) return false;

    Vector2Int p = target->GetGridPos();
    Vector2Int e = self.GetGridPos();

    int dx = p.x - e.x;
    int dy = p.y - e.y;
    int adx = abs(dx);
    int ady = abs(dy);

    if (adx == 0 && ady == 0) return false;
    if (adx + ady == 1) return true;

    if (adx == 1 && ady == 1) {
        if (!map) map = MapManager::Instance()->GetCurrentMap();
        if (!map) return false;
        return map->IsWalkable({ e.x + dx, e.y }) && map->IsWalkable({ e.x, e.y + dy });
    }

    return false;
}
void UnitAI::ExecuteConfusion(Unit& self, MapData* map) {
    static const Vector2Int dirs[8] = {
        {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}
    };

    int actionType = GameRandom::Range(0, 2); // 0:方向転換 1:移動 2:攻撃
    int dirIndex = GameRandom::Range(0, 7);
    Vector2Int randomDir = dirs[dirIndex];
    Vector2Int targetPos = self.GetGridPos() + randomDir;
    auto faceRandomDir = [&]() {
        self.SetCurrentDir(randomDir);
        self.LookAt(randomDir);
        self.UpdateFacingRotation();
    };

    switch (actionType) {
    case 0: // 方向転換のみ
        faceRandomDir();
        break;

    case 1:
        if (map->IsInBounds(targetPos) && map->IsWalkable(targetPos)) {
            // 斜め移動時の角抜けチェック
            if (!self.IsDiagonalMoveBlocked(self.GetGridPos(), randomDir, map)) {
                // 移動先にユニットがいないかチェック
                if (!UnitManager::Instance()->GetUnitAt(targetPos)) {
                    faceRandomDir();
                    self.RequestMove(targetPos);
                    self.EndTurn();
                    self.ConsumeAllMoves();
                    return;
                }
            }
        }
        // 移動できなかった場合は方向転換のみ
        faceRandomDir();
        break;

    case 2:
        faceRandomDir();
        if (!self.IsDiagonalMoveBlocked(self.GetGridPos(), randomDir, map)) {
            Unit* target = UnitManager::Instance()->GetUnitAt(targetPos);
            if (target && target != &self) {
                // 混乱中の直接攻撃も、通常攻撃と同じく表示中ログをリセットする。
                MessageLog::Instance().Clear();
                if (self.ShouldShowCombatVisual(target)) self.SetTriggerAnimation("Attack", 1.0f);
                if (!self.CheckHit(self.GetACC(), target->GetEVD())) {
                    MessageLog::Instance().AddMessage(self.GetName() + u8"の攻撃は外れた。");
                    self.EndTurn();
                    self.ConsumeAllMoves();
                    return;
                }

                int damage = self.CalcDamage(self.GetATK(), target->GetDEF());
                target->TakeDamage(damage, &self);
                self.EndTurn();
                self.ConsumeAllMoves();
                return;
            }
        }
        break;
    }

    self.EndTurn();
    self.ConsumeAllMoves();
}

std::vector<Vector2Int> UnitAI::BFSPath(Unit& self, const Vector2Int& start, const Vector2Int& goal, MapData* map)
{
    if (!map || !map->IsInBounds(start) || !map->IsInBounds(goal)) return {};

    std::queue<Vector2Int> q;
    std::unordered_map<int, Vector2Int> cameFrom;

    //ここのencodeは、マップの横サイズと現在位置の座標から、
    //一番左上のマス(番号で言うと0番)から何マス離れた場所にいるかをintで返す(番号)
    //50×50だと、(2,2)はintの102(番)が返る 2×50 + 2
    //要するにこれは、マップの位置座標をintだけで返す関数となる(camefromの設計の都合上intが一番都合がいい)
    auto encode = [&](const Vector2Int& p) {
        return p.y * map->GetWidth() + p.x;
        };

    //まず現在位置から先にキューへ入れる(現在キューの先頭は現在位置)
    q.push(start);

    //どこから来たのかの配列に最初の位置を入れる(これがないと始まらない)
    cameFrom[encode(start)] = start;

    // 8方向に拡張
    const Vector2Int dirs[8] = {
        {1,0}, {-1,0}, {0,1}, {0,-1},  // 上下左右
        {1,1}, {1,-1}, {-1,1}, {-1,-1} // 斜め
    };

    bool found = false;
    while (!q.empty())
    {
        //今のキューの最前面を入れる(最初だと現在位置に該当する)
        Vector2Int cur = q.front();
        //最前面を出してキューに入っている位置情報を詰める(捨てる)
        q.pop();

        //ゴール座標と一致した場合見つかった判定になり経路を入力するフェーズに入る
        if (cur == goal)
        {
            found = true;
            break;
        }

        //次移動可能なマスの位置を入れるループ
       // ★ 8方向（固定配列ではなく可変ベクタ）
        std::vector<Vector2Int> dirs = {
            {1,0}, {-1,0}, {0,1}, {0,-1},
            {1,1}, {1,-1}, {-1,1}, {-1,-1}
        };

        // ★ ゴールに近づく方向を優先するようソート
        std::sort(dirs.begin(), dirs.end(),
            [&](const Vector2Int& a, const Vector2Int& b)
            {
                Vector2Int nextA = cur + a;
                Vector2Int nextB = cur + b;

                int distA = abs(goal.x - nextA.x) + abs(goal.y - nextA.y);
                int distB = abs(goal.x - nextB.x) + abs(goal.y - nextB.y);

                return distA < distB;
            });

        // ★ ソート済み方向で探索
        for (auto& d : dirs)
        {
            Vector2Int next = cur + d;

            if (!map->IsInBounds(next)) continue;
            if (!map->IsWalkable(next)) continue;
            if (self.IsDiagonalMoveBlocked(cur, d, map)) continue;

            if (next != goal)
            {
                // 修正ポイント：自分と同勢力のユニットがいる場合は避ける
                Unit* existingUnit = UnitManager::Instance()->GetUnitAt(next);
                if (existingUnit) {
                    bool isSameFaction = false;
                    if (dynamic_cast<Ally*>(&self) && dynamic_cast<Ally*>(existingUnit)) isSameFaction = true;
                    if (dynamic_cast<Enemy*>(&self) && dynamic_cast<Enemy*>(existingUnit)) isSameFaction = true;
                    if (dynamic_cast<Player*>(existingUnit)) isSameFaction = true;

                    if (isSameFaction) continue;
                }
            }

            int code = encode(next);
            if (cameFrom.count(code)) continue;//既に調べたマスの番号が入ってる場合スキップ

            //次に行く予定のマスの位置に調べた方向のマスの座標を代入する
            // (要するに、curに入っている座標から来たことを示すためにcurの座標を敢えて入れる)
            //これを、8方向すべての移動可能なマスに記録することでそのマスすべてにcur(現在の座標)から来たことを書き込む、という意味になる
            cameFrom[code] = cur;
            //今調べた方向をキューに入れるのを忘れずに
            q.push(next);
        }

    }

    if (!found) return {};

    // 経路復元
    std::vector<Vector2Int> path;
    Vector2Int cur = goal;
    while (!(cur == start))
    {
        //本題となるとなる辿っていくルートを作成する
        //ここでは、ゴールのマスから始めて最初の位置の一歩手前まで経路を入れる

        path.push_back(cur);//経路を入れる
        cur = cameFrom[encode(cur)];//調べた経路を辿る

        //これを一回ずつ繰り返す
    }
    //逆から(ゴールから)計算してたので、これを反転させる
    std::reverse(path.begin(), path.end());
    //経路が見つかったんでreturn
    return path;
}

bool UnitAI::IsAdjacentToPlayer(Unit& self)
{
    return IsAdjacent(self, UnitManager::Instance()->GetPlayer());
}


