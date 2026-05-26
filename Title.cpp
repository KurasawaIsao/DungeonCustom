#include "title.h"
#include "manager.h"
#include "Polygon.h"
#include "DungeonScene.h"
#include "input.h"
#include "camera.h"
#include "Skydorm.h"
#include "player.h"
#include "Object3D.h"
#include "WarpParticle.h"
#include "TitleCharacter.h"
#include "Easing.h"
#include "Polygon.h"
#include "TitleUI.h"
#include "Fade.h"
#include "SceneDataReference.h"
#include "ItemDataBase.h"
#include "ItemTableDataBase.h"
#include "EditorScene.h"
#include "EnemyDatabase.h"
#include "EnemyTableDatabase.h"
#include "audio.h"
#include "UnitManager.h"
#include "TrapDataBase.h"
#include "UIRenderer.h"
#include "UITextRenderer.h"
#include "Renderer.h"
#include <algorithm>
#include <cstring>
#include <filesystem> 
namespace fs = std::filesystem;

namespace
{
    constexpr int TITLE_MENU_DUNGEON = 0;
    constexpr int TITLE_MENU_START = 1;
    constexpr int TITLE_MENU_EDITOR = 2;
    constexpr int TITLE_MENU_COUNT = 3;
    constexpr int DUNGEON_LIST_VISIBLE_COUNT = 5;

    void RestoreMainRenderTarget()
    {
        ID3D11RenderTargetView* rtv = Renderer::GetMainRenderTargetView();
        Renderer::GetDeviceContext()->OMSetRenderTargets(1, &rtv, nullptr);
    }
}

void Title::Init()
{
    TrapDatabase::Init();
    ItemDatabase::Init();
    ItemTableDatabase::Init();
    EnemyDatabase::Init();
    EnemyTableDatabase::Init();
    UnitManager::Instance()->ClearAllAllies();

    AddGameObject<Camera>(0)->SetOtherObjEnable(true);
    AddGameObject<TitleUI>(2);
    m_UIStarted = false;
	AddGameObject<Skydorm>(0);
	Skydorm::Load();

    m_BGM = new Audio;
    m_BGM->Load("Asset\\Sound\\tobiranomukou.wav");
    m_BGM->Play(true);
	auto* warp = AddGameObject<WarpParticle>(1);

    warp->Center = Vector3{ 0.0f, 2.0f, -1.0f  };

	for (int i = 0; i < 60; i++)
	{
		warp->Spawn(warp->Center, Vector4(0.6f, 0.8f, 1.0f, 1.0f));
	}


	// タイトルの土台
    Object3D* ob = AddGameObject<Object3D>(1)->Init("Asset\\Model\\Titleobj.fbx");
    ob->SetScale({ 0.01f,0.01f,0.01f });

    ob->SetRotation({ 0.0f, XM_PIDIV2, 0.0f });  

    AddGameObject<Fade>(2)->StartFadeIn();
    m_EditorScene = false;

    // キャラ1: 中央
    m_Char1 = AddGameObject<TitleCharacter>(1);
    m_Char1->Init("Asset\\Model\\T.fbx", TitleCharacter::AppearType::WalkFromBack, { 0.0f, 2.0f, -2.0f }, "Walk");
    m_Char1->AddAnimation("Walk", "Asset\\Model\\TRun.fbx");
    m_Char1->AddAnimation("Idle", "Asset\\Model\\TIdle.fbx");
    m_Char1->SetEnable(false);

    // キャラ2: 左 (ワープ)
    m_Char2 = AddGameObject<TitleCharacter>(1);
    m_Char2->Init("Asset\\Model\\EnemyModel\\tormasX.fbx", TitleCharacter::AppearType::Warp, { 4.0f, 3.0f, -2.0f }, "Snap");
    m_Char2->SetScale({ 2.0f, 2.0f, 2.0f });
    m_Char2->AddAnimation("Snap", "Asset\\Model\\EnemyModel\\tormasX_Snap.fbx");
    m_Char2->AddAnimation("Idle", "Asset\\Model\\EnemyModel\\tormasX_Idle.fbx");
    m_Char2->SetEnable(false);

    // キャラ3: 右 (水)
    m_Char3 = AddGameObject<TitleCharacter>(1);
    m_Char3->Init("Asset\\Model\\EnemyModel\\kingyo.fbx", TitleCharacter::AppearType::WaterJump, { -4.0f, 2.0f, -2.0f }, "Idle");
    m_Char3->AddAnimation("Idle", "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx");
    m_Char3->SetEnable(false);
    
    RefreshDungeonList();
    if (!m_DungeonFiles.empty()) {
        strcpy_s(m_DungeonIdBuf, sizeof(m_DungeonIdBuf), m_DungeonFiles[m_SelectedDungeonIndex].c_str());
    }

}

