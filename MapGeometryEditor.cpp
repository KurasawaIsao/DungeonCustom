#include "MapGeometryEditor.h"
#include "MapObjectPlacer.h"
#include "imgui.h"

bool MapGeometryEditor::DrawGeometryTab(MapData* map, MapObjectPlacer& placer) {
    if (!map) return false;
    bool needsSync = false;

    // --- リセット ---
    if (ImGui::CollapsingHeader("Map Infrastructure", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Width", &m_NewWidth);
        ImGui::InputInt("Height", &m_NewHeight);
        if (ImGui::Button("Apply & Clear Map", ImVec2(-1, 0))) {
            ResizeMap(&map, placer);
            needsSync = true;
        }
    }

    // --- ブラシ・タイル設定  ---
    if (ImGui::CollapsingHeader("Brush Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        // タイル選択
        if (ImGui::RadioButton("Floor", m_SelectedTileType == TileType::Floor)) m_SelectedTileType = TileType::Floor;
        ImGui::SameLine();
        if (ImGui::RadioButton("Wall", m_SelectedTileType == TileType::Wall)) m_SelectedTileType = TileType::Wall;
        ImGui::SameLine();
        if (ImGui::RadioButton("Corridor", m_SelectedTileType == TileType::Corridor)) m_SelectedTileType = TileType::Corridor;
        ImGui::SameLine();
        if (ImGui::RadioButton("Stair", m_SelectedTileType == TileType::Stair)) m_SelectedTileType = TileType::Stair;

        ImGui::Separator();

        if (ImGui::Button("Fill Perimeter (Wall)", ImVec2(-1, 0))) {
            FillPerimeter(map);
            needsSync = true; // SyncRendererを呼ぶためにtrueを返す
        }
    }

    if (ImGui::Button("Sync Rooms")) {
        UpdateRoomDataFromTiles(map);
        needsSync = true;
    }

    return needsSync;
}
void MapGeometryEditor::FillPerimeter(MapData* map) {
    int w = map->GetWidth();
    int h = map->GetHeight();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // 最端（x=0, y=0, x=w-1, y=h-1）のタイルを壁にする
            if (x == 0 || y == 0 || x == w - 1 || y == h - 1) {
                map->SetTile(x, y, TileType::Wall);
            }
        }
    }
}
// 塗り処理の起点
void MapGeometryEditor::ApplyTile(MapData* map, Vector2Int pos) {
    if (!map->IsInside(pos)) return;
    map->SetTile(pos.x, pos.y, m_SelectedTileType);
}

void MapGeometryEditor::ResizeMap(MapData** mapPtr, MapObjectPlacer& placer) {
    if (m_NewWidth <= 0 || m_NewHeight <= 0) return;

    // 現在のマップ上の全オブジェクトを掃除する
    placer.ClearAllObjects(*mapPtr);
	mapPtr[0]->Reset(m_NewWidth, m_NewHeight);

    mapPtr[0]->SetAllTile(TileType::Floor); 
}

void MapGeometryEditor::UpdateRoomDataFromTiles(MapData* map) {
    int w = map ->GetWidth();
    int h = map->GetHeight();
    std::vector<bool> visited(w * h, false);
    std::vector<Room> detectedRooms;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // 未訪問の床タイルを見つけたら、一つの塊として探索
            if (map->GetTile(x, y) == TileType::Floor && !visited[y * w + x]) {

                int minX = x, maxX = x, minY = y, maxY = y;
                std::vector<Vector2Int> floorTiles;

                // BFS探索
                std::vector<Vector2Int> stack = { {x, y} };
                visited[y * w + x] = true;

                while (!stack.empty()) {
                    Vector2Int curr = stack.back();
                    stack.pop_back();
                    floorTiles.push_back(curr);

                    // 外枠（AABB）用の計算
                    minX = std::min(minX, curr.x); maxX = std::max(maxX, curr.x);
                    minY = std::min(minY, curr.y); maxY = std::max(maxY, curr.y);

                    Vector2Int dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
                    for (auto& d : dirs) {
                        Vector2Int next = { curr.x + d.x, curr.y + d.y };
                        if (map->IsInside(next) && !visited[next.y * w + next.x] &&
                            map->GetTile(next.x, next.y) == TileType::Floor) {
                            visited[next.y * w + next.x] = true;
                            stack.push_back(next);
                        }
                    }
                }

                // 初期状態（全床）を除外（マップの面積の大部分を占める場合は無視）
                int rw = maxX - minX + 1;
                int rh = maxY - minY + 1;
                if (rw >= w - 1 && rh >= h - 1) continue;

                // Roomを作成
                Room newRoom({ minX, minY }, { rw, rh });
                newRoom.isFixed = true;
                newRoom.ClearSubRects(); // 既存の矩形をクリア（※関数がある前提）

                // 【重要】床タイル1枚1枚を1x1のSubRectとして登録
                for (const auto& pos : floorTiles) {
                    newRoom.AddSubRect(pos, { 1, 1 });
                }
                detectedRooms.push_back(newRoom);
            }
        }
    }
    if (detectedRooms.empty()) {
        // マップ全体を包む矩形を設定
        Room wholeMapRoom({ 0, 0 }, { w, h });
        wholeMapRoom.isFixed = true; // エディタ用フラグ

        // 全マスを subRects に登録（床タイルのみを対象にするのが安全）
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                if (map->GetTile(x, y) == TileType::Floor) {
                    wholeMapRoom.AddSubRect({ x, y }, { 1, 1 });
                }
            }
        }

        // subRects が一つでもある（何かしら床がある）なら追加
        if (!wholeMapRoom.GetSubRects().empty()) {
            detectedRooms.push_back(wholeMapRoom);
        }
    }
    map->GetRooms() = detectedRooms;
}