#include "main.h"
#include "GameRandom.h"
#include "manager.h"
#include "renderer.h"
#include "input.h"
#include "scene.h"
#include "title.h"
#include "audio.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "MessageLog.h"
#include "MapManager.h"
#include "Time.h"
#include <ctime>
#include "TitleUI.h"
#include "DungeonScene.h"
#include "EditorScene.h"
#include "PlayerInventoryUI.h"
#include "MapEditor.h"
#include "MiniMapRenderer.h"
#include "ImguiObject.h"
#include "GameUIObject.h"
#include "EnemyModelManager.h"
#include "UIRenderer.h"
#include "UITextRenderer.h"

// Manager は「現在どの Scene を動かすか」を管理するゲーム全体の司令塔。
// main.cpp のメインループから毎フレーム呼ばれ、Input -> Scene 更新 -> Scene 描画 -> ImGui 描画の順で進める。
// 個別のゲームルールは Scene や GameObject 側に置き、Manager には共通の初期化/終了/Scene 切替だけを集約する。

Scene* Manager::m_scene = nullptr;
Scene* Manager::m_Nextscene = nullptr;
ImFont* Manager::fontDebug;
ImFont* Manager::fontLog;

void Manager::Init()
{
    GameRandom::Reseed();

    // 起動直後はタイトル Scene から開始する。
    // DungeonScene や EditorScene への遷移は Manager::SetScene で m_Nextscene に予約される。
	Input::Init();
	Audio::InitMaster();
    //InitImGuiFonts();
	m_scene = new Title();
	m_scene->Init();

    Renderer::InitCommonShader();
    UIRenderer::Init();
    UITextRenderer::Init();


}
void Manager::InitImGuiFonts()
{
    ImGuiIO& io = ImGui::GetIO();

    fontDebug = io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/meiryo.ttc", 20.0f, nullptr, io.Fonts->GetGlyphRangesJapanese()
    );

    fontLog = io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/meiryo.ttc", 36.0f, nullptr, io.Fonts->GetGlyphRangesJapanese()
    );
    io.Fonts->Build();
}

void Manager::Uninit()
{
	m_scene->Uninit();
	delete m_scene;
    EnemyModelManager::Instance().Uninit();
	Input::Uninit();
	Audio::UninitMaster();
    UITextRenderer::Uninit();
    UIRenderer::Uninit();
	Renderer::Uninit();
}

void Manager::Update()
{
    // 入力は Scene より先に更新する。
    // これにより各 GameObject は同じフレーム内の最新入力状態を参照できる。
	Input::Update();

    MessageLog::Instance().Update(Time::DeltaTime());
	
	m_scene->Update();
	
}

void Manager::Draw()
{
    // Scene の 3D/2D 描画を先に行い、その後で ImGui のデバッグUIやログを重ねる。
    Renderer::Begin();

    m_scene->Draw();

    ID3D11RenderTargetView* rtv = Renderer::GetMainRenderTargetView();
    Renderer::GetDeviceContext()->OMSetRenderTargets(1, &rtv, nullptr);

    bool miniMapLookMode = false;
    if (m_scene)
    {
        if (auto* miniMap = m_scene->GetGameObject<MiniMapRenderer>())
        {
            miniMapLookMode = miniMap->IsLookMode();
        }
    }

    if (!miniMapLookMode && MessageLog::Instance().IsVisible())
    {
        MessageLog::Instance().Draw("Log");
    }

    if (!miniMapLookMode)
    {
        auto gameUIObjects = GetScene()->GetGameObjects<GameUIObject>();
        for (auto* ui : gameUIObjects)
        {
            ui->DrawGameUI();
        }
    }

    Drawgui();

    ImGui::PushFont(GetDebugFont());

    if (!miniMapLookMode)
    {
        auto objects = GetScene()->GetGameObjects<ImGuiObject>();
        for (auto* obj : objects)
        {
            obj->DrawImGui();
        }
    }

    ImGui::PopFont();
    //メッセージログ

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    Renderer::End();

    if (m_Nextscene != nullptr)
    {
        // Scene 切替は Draw の最後にまとめて行う。
        // Update 中に Scene を破棄すると、処理中の GameObject 参照が壊れるため。
        m_scene->Uninit();
        delete m_scene;
        m_scene = m_Nextscene;
        m_scene->Init();
        m_Nextscene = nullptr;
    }
}

void Manager::Drawgui()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

}
void Manager::DrawOutlinedText(
    ImDrawList* dl,
    const ImVec2& pos,
    ImU32 color,
    const char* text)
{
    const ImU32 outline = IM_COL32(0, 0, 0, 255);

    dl->AddText(ImVec2(pos.x - 1, pos.y), outline, text);
    dl->AddText(ImVec2(pos.x + 1, pos.y), outline, text);
    dl->AddText(ImVec2(pos.x, pos.y - 1), outline, text);
    dl->AddText(ImVec2(pos.x, pos.y + 1), outline, text);
    dl->AddText(pos, color, text);
}

void Manager::DrawGaugeBar(
    ImDrawList* dl,
    const ImVec2& pos,
    const ImVec2& size,
    float rate,
    ImU32 backColor,
    ImU32 fillColor,
    ImU32 frameColor)
{
    rate = Vector3::Clamp(rate, 0.0f, 1.0f);

    dl->AddRectFilled(
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        backColor
    );

    dl->AddRectFilled(
        pos,
        ImVec2(pos.x + size.x * rate, pos.y + size.y),
        fillColor
    );

    dl->AddRect(
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        frameColor
    );
}