void Title::Update()
{
	Scene::Update();
    UpdateTimeline();
    UpdateDungeonSelectFade();
    UpdateDungeonSelectInput();

    if (m_SceneStart)
    {
        if (Manager::GetScene()->GetGameObject<Fade>()->GetState() == State::None && !m_EditorScene)
        {
            m_BGM->Stop();
            Manager::SetScene<DungeonScene>();
        }
        else if (Manager::GetScene()->GetGameObject<Fade>()->GetState() == State::None && m_EditorScene)
        {
            m_BGM->Stop();
            Manager::SetScene<EditorScene>();
        }
       
    }
}
void Title::Draw()
{
    Scene::Draw();
    DrawDungeonSelectUI();
}
void Title::Uninit()
{
    delete m_BGM;
    m_BGM = nullptr;
}
void Title::RefreshDungeonList()
{
    m_DungeonFiles.clear();
    std::string path = "DungeonData\\DungeonContext\\";

    // ダンジョン定義フォルダが無い場合は作成して、次回以降の保存先を確保する。
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }

    // DungeonContext 配下の json を、拡張子なしのダンジョンIDとして一覧化する。
    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.path().extension() == ".json") {
            m_DungeonFiles.push_back(entry.path().stem().string());
        }
    }

    std::sort(m_DungeonFiles.begin(), m_DungeonFiles.end());

    // ファイルが一つも無い場合でも、UIと開始処理が空参照にならないよう仮IDを入れる。
    if (m_DungeonFiles.empty()) {
        m_DungeonFiles.push_back("default_dungeon");
    }

    m_SelectedDungeonIndex = std::clamp(
        m_SelectedDungeonIndex,
        0,
        static_cast<int>(m_DungeonFiles.size()) - 1);
    strcpy_s(m_DungeonIdBuf, sizeof(m_DungeonIdBuf), GetSelectedDungeonName().c_str());
}
void Title::UpdateDungeonSelectFade()
{
    if (!m_UIStarted)
    {
        m_DungeonSelectAlpha = 0.0f;
        return;
    }

    // タイトルメニューが表示開始したあと、約0.35秒かけて自然にフェードインさせる。
    const float fadeSpeed = 1.0f / 0.35f;
    m_DungeonSelectAlpha = std::clamp(m_DungeonSelectAlpha + fadeSpeed * (1.0f / 60.0f), 0.0f, 1.0f);
}

void Title::UpdateDungeonSelectInput()
{
    if (!m_UIStarted || m_SceneStart) return;
    if (m_DungeonSelectAlpha < 0.95f) return;
    if (m_DungeonFiles.empty()) return;

    if (m_DungeonDropdownOpen)
    {
        // ドロップダウン展開中は、上下で候補を移動し Z で確定、X で閉じる。
        if (Input::GetKeyTrigger(VK_UP))
        {
            m_SelectedDungeonIndex = (m_SelectedDungeonIndex + static_cast<int>(m_DungeonFiles.size()) - 1)
                % static_cast<int>(m_DungeonFiles.size());
        }
        if (Input::GetKeyTrigger(VK_DOWN))
        {
            m_SelectedDungeonIndex = (m_SelectedDungeonIndex + 1) % static_cast<int>(m_DungeonFiles.size());
        }
        if (Input::GetKeyTrigger('Z') || Input::GetKeyTrigger(VK_RETURN))
        {
            strcpy_s(m_DungeonIdBuf, sizeof(m_DungeonIdBuf), GetSelectedDungeonName().c_str());
            m_DungeonDropdownOpen = false;
        }
        if (Input::GetKeyTrigger('X'))
        {
            m_DungeonDropdownOpen = false;
        }
        return;
    }

    // 通常時はメニュー項目を選び、ダンジョン欄では左右キーで素早く候補を切り替える。
    if (Input::GetKeyTrigger(VK_UP))
    {
        m_TitleMenuCursor = (m_TitleMenuCursor + TITLE_MENU_COUNT - 1) % TITLE_MENU_COUNT;
    }
    if (Input::GetKeyTrigger(VK_DOWN))
    {
        m_TitleMenuCursor = (m_TitleMenuCursor + 1) % TITLE_MENU_COUNT;
    }
    if (m_TitleMenuCursor == TITLE_MENU_DUNGEON && Input::GetKeyTrigger(VK_LEFT))
    {
        m_SelectedDungeonIndex = (m_SelectedDungeonIndex + static_cast<int>(m_DungeonFiles.size()) - 1)
            % static_cast<int>(m_DungeonFiles.size());
        strcpy_s(m_DungeonIdBuf, sizeof(m_DungeonIdBuf), GetSelectedDungeonName().c_str());
    }
    if (m_TitleMenuCursor == TITLE_MENU_DUNGEON && Input::GetKeyTrigger(VK_RIGHT))
    {
        m_SelectedDungeonIndex = (m_SelectedDungeonIndex + 1) % static_cast<int>(m_DungeonFiles.size());
        strcpy_s(m_DungeonIdBuf, sizeof(m_DungeonIdBuf), GetSelectedDungeonName().c_str());
    }
    if (Input::GetKeyTrigger('R'))
    {
        RefreshDungeonList();
    }

    if (Input::GetKeyTrigger('Z') || Input::GetKeyTrigger(VK_RETURN))
    {
        if (m_TitleMenuCursor == TITLE_MENU_DUNGEON)
        {
            m_DungeonDropdownOpen = true;
        }
        else if (m_TitleMenuCursor == TITLE_MENU_START)
        {
            StartSelectedDungeon();
        }
        else if (m_TitleMenuCursor == TITLE_MENU_EDITOR)
        {
            StartEditorScene();
        }
    }
}

