#include "MapEditor.h"
#include "Manager.h"
#include "Scene.h"
#include "Title.h"
#include "renderer.h"
#include "camera.h"
#include "scene.h"
#include "RayUtil.h"
#include "Item.h"
#include "MapRenderer.h"
#include "ItemDataBase.h"
#include "ItemTableDataBase.h"
#include "EnemyTableDatabase.h"
#include "DungeonDataIO.h"
#include "input.h"
#include "LightManager.h"
#include "TrapDataBase.h"
#include "ItemDataBase.h"
#include "Trap.h"
#include "Shrine.h"
#include "DungeonScene.h"
#include "SceneDataReference.h"
#include <ctime>


namespace fs = std::filesystem;

namespace
{
    std::string GetDungeonIdFromFileName(const std::string& fileName)
    {
        fs::path path(fileName);
        return path.stem().string();
    }
}

void MapEditor::Init() {
    LightManager::Instance().Init();
    ItemDatabase::Init();
    TrapDatabase::Init();
    m_ObjectPlacer.InitializeSelection();
    m_Map = new MapData(50, 50);
    m_Map->SetAllTile(TileType::Floor);
    RefreshFileList();
    SyncTestDungeonSelection(true);
    SyncRenderer();
}

void MapEditor::Uninit() {
    m_ObjectPlacer.ClearAllObjects(m_Map);
    delete m_Map;
}

void MapEditor::Update() {
    if (Input::GetKeyTrigger(VK_ESCAPE)) m_Enabled = !m_Enabled;
    if (!m_Enabled) return;

    UpdateHover();
    HandleMouse();
}

void MapEditor::Draw()
{
}

void MapEditor::DrawImGui() {
    if (!m_Enabled) return;

    // ウィンドウ1: マップ設計
    DrawMapEditorWindow();

    // ウィンドウ2: ダンジョン階層設計
    DrawDungeonEditorWindow();
}

