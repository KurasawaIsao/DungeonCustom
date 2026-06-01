#include "MapObjectPlacer.h"
#include "MapData.h"
#include "Manager.h"
#include "Scene.h"
#include "Item.h"
#include "Trap.h"
#include "Shrine.h"
#include "Enemy.h"
#include "EnemyDatabase.h"
#include "ItemDataBase.h"
#include "TrapDataBase.h"
#include "MapEditor.h"
#include "MessageLog.h"
#include "imgui.h"
#include <algorithm>

void MapObjectPlacer::InitializeSelection() {
    auto allItems = ItemDatabase::GetAll();
    if (!allItems.empty()) m_SelectedItemId = allItems[0].id;

    auto allTraps = TrapDatabase::GetAllIds();
    if (!allTraps.empty()) m_SelectedTrapId = allTraps[0];

    if (!EnemyDatabase::GetAll().empty()) m_SelectedEnemyId = EnemyDatabase::GetAll().begin()->first;
}
void MapObjectPlacer::PlaceItem(MapData* map, int x, int y) {
    if (!map || !map->IsInside(x, y)) return;
    // 壁など歩けないマスには配置しない。
    if (!map->IsWalkable(x, y)) return;

    RemoveObjectAt(map, x, y);

    const ItemData* data = ItemDatabase::Get(m_SelectedItemId);
    if (!data) return;

    // 1. 生成
    auto* itemObj = Manager::GetScene()->AddGameObject<Item>(1);

    // 2. インスタンスの作成
    ItemInstance inst(data);
    inst.InitIdentify();

    // --- 型に応じた初期化 ---
    if (data->type == ItemType::Pot) {
        inst.CreatePot(m_SelectedItemCount, true); 
    }
    else if (data->type == ItemType::Staff) {
        inst.SetCharge(m_SelectedItemCount); 
    }
    else if (data->type == ItemType::Arrow || data->type == ItemType::Stone) {
        if (m_SelectedItemCount < 3) m_SelectedItemCount = 3;
        if (m_SelectedItemCount > 8) m_SelectedItemCount = 8;
        inst.SetStackCount(m_SelectedItemCount);
    }

    // モデルのセットアップ
    itemObj->SetInstance(std::move(inst));
   

    //  座標と可視化
    itemObj->SetPosition({ x * (float)TILE_DISTANCE, 0.01f, y * (float)TILE_DISTANCE });
    itemObj->SetVisible(true);

    map->AddMapObject(itemObj, x, y);

    // 管理リスト更新
    EditorPlacedObject newObj;
    newObj.type = EditorPlacedObject::Type::Item;
    newObj.id = m_SelectedItemId;
    newObj.pos = { x, y };
    newObj.view = itemObj;
    if (data->type == ItemType::Pot) newObj.potCapacity = m_SelectedItemCount;
    else if (data->type == ItemType::Staff) newObj.staffCharges = m_SelectedItemCount;
    else if (data->type == ItemType::Arrow || data->type == ItemType::Stone) newObj.stackCount = m_SelectedItemCount;
    m_PlacedObjects.push_back(newObj);
}

void MapObjectPlacer::PlaceTrap(MapData* map, int x, int y)
{
    if (!map || !map->IsInside(x, y)) return;
    // 壁など歩けないマスには配置しない。
    if (!map->IsWalkable(x, y)) return;

    RemoveObjectAt(map, x, y);

    const TrapData* data = TrapDatabase::Get(m_SelectedTrapId);
    if (!data) return;

    // 新しい罠の生成
    Trap* trapObj = Manager::GetScene()->AddGameObject<Trap>(1);
    trapObj->Init();
    trapObj->Setup(data);
    trapObj->SetPosition({ x * 2.0f, 0.01f, y * 2.0f });
    trapObj->SetVisible(true);

    map->AddMapObject(trapObj, x, y);

    // エディタ用リストへの登録
    EditorPlacedObject newObj;
    newObj.type = EditorPlacedObject::Type::Trap;
    newObj.id = m_SelectedTrapId;
    newObj.pos = { x, y };
    newObj.view = trapObj;
    m_PlacedObjects.push_back(newObj);
}
void MapObjectPlacer::PlaceShrine(MapData* map, int x, int y)
{
    if (!map || !map->IsInside(x, y)) return;
    // 壁など歩けないマスには配置しない。
    if (!map->IsWalkable(x, y)) return;

    RemoveObjectAt(map, x, y);

    Shrine* shrineObj = Manager::GetScene()->AddGameObject<Shrine>(1);
    shrineObj->Init();
    shrineObj->SetPosition({ x * 2.0f, 0.01f, y * 2.0f });

    // MapData への追加
    map->AddMapObject(shrineObj, x, y);

    // エディタ用リストへの登録
    EditorPlacedObject newObj;
    newObj.type = EditorPlacedObject::Type::Shrine;
    newObj.pos = { x, y };
    newObj.view = shrineObj;
    m_PlacedObjects.push_back(newObj);
}