void Title::DrawDungeonSelectUI()
{
    if (!m_UIStarted) return;

    const float panelX = 50.0f;
    const float panelY = 50.0f;
    const float panelW = 410.0f;
    const float panelH = m_DungeonDropdownOpen ? 320.0f : 210.0f;
    const float rowX = panelX + 28.0f;
    const float rowW = panelW - 56.0f;
    const float rowH = 42.0f;
    const float alpha = std::clamp(m_DungeonSelectAlpha, 0.0f, 1.0f);
    const D2D1_COLOR_F white = D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha);
    const D2D1_COLOR_F selectedColor = D2D1::ColorF(1.0f, 0.92f, 0.45f, alpha);

    UIRenderer::DrawWindow(UIRect{ panelX, panelY, panelW, panelH }, XMFLOAT4(1.0f, 1.0f, 1.0f, alpha));

    // 現在フォーカスしている行を矩形で強調して、キーボード操作の対象を分かりやすくする。
    if (!m_DungeonDropdownOpen)
    {
        const float focusY = panelY + 70.0f + rowH * static_cast<float>(m_TitleMenuCursor);
        UIRenderer::DrawSolidRect(
            UIRect{ rowX - 8.0f, focusY - 4.0f, rowW + 16.0f, rowH },
            XMFLOAT4(0.95f, 0.78f, 0.25f, 0.25f * alpha));
    }

    if (m_DungeonDropdownOpen)
    {
        const int fileCount = static_cast<int>(m_DungeonFiles.size());
        const int visibleCount = (std::min)(DUNGEON_LIST_VISIBLE_COUNT, fileCount);
        const int halfVisible = visibleCount / 2;
        int listStart = (std::max)(0, m_SelectedDungeonIndex - halfVisible);
        listStart = (std::min)(listStart, (std::max)(0, fileCount - visibleCount));

        for (int i = 0; i < visibleCount; ++i)
        {
            const int fileIndex = listStart + i;
            const float itemY = panelY + 116.0f + rowH * static_cast<float>(i);
            if (fileIndex == m_SelectedDungeonIndex)
            {
                UIRenderer::DrawSolidRect(
                    UIRect{ rowX - 8.0f, itemY - 4.0f, rowW + 16.0f, rowH },
                    XMFLOAT4(0.95f, 0.78f, 0.25f, 0.35f * alpha));
            }
        }
    }

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8Aligned(
        u8"Dungeon Select",
        panelX,
        panelY + 24.0f,
        panelW,
        38.0f,
        30.0f,
        white,
        DWRITE_TEXT_ALIGNMENT_CENTER);

    const std::string dungeonLine = std::string(m_TitleMenuCursor == TITLE_MENU_DUNGEON && !m_DungeonDropdownOpen ? "> " : "  ")
        + u8"ダンジョン  < " + GetSelectedDungeonName() + " >";
    UITextRenderer::DrawOutlinedTextUtf8(
        dungeonLine,
        rowX,
        panelY + 70.0f,
        rowW,
        rowH,
        25.0f,
        m_TitleMenuCursor == TITLE_MENU_DUNGEON ? selectedColor : white);

    if (m_DungeonDropdownOpen)
    {
        const int fileCount = static_cast<int>(m_DungeonFiles.size());
        const int visibleCount = (std::min)(DUNGEON_LIST_VISIBLE_COUNT, fileCount);
        const int halfVisible = visibleCount / 2;
        int listStart = (std::max)(0, m_SelectedDungeonIndex - halfVisible);
        listStart = (std::min)(listStart, (std::max)(0, fileCount - visibleCount));

        for (int i = 0; i < visibleCount; ++i)
        {
            const int fileIndex = listStart + i;
            std::string line = fileIndex == m_SelectedDungeonIndex ? "> " : "  ";
            line += m_DungeonFiles[fileIndex];
            UITextRenderer::DrawOutlinedTextUtf8(
                line,
                rowX + 18.0f,
                panelY + 116.0f + rowH * static_cast<float>(i),
                rowW - 36.0f,
                rowH,
                24.0f,
                fileIndex == m_SelectedDungeonIndex ? selectedColor : white);
        }
    }
    else
    {
        const std::string startLine = std::string(m_TitleMenuCursor == TITLE_MENU_START ? "> " : "  ") + u8"開始";
        const std::string editorLine = std::string(m_TitleMenuCursor == TITLE_MENU_EDITOR ? "> " : "  ") + u8"マップエディタ";
        UITextRenderer::DrawOutlinedTextUtf8(
            startLine,
            rowX,
            panelY + 112.0f,
            rowW,
            rowH,
            27.0f,
            m_TitleMenuCursor == TITLE_MENU_START ? selectedColor : white);
        UITextRenderer::DrawOutlinedTextUtf8(
            editorLine,
            rowX,
            panelY + 154.0f,
            rowW,
            rowH,
            27.0f,
            m_TitleMenuCursor == TITLE_MENU_EDITOR ? selectedColor : white);
    }

    UITextRenderer::End();
    RestoreMainRenderTarget();
}

