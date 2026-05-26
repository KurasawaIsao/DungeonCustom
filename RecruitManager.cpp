#include "RecruitManager.h"
#include "Enemy.h"
#include "Ally.h"
#include "Manager.h"
#include "Scene.h"
#include "UnitManager.h"
#include "MapManager.h"
#include "MessageLog.h"
#include "Player.h"
#include "TurnManager.h"
void RecruitManager::ExecuteRecruit(Enemy* target, const std::string& customName) {
    if (!target) return;

    auto* scene = Manager::GetScene();

    // 1. 新しい仲間を生成して初期化
    auto* newAlly = scene->AddGameObject<Ally>(1);
    newAlly->InitFromEnemy(target);
    newAlly->SetName(customName);

    // 2. 座標の設定
    Vector2Int pos = target->GetGridPos();
    newAlly->SetGridPos(pos);
    newAlly->SetPosition(Vector3(pos.x * TILE_DISTANCE, 0.01f, pos.y * TILE_DISTANCE));

    // 3. 各種マネージャーへの登録
    UnitManager::Instance()->RegisterAlly(newAlly);

    MessageLog::Instance().AddMessage(customName + u8"が仲間に加わった！");

    // 4. 元の敵を削除
    UnitManager::Instance()->RemoveEnemy(target);
    target->SetDestroy();

    // 5. プレイヤーの状態復帰
    auto* player = scene->GetGameObject<Player>();
    if (player) {
        player->SetInputEnable(true);
    }
    TurnManager::Instance()->ResumeTurnProgression();
}

void RecruitManager::DeclineRecruit(Enemy* target) {
    if (!target) return;

    MessageLog::Instance().AddMessage(target->GetName() + u8"はさみしそうに去っていった...");

    auto* map = MapManager::Instance()->GetCurrentMap();
    UnitManager::Instance()->RemoveEnemy(target);
    target->SetDestroy();

    auto* player = Manager::GetScene()->GetGameObject<Player>();
    if (player) player->SetInputEnable(true);
    TurnManager::Instance()->ResumeTurnProgression();
}