void MapObjectPlacer::PlaceEnemy(MapData* map, int x, int y)
{
    PlaceEnemyByID(map, m_SelectedEnemyId, x, y);
}

void MapObjectPlacer::PlaceEnemyByID(MapData* map, std::string id, int x, int y)
{
    if (!map || !map->IsInside(x, y)) return;
    // 敵は歩行可能なマスにだけ置き、固定マップの初期配置として扱う。
    if (!map->IsWalkable(x, y)) return;

    RemoveObjectAt(map, x, y);

    if (map->GetObjectAt(x, y) || map->GetUnitAt(x, y)) return;

    const EnemyData* data = EnemyDatabase::Get(id);
    if (!data) return;

    Enemy* enemyObj = Manager::GetScene()->AddGameObject<Enemy>(1);
    enemyObj->ApplyData(*data);
    enemyObj->ClearStatus();
    enemyObj->SetEditorPreviewOnly(true);
    enemyObj->SetInitGridPos({ x, y });

    // エディタの削除・保存対象として、敵IDと座標を保持する。
    EditorPlacedObject newObj;
    newObj.type = EditorPlacedObject::Type::Enemy;
    newObj.id = id;
    newObj.pos = { x, y };
    newObj.enemyView = enemyObj;
    m_PlacedObjects.push_back(newObj);
}

void MapObjectPlacer::RemoveObjectAt(MapData* map, int x, int y) {
    auto it = std::find_if(m_PlacedObjects.begin(), m_PlacedObjects.end(), [&](const EditorPlacedObject& o) {
        return o.pos.x == x && o.pos.y == y;
        });

    if (it != m_PlacedObjects.end()) {
        if (it->view) {
            map->RemoveMapObject(it->view);
            it->view->SetDestroy();
        }
        if (it->enemyView) {
            // エディタ表示用の敵をシーンから破棄する。
            it->enemyView->StopLoopEffect();
            it->enemyView->SetDestroy();
        }
        m_PlacedObjects.erase(it);
        // ここで return することで、削除後のイテレータを触るリスクをゼロにする
        return;
    }
}
void MapObjectPlacer::ClearAllObjects(MapData* map)
{
    for (auto& obj : m_PlacedObjects) {
        if (obj.view) {
            // MapData側の管理(Grid配列とAllObjects)から外す
            map->RemoveMapObject(obj.view);
            // Sceneから消去
            obj.view->SetDestroy();
        }
        if (obj.enemyView) {
            // 敵プレビューはMapObjectではないため、シーン側の実体だけ破棄する。
            obj.enemyView->StopLoopEffect();
            obj.enemyView->SetDestroy();
        }
    }
    m_PlacedObjects.clear();
}

