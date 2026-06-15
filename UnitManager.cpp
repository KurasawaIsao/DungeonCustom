#include "UnitManager.h"
#include"Unit.h"
#include "player.h"
#include "Enemy.h"
#include "UnitAI.h"
#include "MapManager.h"
#include "Ally.h"
#include <algorithm>
UnitManager* UnitManager::m_Instance = nullptr;

// UnitManager は Scene に散らばる Player/Enemy/Ally への参照をまとめる検索窓口。
// ターン処理、攻撃判定、マップ占有判定はここを通すことで、各クラスが Scene 全体を直接探し回らずに済む。

void UnitManager::ClearAllEnemies()
{
    // 階層遷移時は敵だけを破棄予約し、Scene の通常 Uninit とは独立してフロア単位で片付ける。
    for (Enemy* e : m_Enemies)
    {
        if (e)
        {
            e->StopLoopEffect();
            e->SetDestroy();
            e = nullptr;
        }
    }
    m_Enemies.clear();
}

void UnitManager::ClearAllAllies()
{
    for (Ally* a : m_Allies)
    {
        if (a)
        {
            a->StopLoopEffect();
            a->SetDestroy();
            a = nullptr;
        }
    }
    m_Allies.clear();
}

void UnitManager::ClearSceneReferences()
{
    // Sceneをまたいで破棄済みUnitを参照しないよう、テストプレイ開始/終了時に登録だけを必ず空にする。
    m_Player = nullptr;
    m_Enemies.clear();
    m_Allies.clear();
}

Unit* UnitManager::GetUnitAt(const Vector2Int& pos) const
{
    // 攻撃・移動・投擲の共通当たり判定。
    // 優先順はプレイヤー -> 仲間 -> 敵。通常は同じマスに複数 Unit がいない前提。
    if (m_Player && m_Player->GetGridPos() == pos) return m_Player;
    for (auto* a : m_Allies) if (a && a->GetGridPos() == pos) return a;
    for (auto* e : m_Enemies) if (e && e->GetGridPos() == pos) return e;
    return nullptr;
}

void UnitManager::RegisterEnemy(Enemy* enemy)
{
    // GeneraterPlacer が敵を Scene に追加した直後に登録する。
    // TurnManager はこの配列を使って敵ターンを回す。
    if (!enemy) return;
    if (std::find(m_Enemies.begin(), m_Enemies.end(), enemy) != m_Enemies.end()) return;
    m_Enemies.push_back(enemy);
}
void UnitManager::RemoveEnemy(Enemy* enemy)
{
    if (!enemy) return;
    m_Enemies.erase(
        std::remove(m_Enemies.begin(), m_Enemies.end(), enemy),
        m_Enemies.end()
    );
}

void UnitManager::RegisterAlly(Ally* ally)
{
    if (!ally) return;
    if (std::find(m_Allies.begin(), m_Allies.end(), ally) != m_Allies.end()) return;
    m_Allies.push_back(ally);
}
void UnitManager::RemoveAlly(Ally* ally)
{
    if (!ally) return;

    m_Allies.erase(
        std::remove(m_Allies.begin(), m_Allies.end(), ally),
        m_Allies.end()
    );
}
std::vector<Enemy*> UnitManager::GetAdjacentEnemies(Unit& self) const
{
    std::vector<Enemy*> neighbors;
    Vector2Int myPos = self.GetGridPos();
    MapData* map = MapManager::Instance()->GetCurrentMap();

    for (auto* e : m_Enemies) {
        if (!e) continue;

        Vector2Int eP = e->GetGridPos();
        Vector2Int dir = eP - myPos;
        int chebyshev = dir.Chebyshev(Vector2Int(0, 0));
        int manhattan = dir.Manhattan(Vector2Int(0, 0));

        // 隣接判定
        if (manhattan == 1) {
            neighbors.push_back(e);
        }
        else if (chebyshev == 1 && manhattan == 2) {
            if (map->IsWalkable({ myPos.x + dir.x, myPos.y }) && map->IsWalkable({ myPos.x, myPos.y + dir.y })) {
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
    if (m_Player) {
        float dist = Vector2Int::Distance(pos, m_Player->GetGridPos());
        minDist = dist;
        nearest = m_Player;
    }

    // 全ての味方(Ally)との距離をチェックし、より近い方がいれば更新
    for (auto* a : m_Allies) {
        if (!a) continue;
        float dist = Vector2Int::Distance(pos, a->GetGridPos());
        if (dist < minDist) {
            minDist = dist;
            nearest = a;
        }
    }

    return nearest;
}