void Title::StartSelectedDungeon()
{
    if (m_SceneStart) return;

    // 選択したダンジョン名を、読み込み側が使っている DungeonContext 配下のIDに変換する。
    DataReference::NextDungeonId = "DungeonData\\DungeonContext\\" + GetSelectedDungeonName();
    DataReference::IsEditorTestPlay = false;

    if (auto* fade = Manager::GetScene()->GetGameObject<Fade>())
    {
        fade->StartFadeOut();
    }

    m_EditorScene = false;
    m_SceneStart = true;
}

void Title::StartEditorScene()
{
    if (m_SceneStart) return;

    // タイトルから直接エディタへ移動する場合も、通常プレイ扱いに戻してから遷移する。
    DataReference::IsEditorTestPlay = false;

    if (auto* fade = Manager::GetScene()->GetGameObject<Fade>())
    {
        fade->StartFadeOut();
    }

    m_EditorScene = true;
    m_SceneStart = true;
}

const std::string& Title::GetSelectedDungeonName() const
{
    static const std::string emptyName = "No Files";
    if (m_DungeonFiles.empty()) return emptyName;

    const int index = std::clamp(m_SelectedDungeonIndex, 0, static_cast<int>(m_DungeonFiles.size()) - 1);
    return m_DungeonFiles[index];
}

void Title::UpdateTimeline()
{
    m_TitleTime += 1.0f / 60.0f;

    // 0.0秒：カメラ開始
    if (m_TitleTime >= 0.0f && !m_CameraStarted) {
        auto* cam = Manager::GetScene()->GetGameObject<Camera>();
        cam->StartTitleCamera(Vector3(0, 4, 12), Vector3(0, 2, 6), -5.0f, 15.0f, 3.0f);
        m_CameraStarted = true;
    }

    // 0.0秒：キャラ1開始
    if (m_TitleTime >= 0.0f && !m_Char1Started) {
        if (m_Char1) {
            m_Char1->SetEnable(true);
            m_Char1->PlayAnimation("Walk", 0.5f);
        }
        m_Char1Started = true;
    }

    // 1.2秒：キャラ2開始
    if (m_TitleTime >= 1.2f && !m_Char2Started) {
        if (m_Char2) {
            m_Char2->SetEnable(true);
            m_Char2->Start();
            m_Char2->PlayAnimation("Snap", 0.5f);
        }
        m_Char2Started = true;
    }

    // 2.0秒：キャラ3開始
    if (m_TitleTime >= 2.0f && !m_Char3Started) {
        if (m_Char3) {
            m_Char3->SetEnable(true);
            m_Char3->Start();
            m_Char3->PlayAnimation("Idle", 0.5f);
        }
        m_Char3Started = true;
    }

    // 2.5秒：UI
    if (m_TitleTime >= 2.5f && !m_UIStarted) {
        Manager::GetScene()->GetGameObject<TitleUI>()->Init();
        Manager::GetScene()->GetGameObject<TitleUI>()->Start();
        m_DungeonSelectAlpha = 0.0f;
        m_UIStarted = true;
    }
}