void MapObjectPlacer::DrawPlacerTab(MapData* map,EditorPlaceMode mode)
{
    if (mode == EditorPlaceMode::Item) {
        const auto items = ItemDatabase::GetAll();

        // 現在選択中のアイテム名を取得
        const ItemData* selectedItem = ItemDatabase::Get(m_SelectedItemId);
        const char* comboLabel = selectedItem ? selectedItem->name.c_str() : "Select Item...";

        if (ImGui::BeginCombo("Select Item", comboLabel)) {
            for (const auto& it : items) {
                // it.name.c_str() を使用して一覧を表示
                if (ImGui::Selectable(it.name.c_str(), m_SelectedItemId == it.id)) {
                    m_SelectedItemId = it.id;
                }
            }
            ImGui::EndCombo();
        }
        if (selectedItem) {
            if (selectedItem->useType == ItemUseType::Zap) {
                ImGui::InputInt("Staff Charges (Count)", &m_SelectedItemCount);
                if (m_SelectedItemCount < 0) m_SelectedItemCount = 0;
            }
            else if (selectedItem->useType == ItemUseType::Put) {
                ImGui::InputInt("Pot Capacity (Size)", &m_SelectedItemCount);
                if (m_SelectedItemCount < 1) m_SelectedItemCount = 1;
            }
            else if (selectedItem->type == ItemType::Arrow || selectedItem->type == ItemType::Stone) {
                ImGui::InputInt("Stack Count", &m_SelectedItemCount);
                if (m_SelectedItemCount < 3) m_SelectedItemCount = 3;
                if (m_SelectedItemCount > 8) m_SelectedItemCount = 8;
            }
        }
    }
    else if (mode == EditorPlaceMode::Trap) {
        const TrapData* selectedData = TrapDatabase::Get(m_SelectedTrapId);
        const char* comboLabel = selectedData ? selectedData->name.c_str() : "Select Trap...";

        if (ImGui::BeginCombo("Select Trap", comboLabel)) {
            for (const auto& id : TrapDatabase::GetAllIds()) {
                const TrapData* tData = TrapDatabase::Get(id);
                if (!tData) continue;

                if (ImGui::Selectable(tData->name.c_str(), m_SelectedTrapId == id)) {
                    m_SelectedTrapId = id;
                }
            }
            ImGui::EndCombo();
        }
    }

    else if (mode == EditorPlaceMode::Enemy) {
        std::vector<const EnemyData*> enemies;
        for (const auto& pair : EnemyDatabase::GetAll()) {
            enemies.push_back(&pair.second);
        }
        std::sort(enemies.begin(), enemies.end(), [](const EnemyData* a, const EnemyData* b) {
            return a->id < b->id;
            });

        const EnemyData* selectedEnemy = EnemyDatabase::Get(m_SelectedEnemyId);
        std::string comboLabel = selectedEnemy ? selectedEnemy->name  : "Select Enemy...";
        if (ImGui::BeginCombo("Select Enemy", comboLabel.c_str())) {
            for (const EnemyData* enemy : enemies) {
                if (!enemy) continue;
                std::string label = enemy->name;
                if (ImGui::Selectable(label.c_str(), m_SelectedEnemyId == enemy->id)) {
                    m_SelectedEnemyId = enemy->id;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Left click: place / Right click: remove");
    }
    else if (mode == EditorPlaceMode::Shrine) {
        ImGui::TextColored(ImVec4(0, 1, 1, 1), "Set Shrine");
    }
}
void MapObjectPlacer::PlaceItemByID(MapData* map, std::string id, int x, int y, int count) {
    if (!map || !map->IsInside(x, y)) return;
    // JSON読み込み時も、壁など歩けないマスの配置は無視する。
    if (!map->IsWalkable(x, y)) return;

    RemoveObjectAt(map, x, y);

    const ItemData* data = ItemDatabase::Get(id);
    if (!data) return;

    auto* itemObj = Manager::GetScene()->AddGameObject<Item>(1);

    ItemInstance inst(data);
    inst.InitIdentify();
    if (data->type == ItemType::Pot) inst.CreatePot(count > 0 ? count : 4, true);
    else if (data->type == ItemType::Staff) inst.SetCharge(count > 0 ? count : 5);
    else if (data->type == ItemType::Arrow || data->type == ItemType::Stone) {
        if (count < 3) count = 3;
        if (count > 8) count = 8;
        inst.SetStackCount(count);
    }
    itemObj->SetInstance(std::move(inst));

    // AddGameObject の時点で Init 済み。ここで再Initすると読み込んだアイテムモデルが空に戻る。
    itemObj->SetPosition({ x * 2.0f, 0.01f, y * 2.0f });
    itemObj->SetVisible(true);

    map->AddMapObject(itemObj, x, y);

    EditorPlacedObject newObj;
    newObj.type = EditorPlacedObject::Type::Item;
    newObj.id = id;
    newObj.pos = { x, y };
    newObj.view = itemObj;
    if (data->type == ItemType::Pot) newObj.potCapacity = count;
    else if (data->type == ItemType::Staff) newObj.staffCharges = count;
    else if (data->type == ItemType::Arrow || data->type == ItemType::Stone) newObj.stackCount = count;
    m_PlacedObjects.push_back(newObj);
}

void MapObjectPlacer::PlaceTrapByID(MapData* map, std::string id, int x, int y) {
    if (!map || !map->IsInside(x, y)) return;
    // JSON読み込み時も、壁など歩けないマスの配置は無視する。
    if (!map->IsWalkable(x, y)) return;

    // 1. 重複削除（アイテム、罠、祠を問わずその座標にあるものを消す）
    RemoveObjectAt(map, x, y);

    //  TrapDataBaseからデータを取得
    const TrapData* data = TrapDatabase::Get(id);
    if (!data) return;

    //  Trapオブジェクトの生成
    auto* trapObj = Manager::GetScene()->AddGameObject<Trap>(1);
    trapObj->Init();
    trapObj->Setup(data);
    trapObj->SetPosition({ x * 2.0f, 0.01f, y * 2.0f });
    trapObj->SetVisible(true);

    // 4. MapData への登録
    map->AddMapObject(trapObj, x, y);

    // 5. 管理リストへの登録
    EditorPlacedObject newObj;
    newObj.type = EditorPlacedObject::Type::Trap;
    newObj.id = id;
    newObj.pos = { x, y };
    newObj.view = trapObj;
    m_PlacedObjects.push_back(newObj);
}