// --- [ウィンドウ1] マップ・タイル・アイテム配置 ---
void MapEditor::DrawMapEditorWindow() {
    ImGui::SetNextWindowPos(ImVec2(800, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(450, 700), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Map Design Editor", &m_Enabled)) {
        if (ImGui::BeginTabBar("MapTabs")) {

            if (ImGui::BeginTabItem("Edit")) {
                // モード切替
                if (ImGui::RadioButton("Geometry Mode", (int*)&m_EditCategory, (int)EditCategory::Geometry)) {}
                ImGui::SameLine();
                if (ImGui::RadioButton("Objects Mode", (int*)&m_EditCategory, (int)EditCategory::Objects)) {}

                ImGui::Separator();

                if (m_EditCategory == EditCategory::Geometry) {
                    // 地形編集。戻り値がtrueならSyncRenderer
                    if (m_GeometryEditor.DrawGeometryTab(m_Map, m_ObjectPlacer)) {
                        SyncRenderer();
                    }
                }
                else {
                    ImGui::RadioButton("Item", (int*)&m_ObjectMode, (int)EditorPlaceMode::Item);
                    ImGui::SameLine();
                    ImGui::RadioButton("Trap", (int*)&m_ObjectMode, (int)EditorPlaceMode::Trap);
                    ImGui::SameLine();
                    ImGui::RadioButton("Shrine", (int*)&m_ObjectMode, (int)EditorPlaceMode::Shrine);
                    ImGui::Separator();

                    // オブジェクト配置。m_ObjectModeを渡す
                    m_ObjectPlacer.DrawPlacerTab(m_Map, m_ObjectMode);
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0, 1, 1, 1), "Room Layout Preview");
                DrawRoomPreviewUI();

                ImGui::EndTabItem();
            }

            // --- Map File ---
            if (ImGui::BeginTabItem("Map File")) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Full Map");
                ImGui::InputText("Full Map File Name", m_MapFileName, 128);
                if (ImGui::Button("Export Full Map JSON", ImVec2(-1, 0))) ExportMap(ExportDirectory::MapData);

                if (ImGui::BeginCombo("Import Full Map", "Select...")) {
                    for (auto& file : m_MapFileList) {
                        if (ImGui::Selectable(file.c_str())) {
                            strncpy_s(m_MapFileName, file.c_str(), 128);
                            ImportMap(GetMapPath(file, ExportDirectory::MapData));
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Room Part");
                ImGui::InputText("Room Part File Name", m_RoomPartFileName, 128);
                if (ImGui::Button("Export Room Part JSON", ImVec2(-1, 0))) ExportMap(ExportDirectory::RoomData);

                if (ImGui::BeginCombo("Import Room Part", "Select...")) {
                    for (auto& file : m_RoomPartFileList) {
                        if (ImGui::Selectable(file.c_str())) {
                            strncpy_s(m_RoomPartFileName, file.c_str(), 128);
                            ImportMap(GetMapPath(file, ExportDirectory::RoomData));
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tables")) {
				m_SpawnTableEditor.DrawTableEditorTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();

            DrawTestPlayControls();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Return to Title", ImVec2(-1, 30))) {
                Manager::SetScene<Title>();
            }
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

// --- [ウィンドウ2] ダンジョン構造設計 ---
void MapEditor::DrawDungeonEditorWindow() {
    m_DungeonEditor.DrawDungeonEditorWindow(
        m_Dungeon,
        m_Enabled,
        m_DungeonID,
        m_DungeonFileList,
        m_MapFileList,
        m_RoomPartFileList
    );
}

void MapEditor::UpdateHover()
{
    HWND hwnd = Renderer::GetWindowHandle();
    if (!hwnd) return;

    POINT mouse;
    GetCursorPos(&mouse);
    ScreenToClient(hwnd, &mouse);

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;
    if (clientW <= 0 || clientH <= 0) return;

    Camera* cam = Manager::GetScene()->GetGameObject<Camera>();
    if (!cam) return;

    RayUtil::Ray ray = RayUtil::ScreenPointToRay(
        mouse.x, mouse.y,
        clientW, clientH,
        cam->GetViewMatrix(),
        cam->GetProjectionMatrix()
    );

    if (fabs(ray.dir.y) < 1e-6f) return;
    float t = -ray.origin.y / ray.dir.y;
    if (t < 0) return;

    Vector3 hit = ray.origin + ray.dir * t;

    int tx = (int)floor((hit.x + TILE_DISTANCE * 0.5f) / TILE_DISTANCE);
    int ty = (int)floor((hit.z + TILE_DISTANCE * 0.5f) / TILE_DISTANCE);

    if (m_Map->IsInside({ tx, ty })) {
        m_HoverTile = { tx, ty };
    }
    else {
        m_HoverTile = { -1, -1 };
    }
}
// --- マウス入力によるタイル・アイテム設置 ---
void MapEditor::HandleMouse() {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (!m_Map->IsInside(m_HoverTile)) return;

    // --- 地形編集カテゴリの場合 ---
    if (m_EditCategory == EditCategory::Geometry) {
        bool changed = false;

        // 左クリック：GeometryEditorに配置を依頼
        if (ImGui::IsMouseDown(0)) {
            // ApplyTile内部で FillPermitter 判定が行われます
            m_GeometryEditor.ApplyTile(m_Map, m_HoverTile);
            changed = true;
        }
        // 右クリック：消しゴム（床に戻す）
        else if (ImGui::IsMouseDown(1)) {
            if (m_Map->GetTile(m_HoverTile.x, m_HoverTile.y) != TileType::Floor) {
                m_Map->SetTile(m_HoverTile.x, m_HoverTile.y, TileType::Floor);
                changed = true;
            }
        }

        if (changed) SyncRenderer();
    }
    // --- オブジェクト配置カテゴリの場合 ---
    else if (m_EditCategory == EditCategory::Objects) {
        // 配置対象（アイテム・罠・祠）に応じて分岐
        if (m_ObjectMode == EditorPlaceMode::Item) {
            if (ImGui::IsMouseDown(0)) m_ObjectPlacer.PlaceItem(m_Map, m_HoverTile.x, m_HoverTile.y);
            if (ImGui::IsMouseDown(1)) m_ObjectPlacer.RemoveObjectAt(m_Map, m_HoverTile.x, m_HoverTile.y);
        }
        else if (m_ObjectMode == EditorPlaceMode::Trap) {
            if (ImGui::IsMouseDown(0)) m_ObjectPlacer.PlaceTrap(m_Map, m_HoverTile.x, m_HoverTile.y);
            if (ImGui::IsMouseDown(1)) m_ObjectPlacer.RemoveObjectAt(m_Map, m_HoverTile.x, m_HoverTile.y);
        }
        else if (m_ObjectMode == EditorPlaceMode::Shrine) {
            if (ImGui::IsMouseDown(0)) m_ObjectPlacer.PlaceShrine(m_Map, m_HoverTile.x, m_HoverTile.y);
            if (ImGui::IsMouseDown(1)) m_ObjectPlacer.RemoveObjectAt(m_Map, m_HoverTile.x, m_HoverTile.y);
        }
    }
}
void MapEditor::DrawRoomPreviewUI() {
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImVec2(200, 200);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // 背景
    draw_list->AddRectFilled(canvas_pos, { canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y }, IM_COL32(35, 35, 35, 255));

    int mapW = m_Map->GetWidth();
    int mapH = m_Map->GetHeight();
    float scaleX = canvas_size.x / (float)mapW;
    float scaleY = canvas_size.y / (float)mapH;

    auto& rooms = m_Map->GetRooms();
    for (int i = 0; i < (int)rooms.size(); i++) {
        // 各部屋の SubRects (1x1のタイル集合) をループ
        for (const auto& rect : rooms[i].GetSubRects()) {

            // --- 上下反転 ---
            int drawY = (mapH - 1) - rect.pos.y;

            ImVec2 p1 = {
                canvas_pos.x + rect.pos.x * scaleX,
                canvas_pos.y + drawY * scaleY
            };
            ImVec2 p2 = {
                p1.x + rect.size.x * scaleX,
                p1.y + rect.size.y * scaleY
            };

            // 1マスずつ黄色い枠で描画
            draw_list->AddRect(p1, p2, IM_COL32(255, 255, 0, 255));
        }

        // 部屋番号の表示（反転した座標に合わせる）
        Vector2Int rPos = rooms[i].GetPosition();
        Vector2Int rSize = rooms[i].GetSize();
        int textY = (mapH - 1) - (rPos.y + rSize.y - 1);
        char buf[16]; sprintf_s(buf, "R%d", i);
        draw_list->AddText({ canvas_pos.x + rPos.x * scaleX + 2, canvas_pos.y + textY * scaleY + 2 }, IM_COL32(255, 255, 0, 255), buf);
    }

    ImGui::Dummy(canvas_size);
}
// --- マップ描画の更新 ---
void MapEditor::SyncRenderer()
{
    MapRenderer* renderer = Manager::GetScene()->GetGameObject<MapRenderer>();
    if (renderer && m_Map) {
        renderer->Build(*m_Map);
    }
}
std::string MapEditor::GetMapPath(const std::string& name, ExportDirectory directory) const {
    // 拡張子 .json が付いていない場合は付与するガード
    std::string fileName = name;
    if (fileName.find(".json") == std::string::npos) {
        fileName += ".json";
    }

    std::string folder = (directory == ExportDirectory::MapData)
        ? "DungeonData\\EdittedMapData\\"
        : "DungeonData\\EdittedRoomData\\";

    return folder + fileName;
}

void MapEditor::RefreshFileList() {
    auto ScanDir = [](const std::string& path, std::vector<std::string>& list) {
        list.clear();
        if (!fs::exists(path)) {
            fs::create_directories(path);
            return;
        }
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.path().extension() == ".json") {
                list.push_back(entry.path().filename().string());
            }
        }
        };

    // 固定マップと部屋パーツのリストを別々に更新する
    ScanDir("DungeonData\\EdittedMapData\\", m_MapFileList);
    ScanDir("DungeonData\\EdittedRoomData\\", m_RoomPartFileList);
    ScanDir("DungeonData\\DungeonContext\\", m_DungeonFileList);
    SyncTestDungeonSelection(false);
}
void MapEditor::ExportMap(ExportDirectory directory) {
    // 1. タイルから最新の部屋データを同期
    m_GeometryEditor.UpdateRoomDataFromTiles(m_Map);

    // 2. MapEditorが管理しているパスを取得
    const char* fileName = (directory == ExportDirectory::MapData) ? m_MapFileName : m_RoomPartFileName;
    std::string path = GetMapPath(fileName, directory);

    // 3. Serializerに「純粋なデータ」を渡して丸投げ
    if (m_FileManager.ExportMap(path, m_Map, m_ObjectPlacer.GetPlacedObjects())) {
        // 4. 成功したらUIのリストを更新
        RefreshFileList();
    }
}

void MapEditor::ImportMap(const std::string& path) {
    // Serializerに読み込みを丸投げ
    if (m_FileManager.ImportMap(path, m_Map, m_ObjectPlacer)) {
        // 描画情報を同期
        SyncRenderer();
    }
}
std::string MapEditor::GetDungeonPath() const
{
    std::string dungeonId = m_DungeonID;
    if (dungeonId.empty()) dungeonId = "NewDungeon";
    if (dungeonId.find(".json") == std::string::npos) dungeonId += ".json";
    return "DungeonData\\DungeonContext\\" + dungeonId;
}

std::string MapEditor::GetSelectedTestDungeonPath() const
{
    std::string dungeonId = m_TestDungeonID;
    if (dungeonId.empty() && !m_DungeonFileList.empty())
    {
        int index = m_SelectedTestDungeonIndex;
        if (index < 0 || index >= (int)m_DungeonFileList.size()) index = 0;
        dungeonId = GetDungeonIdFromFileName(m_DungeonFileList[index]);
    }
    if (dungeonId.empty()) dungeonId = m_DungeonID;
    if (dungeonId.find(".json") == std::string::npos) dungeonId += ".json";
    return "DungeonData\\DungeonContext\\" + dungeonId;
}

void MapEditor::SyncTestDungeonSelection(bool force)
{
    if (m_DungeonFileList.empty())
    {
        m_SelectedTestDungeonIndex = 0;
        if (force) m_TestDungeonID[0] = '\0';
        return;
    }

    std::string currentId = m_TestDungeonID;
    for (int i = 0; i < (int)m_DungeonFileList.size(); ++i)
    {
        if (GetDungeonIdFromFileName(m_DungeonFileList[i]) == currentId)
        {
            m_SelectedTestDungeonIndex = i;
            return;
        }
    }

    if (!force && !currentId.empty()) return;

    if (m_SelectedTestDungeonIndex < 0 || m_SelectedTestDungeonIndex >= (int)m_DungeonFileList.size())
        m_SelectedTestDungeonIndex = 0;

    const std::string selectedId = GetDungeonIdFromFileName(m_DungeonFileList[m_SelectedTestDungeonIndex]);
    strncpy_s(m_TestDungeonID, sizeof(m_TestDungeonID), selectedId.c_str(), _TRUNCATE);
}

void MapEditor::DrawTestPlayControls()
{
    ImGui::SeparatorText("Test Play");

    const char* preview = m_TestDungeonID[0] ? m_TestDungeonID : "No Files";
    if (ImGui::BeginCombo("Target ID", preview))
    {
        for (int i = 0; i < (int)m_DungeonFileList.size(); ++i)
        {
            const std::string id = GetDungeonIdFromFileName(m_DungeonFileList[i]);
            const bool selected = (m_SelectedTestDungeonIndex == i && id == m_TestDungeonID);
            if (ImGui::Selectable(id.c_str(), selected))
            {
                m_SelectedTestDungeonIndex = i;
                strncpy_s(m_TestDungeonID, sizeof(m_TestDungeonID), id.c_str(), _TRUNCATE);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Refresh Dungeons", ImVec2(-1, 0)))
    {
        RefreshFileList();
    }

    ImGui::InputText("Current ID", m_TestDungeonID, sizeof(m_TestDungeonID));
    ImGui::Checkbox("Randomize Test Seed", &m_RandomizeTestPlaySeed);

    if (ImGui::Button("Test Play", ImVec2(-1, 30)))
    {
        StartTestPlay();
    }
}
std::string MapEditor::GetTestMapPath() const
{
    std::string fileName = m_MapFileName;
    if (fileName.empty()) fileName = "map01.json";
    if (fileName.find(".json") == std::string::npos) fileName += ".json";
    return "DungeonData\\EdittedMapData\\" + fileName;
}

void MapEditor::StartTestPlay()
{
    fs::create_directories("DungeonData\\DungeonContext");
    SyncTestDungeonSelection(false);

    const std::string dungeonPath = GetSelectedTestDungeonPath();
    if (!fs::exists(dungeonPath)) return;

    DataReference::NextDungeonId = dungeonPath.substr(0, dungeonPath.find_last_of('.'));
    DataReference::IsEditorTestPlay = true;
    DataReference::RandomizeEditorTestPlaySeed = m_RandomizeTestPlaySeed;
    DataReference::EditorTestPlaySeedSalt = m_RandomizeTestPlaySeed
        ? static_cast<int>(std::time(nullptr) ^ (std::clock() << 1))
        : 0;
    Manager::SetScene<DungeonScene>();
}