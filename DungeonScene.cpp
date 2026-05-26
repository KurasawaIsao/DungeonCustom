#include "DungeonScene.h"
#include "MapGenerator.h"
#include "MapManager.h"
#include "Skydorm.h"
#include "camera.h"
#include "player.h"
#include "Item.h"
#include "input.h"
#include "manager.h"
#include "UnitManager.h"
#include "ConfirmWindow.h"
#include "TurnManager.h"
#include "ItemDataBase.h"
#include "PlayerInventoryUI.h"
#include "ShopUI.h"
#include "MapRenderer.h"
#include "DungeonData.h"
#include "DungeonDataIO.h"
#include "SceneDataReference.h"
#include "ItemTableDataBase.h"
#include "TrapDataBase.h"
#include "FloorTransitionUI.h"
#include "LightManager.h"
#include "audio.h"
#include "EditorScene.h"
#include "ImguiObject.h"
#include "imgui.h"
#include "VisionMaskRenderer.h"
#include "MiniMapRenderer.h"
#include "DungeonEndingUI.h"

// DungeonScene は実際のダンジョンプレイ画面。
// Init で「共通オブジェクト生成 -> ダンジョンデータ読込 -> マップ生成 -> 表示物構築」を行い、
// Update では Scene 全体の更新後に TurnManager へターン制処理を渡す。

namespace
{
    class TestPlayReturnUI : public GameObject, public ImGuiObject
    {
    public:
        void Init() override {}
        void Update() override
        {
            if (DataReference::IsEditorTestPlay && Input::GetKeyTrigger(VK_ESCAPE))
            {
                ReturnToEditor();
            }
        }
        void Draw() override {}
        void Uninit() override {}
        void DrawImGui() override
        {
            if (!DataReference::IsEditorTestPlay) return;

            const ImVec2 windowSize(220, 130);
            ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(displaySize.x - windowSize.x - 20.0f, 20.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
            ImGui::Begin("Test Play", nullptr,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings);
            if (ImGui::Button("Return to Editor", ImVec2(-1, 32)))
            {
                ReturnToEditor();
            }

            if (auto* miniMap = Manager::GetScene()->GetGameObject<MiniMapRenderer>())
            {
                bool showFullMap = miniMap->IsShowFullMap();
                if (ImGui::Checkbox("Full Minimap", &showFullMap))
                {
                    miniMap->SetShowFullMap(showFullMap);
                }

                bool showDiscoveredMap = miniMap->IsShowDiscoveredMap();
                if (ImGui::Checkbox("Discovered Minimap", &showDiscoveredMap))
                {
                    miniMap->SetShowDiscoveredMap(showDiscoveredMap);
                }
            }
            ImGui::End();
        }

    private:
        void ReturnToEditor()
        {
            DataReference::IsEditorTestPlay = false;
            Manager::SetScene<EditorScene>();
        }
    };
}
// ※注意
// MapManager::StartDungeon が MapData を生成し、その MapData を MapRenderer::Build が 3D 表示へ変換する。
// そのため「StartDungeon -> Build」の順番を崩すと、描画側が参照するマップがまだ存在しない。
void DungeonScene::Init()
{
    // 1. Scene に常駐する表示/入力オブジェクトを先に用意する。
    AddGameObject<Camera>(0)->SetPlayerEnable(true);
    AddGameObject<Skydorm>(0);
    Skydorm::Load();
    AddGameObject<Player>(1)->SetPosition({ 5.0f,1.0f,0.0f });
    AddGameObject<VisionMaskRenderer>(2);


    MessageLog::Instance().SetVisible(true);
    TrapDatabase::Init();
    LightManager::Instance().Init();

    ItemTableDatabase::Init();

    // 2. UI とダンジョン管理オブジェクトを登録する。
    // TurnManager は DungeonScene 中だけ存在し、MapManager と同じく Scene が所有する。
    AddGameObject<PlayerInventoryUI>(2);
    AddGameObject<ShopUI>(2);
    AddGameObject<FloorTransitionUI>(2);
    AddGameObject<ConfirmWindow>(2);
    if (DataReference::IsEditorTestPlay)
    {
        AddGameObject<TestPlayReturnUI>(2);
    }
    auto* miniMap = AddGameObject<MiniMapRenderer>(2);
    AddGameObject<DungeonEndingUI>(2);
    AddGameObject<TurnManager>(2);
    auto* mapManager = AddGameObject<MapManager>(0);

    // 3. JSON の DungeonData を読み、未識別名などフロア全体に関わる状態を初期化する。
    InitDungeonData();
    ItemIdentificationManager::Instance().Init(
        ItemDatabase::GetAllData(),
        mapManager->GetDungeonData().GetUnidentifiedMode()
    );
    // 4. マップ生成、敵/アイテム配置、プレイヤー初期配置までを MapManager 側で行う。
    mapManager->StartDungeon();

    // 5. 生成済み MapData を 3D モデルとミニマップへ反映する。
    // Renderer は描画 API の責務に留め、マップの意味は MapData/MapManager に残す。
    auto* mapRenderer = AddGameObject<MapRenderer>(1);
    mapRenderer->Build(*mapManager->GetCurrentMap());

    miniMap->Init(mapManager->GetCurrentMap(), 50.0f,50.0f,256,256);

}



void DungeonScene::Update()
{
    // TurnManager も Scene-owned な GameObject として、この中でターン進行を更新する。
    Scene::Update();
}

void DungeonScene::Uninit()
{
    Scene::Uninit();
}
void DungeonScene::InitDungeonData()
{
    // 次に入るダンジョンIDは Title/Editor 側で DataReference にセットされる。
    // 対応する JSON がなければ最低限遊べる default データを作る。
    m_DungeonId = DataReference::NextDungeonId;
    DungeonData dungeon;

    std::string path = m_DungeonId + ".json";

    if (!DungeonDataIO::LoadFromFile(path, dungeon))
    {
        BuildDefaultDungeonData(dungeon);
    }

    if (DataReference::IsEditorTestPlay &&
        DataReference::RandomizeEditorTestPlaySeed &&
        dungeon.UseGenerationSeed())
    {
        dungeon.SetGenerationSeed(dungeon.GetGenerationSeed() ^ DataReference::EditorTestPlaySeedSalt);
    }

    auto* mapManager = GetGameObject<MapManager>();
    mapManager->SetDungeonData(dungeon);
}

void DungeonScene::BuildDefaultDungeonData(DungeonData& dungeon)
{
    dungeon.Clear();
    dungeon.SetDungeonId("default");

    FloorData f{};
    f.width = 100;
    f.height = 100;
    f.itemTableId = "early";
    f.enemyTableId = "early";
    f.trapTableId = "Basic";
    f.maxEnemyCount = 10;
    f.maxItemCount = 10;
    f.maxTrapCount = 10;

    dungeon.AddFloor(f);
}

