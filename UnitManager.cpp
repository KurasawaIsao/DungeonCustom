#include "UnitManager.h"
#include"Unit.h"
#include "player.h"
#include "Enemy.h"
#include "UnitAI.h"
#include "MapManager.h"
#include "Ally.h"
#include <algorithm>
UnitManager* UnitManager::instance = nullptr;

// UnitManager は Scene に散らばる Player/Enemy/Ally への参照をまとめる検索窓口。
// ターン処理、攻撃判定、マップ占有判定はここを通すことで、各クラスが Scene 全体を直接探し回らずに済む。

void UnitManager::ClearAllEnemies()
{
    // 階層遷移時は敵だけを破棄予約し、Scene の通常 Uninit とは独立してフロア単位で片付ける。
    for (Enemy* e : enemies)
    {
        if (e)
        {
            e->StopLoopEffect();
            e->SetDestroy();
            e = nullptr;
        }
    }
    enemies.clear();
}

void UnitManager::ClearAllAllies()
{
    for (Ally* a : allies)
    {
        if (a)
        {
            a->StopLoopEffect();
            a->SetDestroy();
            a = nullptr;
        }
    }
    allies.clear();
}

void UnitManager::ClearSceneReferences()
{
    // Sceneをまたいで破棄済みUnitを参照しないよう、テストプレイ開始/終了時に登録だけを必ず空にする。
    player = nullptr;
    enemies.clear();
    allies.clear();
}

//指定のマスに敵がいるかどうか
bool UnitManager::HasEnemy(const Vector2Int& pos) const
{
    // プレイヤーの移動先チェックなど、敵だけを障害物として見たい時に使う。
    for (auto* e : enemies)
    {
        if (e && e->GetGridPos() == pos)
            return true;
    }
    return false;
}
Unit* UnitManager::GetUnitAt(const Vector2Int& pos) const
{
    // 攻撃・移動・投擲の共通当たり判定。
    // 優先順はプレイヤー -> 仲間 -> 敵。通常は同じマスに複数 Unit がいない前提。
    if (player && player->GetGridPos() == pos) return player;
    for (auto* a : allies) if (a && a->GetGridPos() == pos) return a;
    for (auto* e : enemies) if (e && e->GetGridPos() == pos) return e;
    return nullptr;
}

void UnitManager::RegisterEnemy(Enemy* enemy)
{
    // GeneraterPlacer が敵を Scene に追加した直後に登録する。
    // TurnManager はこの配列を使って敵ターンを回す。
    if (!enemy) return;
    if (std::find(enemies.begin(), enemies.end(), enemy) != enemies.end()) return;
    enemies.push_back(enemy);
}
void UnitManager::RemoveEnemy(Enemy* enemy)
{
    if (!enemy) return;
    enemies.erase(
        std::remove(enemies.begin(), enemies.end(), enemy),
        enemies.end()
    );
}

void UnitManager::RegisterAlly(Ally* ally)
{
    if (!ally) return;
    if (std::find(allies.begin(), allies.end(), ally) != allies.end()) return;
    allies.push_back(ally);
}
void UnitManager::RemoveAlly(Ally* ally)
{
    if (!ally) return;

    allies.erase(
        std::remove(allies.begin(), allies.end(), ally),
        allies.end()
    );
}
std::vector<Enemy*> UnitManager::GetAdjacentEnemies(Unit& self) const
{
    std::vector<Enemy*> neighbors;
    Vector2Int myPos = self.GetGridPos();
    MapData* map = MapManager::Instance()->GetCurrentMap();

    for (auto* e : enemies) {
        if (!e) continue;

        Vector2Int eP = e->GetGridPos();
        int dx = eP.x - myPos.x;
        int dy = eP.y - myPos.y;
        int adx = abs(dx);
        int ady = abs(dy);

        // 隣接判定
        if ((adx == 1 && ady == 0) || (adx == 0 && ady == 1)) {
            neighbors.push_back(e);
        }
        else if (adx == 1 && ady == 1) {
            if (map->IsWalkable({ myPos.x + dx, myPos.y }) && map->IsWalkable({ myPos.x, myPos.y + dy })) {
                neighbors.push_back(e);
            }
        }
    }
    return neighbors;
}
Unit* UnitManager::GetNearestHostileToEnemy(const Vector2Int& pos) const
{
    Unit* nearest = nullptr;
    float minDist = 1e9f;

    // プレイヤーとの距離をチェック
    if (player) {
        float dist = Vector2Int::Distance(pos, player->GetGridPos());
        minDist = dist;
        nearest = player;
    }

    // 全ての味方(Ally)との距離をチェックし、より近い方がいれば更新
    for (auto* a : allies) {
        if (!a) continue;
        float dist = Vector2Int::Distance(pos, a->GetGridPos());
        if (dist < minDist) {
            minDist = dist;
            nearest = a;
        }
    }

    return nearest;
}