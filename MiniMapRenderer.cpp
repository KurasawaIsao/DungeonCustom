#include "MiniMapRenderer.h"
#include "renderer.h"
#include "MapData.h"
#include "UnitManager.h"
#include "Player.h"
#include "Enemy.h"
#include "Ally.h"
#include "Item.h"
#include "Input.h"
#include "Time.h"
#include "manager.h"
#include "scene.h"
#include "PlayerInventoryUI.h"
#include "ShopUI.h"
#include "MessageLog.h"
#include "Trap.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>

void MiniMapRenderer::Init(MapData* data, int drawX, int drawY, int drawW, int drawH)
{
    m_Map = data;
    m_MapWidth = data ? data->GetWidth() : 0;
    m_MapHeight = data ? data->GetHeight() : 0;

    m_PosX = drawX;
    m_PosY = drawY;
    m_Width = drawW;
    m_Height = drawH;

    ResizeTextureForCurrentMode();
    m_DiscoveredTiles.assign(m_MapWidth * m_MapHeight, false);
    m_DiscoveredRooms.assign(m_Map ? m_Map->GetRooms().size() : 0, false);

    m_MiniMapPoly.Init((float)m_PosX, (float)m_PosY, (float)m_Width, (float)m_Height, nullptr, 1.0f);
    m_LookMapPoly.Init((float)(SCREEN_WIDTH - 560) * 0.5f, (float)(SCREEN_HEIGHT - 560) * 0.5f, 560.0f, 560.0f, nullptr, 1.0f);
    m_BlackOverlayPoly.Init(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, nullptr, 1.0f);
    m_BlackOverlayPoly.SetColor(XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));

    if (!m_ShowFullMap)
    {
        RevealFromPlayer();
    }
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}

void MiniMapRenderer::CreateTexture()
{
    ReleaseTexture();
    CreateMapSampler();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = m_TextureWidth;
    desc.Height = m_TextureHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(Renderer::GetDevice()->CreateTexture2D(&desc, nullptr, &m_Texture)))
    {
        m_Texture = nullptr;
        return;
    }

    if (FAILED(Renderer::GetDevice()->CreateShaderResourceView(m_Texture, nullptr, &m_ShaderResourceView)))
    {
        m_ShaderResourceView = nullptr;
    }
}

void MiniMapRenderer::ReleaseTexture()
{
    if (m_ShaderResourceView) { m_ShaderResourceView->Release(); m_ShaderResourceView = nullptr; }
    if (m_Texture) { m_Texture->Release(); m_Texture = nullptr; }
}

void MiniMapRenderer::CreateMapSampler()
{
    if (m_MapSampler) return;

    // ミニマップはタイル単位で表示するため、にじみ防止用にポイントサンプリングとクランプを使う。
    D3D11_SAMPLER_DESC desc{};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    Renderer::GetDevice()->CreateSamplerState(&desc, &m_MapSampler);
}

void MiniMapRenderer::ReleaseMapSampler()
{
    if (m_MapSampler)
    {
        m_MapSampler->Release();
        m_MapSampler = nullptr;
    }
}

void MiniMapRenderer::ResizeTextureForCurrentMode()
{
    int desiredW = DRAW_CELL_COUNT;
    int desiredH = DRAW_CELL_COUNT;
    if (m_LookMode)
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        if (GetPlayerRoomBounds(left, top, right, bottom))
        {
            const int roomSize = (std::max)(right - left, bottom - top);
            desiredW = (std::max)(DRAW_CELL_COUNT, roomSize);
            desiredH = desiredW;
        }
    }
    else if (m_ShowFullMap)
    {
        desiredW = (std::max)(1, m_MapWidth);
        desiredH = (std::max)(1, m_MapHeight);
    }
    else if (!m_LookMode && m_ShowDiscoveredMap)
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        if (GetDiscoveredBounds(left, top, right, bottom))
        {
            desiredW = (std::max)(1, right - left);
            desiredH = (std::max)(1, bottom - top);
        }
    }

    const bool needsRecreate = (m_Texture == nullptr || m_ShaderResourceView == nullptr || m_TextureWidth != desiredW || m_TextureHeight != desiredH);
    m_TextureWidth = desiredW;
    m_TextureHeight = desiredH;
    m_Pixels.assign(m_TextureWidth * m_TextureHeight, 0);

    if (needsRecreate)
    {
        CreateTexture();
    }
}

void MiniMapRenderer::SetShowFullMap(bool showFull)
{
    if (m_ShowFullMap == showFull) return;

    m_ShowFullMap = showFull;
    if (m_ShowFullMap)
    {
        m_ShowDiscoveredMap = false;
    }
    ResizeTextureForCurrentMode();
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}

void MiniMapRenderer::SetShowDiscoveredMap(bool showDiscovered)
{
    if (m_ShowDiscoveredMap == showDiscovered) return;

    m_ShowDiscoveredMap = showDiscovered;
    if (m_ShowDiscoveredMap)
    {
        m_ShowFullMap = false;
    }
    ResizeTextureForCurrentMode();
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}

void MiniMapRenderer::SetLookMode(bool enabled)
{
    if (m_LookMode == enabled) return;

    m_LookMode = enabled;
    if (m_LookMode)
    {
        UnitManager* unitManager = UnitManager::Instance();
        Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
        m_LookCenter = player ? player->GetGridPos() : Vector2Int(m_MapWidth / 2, m_MapHeight / 2);

        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        if (GetPlayerRoomBounds(left, top, right, bottom))
        {
            
            m_LookCenter = Vector2Int((left + right) / 2, (top + bottom) / 2);
        }
        ClampLookCenterToDiscoveredBounds();
    }
    m_LookPanTimer = 0.0f;
    ResizeTextureForCurrentMode();
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}

void MiniMapRenderer::UpdateLookModeInput()
{
    const bool tabTriggered = Input::GetKeyTrigger(VK_TAB);
    if (tabTriggered)
    {
        if (!m_LookMode)
        {
            Scene* scene = Manager::GetScene();
            PlayerInventoryUI* ui = scene ? scene->GetGameObject<PlayerInventoryUI>() : nullptr;
            ShopUI* shopUi = scene ? scene->GetGameObject<ShopUI>() : nullptr;
            if ((ui && ui->IsAnyMenuOpen()) || (shopUi && shopUi->IsAnyMenuOpen()))
            {
                return;
            }
        }
        SetLookMode(!m_LookMode);
        return;
    }
    if (!m_LookMode) return;

    m_LookPanTimer -= Time::DeltaTime();

    Vector2Int dir(0, 0);
    if (Input::GetKeyPress(VK_UP)) dir.y += 1;
    if (Input::GetKeyPress(VK_DOWN)) dir.y -= 1;
    if (Input::GetKeyPress(VK_LEFT)) dir.x -= 1;
    if (Input::GetKeyPress(VK_RIGHT)) dir.x += 1;

    const bool triggerMove =
        Input::GetKeyTrigger(VK_UP) ||
        Input::GetKeyTrigger(VK_DOWN) ||
        Input::GetKeyTrigger(VK_LEFT) ||
        Input::GetKeyTrigger(VK_RIGHT);

    if ((dir.x != 0 || dir.y != 0) && (triggerMove || m_LookPanTimer <= 0.0f))
    {
        m_LookCenter = m_LookCenter + dir;
        ClampLookCenterToDiscoveredBounds();
        m_LookPanTimer = 0.08f;
    }
}

void MiniMapRenderer::ClampLookCenterToDiscoveredBounds()
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    if (!GetDiscoveredBounds(left, top, right, bottom))
    {
        m_LookCenter.x = (std::max)(0, (std::min)(m_MapWidth - 1, m_LookCenter.x));
        m_LookCenter.y = (std::max)(0, (std::min)(m_MapHeight - 1, m_LookCenter.y));
        return;
    }

    const int halfW = m_TextureWidth / 2;
    const int halfH = m_TextureHeight / 2;

    // 探索済み範囲の一部が画面内に残る範囲で、Tab視点を自由に動かせるようにする。
    const int minX = (std::max)(0, left - halfW);
    const int maxX = (std::min)(m_MapWidth - 1, right - 1 + halfW);
    const int minY = (std::max)(0, top - halfH);
    const int maxY = (std::min)(m_MapHeight - 1, bottom - 1 + halfH);

    m_LookCenter.x = (std::max)(minX, (std::min)(maxX, m_LookCenter.x));
    m_LookCenter.y = (std::max)(minY, (std::min)(maxY, m_LookCenter.y));
}

void MiniMapRenderer::MarkDiscovered(int x, int y)
{
    if (!m_Map || !m_Map->IsInside(x, y)) return;
    m_DiscoveredTiles[y * m_MapWidth + x] = true;
}

bool MiniMapRenderer::IsDiscovered(int x, int y) const
{
    if (!m_Map || !m_Map->IsInside(x, y)) return false;
    if (m_DiscoveredTiles.size() != static_cast<size_t>(m_MapWidth * m_MapHeight)) return false;
    return m_DiscoveredTiles[y * m_MapWidth + x];
}

void MiniMapRenderer::RevealViewArea(const Vector2Int& center, int viewDistance)
{
    viewDistance = (std::max)(0, viewDistance);
    for (int y = center.y - viewDistance; y <= center.y + viewDistance; ++y)
    {
        for (int x = center.x - viewDistance; x <= center.x + viewDistance; ++x)
        {
            MarkDiscovered(x, y);
        }
    }
}

void MiniMapRenderer::RevealRoom(int roomIndex, int viewDistance)
{
    if (!m_Map) return;
    const auto& rooms = m_Map->GetRooms();
    if (roomIndex < 0 || roomIndex >= (int)rooms.size()) return;

    const Room& room = rooms[roomIndex];
    const Vector2Int pos = room.GetPosition();
    const Vector2Int size = room.GetSize();
    viewDistance = (std::max)(0, viewDistance);

    for (int y = pos.y; y < pos.y + size.y; ++y)
    {
        for (int x = pos.x; x < pos.x + size.x; ++x)
        {
            if (room.Contains({ x, y }))
            {
                MarkDiscovered(x, y);
            }
        }
    }

    for (int y = pos.y - viewDistance; y < pos.y + size.y + viewDistance; ++y)
    {
        for (int x = pos.x - viewDistance; x < pos.x + size.x + viewDistance; ++x)
        {
            if (!m_Map->IsInside(x, y) || room.Contains({ x, y })) continue;

            const TileType tile = m_Map->GetTile(x, y);
            if (tile == TileType::Corridor || tile == TileType::Stair)
            {
                MarkDiscovered(x, y);
            }
        }
    }

    if (roomIndex >= (int)m_DiscoveredRooms.size())
    {
        m_DiscoveredRooms.resize(rooms.size(), false);
    }
    m_DiscoveredRooms[roomIndex] = true;
}

void MiniMapRenderer::RevealConnectedCorridors(const Vector2Int& center, int viewDistance)
{
    if (!m_Map || m_DiscoveredTiles.size() != static_cast<size_t>(m_MapWidth * m_MapHeight)) return;

    const int revealRadius = (std::max)(0, viewDistance);
    const int maxSteps = (std::max)(6, viewDistance * 4 + 6);
    std::vector<bool> visited(m_MapWidth * m_MapHeight, false);
    std::vector<std::pair<Vector2Int, int>> queue;

    auto isCorridorLike = [this](const Vector2Int& p) -> bool
    {
        if (!m_Map->IsInside(p)) return false;
        const TileType tile = m_Map->GetTile(p.x, p.y);
        return tile == TileType::Corridor || tile == TileType::Stair;
    };

    auto push = [&](const Vector2Int& p, int step)
    {
        if (!isCorridorLike(p)) return;
        const int index = p.y * m_MapWidth + p.x;
        if (visited[index]) return;
        visited[index] = true;
        queue.push_back({ p, step });
    };

    // 現在見えている通路を起点にして、通路マップに小さな欠けが出ないよう補完する。
    for (int y = center.y - revealRadius; y <= center.y + revealRadius; ++y)
    {
        for (int x = center.x - revealRadius; x <= center.x + revealRadius; ++x)
        {
            push(Vector2Int(x, y), 0);
        }
    }

    static const Vector2Int dirs[4] =
    {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
    };

    for (size_t i = 0; i < queue.size(); ++i)
    {
        const Vector2Int p = queue[i].first;
        const int step = queue[i].second;
        MarkDiscovered(p.x, p.y);
        if (step >= maxSteps) continue;

        for (const Vector2Int& dir : dirs)
        {
            push(p + dir, step + 1);
        }
    }
}

void MiniMapRenderer::RevealFromPlayer()
{
    if (!m_Map) return;

    UnitManager* unitManager = UnitManager::Instance();
    Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
    if (!player) return;

    // 移動演出中は移動開始マスから探索範囲を更新し、到着後に新しい範囲を開く。
    RevealFromPosition(player->GetVisionGridPos(), player->GetViewDistance());
}

void MiniMapRenderer::RevealFromPosition(const Vector2Int& center, int viewDistance)
{
    if (!m_Map) return;

    RevealViewArea(center, viewDistance);
    RevealConnectedCorridors(center, viewDistance);

    const int roomIndex = m_Map->GetRoomIndexAt(center.x, center.y);
    if (roomIndex >= 0)
    {
        RevealRoom(roomIndex, viewDistance);
    }
}

Vector2Int MiniMapRenderer::GetViewportTopLeft() const
{
    if (m_LookMode)
    {
        return { m_LookCenter.x - m_TextureWidth / 2, m_LookCenter.y - m_TextureHeight / 2 };
    }
    if (m_ShowFullMap)
    {
        return { 0, 0 };
    }
    if (m_ShowDiscoveredMap)
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        if (GetDiscoveredBounds(left, top, right, bottom))
        {
            return { left, top };
        }
    }

    Vector2Int center(m_MapWidth / 2, m_MapHeight / 2);

    UnitManager* unitManager = UnitManager::Instance();
    Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
    if (player)
    {
        center = player->GetGridPos();
    }

    return { center.x - m_TextureWidth / 2, center.y - m_TextureHeight / 2 };
}

bool MiniMapRenderer::GetDiscoveredBounds(int& outLeft, int& outTop, int& outRight, int& outBottom) const
{
    if (!m_Map || m_DiscoveredTiles.empty()) return false;
    if (m_DiscoveredTiles.size() != static_cast<size_t>(m_MapWidth * m_MapHeight)) return false;

    int left = m_MapWidth;
    int top = m_MapHeight;
    int right = 0;
    int bottom = 0;
    bool found = false;

    for (int y = 0; y < m_MapHeight; ++y)
    {
        for (int x = 0; x < m_MapWidth; ++x)
        {
            if (!IsDiscovered(x, y)) continue;

            found = true;
            left = (std::min)(left, x);
            top = (std::min)(top, y);
            right = (std::max)(right, x + 1);
            bottom = (std::max)(bottom, y + 1);
        }
    }

    if (!found) return false;

    outLeft = (std::max)(0, left);
    outTop = (std::max)(0, top);
    outRight = (std::min)(m_MapWidth, right);
    outBottom = (std::min)(m_MapHeight, bottom);
    return outLeft < outRight && outTop < outBottom;
}

bool MiniMapRenderer::GetPlayerRoomBounds(int& outLeft, int& outTop, int& outRight, int& outBottom) const
{
    if (!m_Map) return false;

    UnitManager* unitManager = UnitManager::Instance();
    Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
    if (!player) return false;

    const int roomIndex = m_Map->GetRoomIndexAt(player->GetGridPos());
    const auto& rooms = m_Map->GetRooms();
    if (roomIndex < 0 || roomIndex >= (int)rooms.size()) return false;

    const Room& room = rooms[roomIndex];
    const Vector2Int pos = room.GetPosition();
    const Vector2Int size = room.GetSize();

    // Tab表示で部屋端の入口が切れないよう、部屋の周囲に1マス余白を持たせる。
    outLeft = (std::max)(0, pos.x - 1);
    outTop = (std::max)(0, pos.y - 1);
    outRight = (std::min)(m_MapWidth, pos.x + size.x + 1);
    outBottom = (std::min)(m_MapHeight, pos.y + size.y + 1);
    return outLeft < outRight && outTop < outBottom;
}

bool MiniMapRenderer::WorldToViewport(const Vector2Int& worldPos, int& outX, int& outY) const
{
    const Vector2Int topLeft = GetViewportTopLeft();
    outX = worldPos.x - topLeft.x;
    outY = worldPos.y - topLeft.y;
    return outX >= 0 && outX < m_TextureWidth && outY >= 0 && outY < m_TextureHeight;
}

bool MiniMapRenderer::IsShopFloorTile(int x, int y) const
{
    if (!m_Map) return false;

    for (const Room& room : m_Map->GetRooms())
    {
        if (room.m_SpecialType != RoomSpecialType::Shop) continue;

        const Vector2Int pos = room.GetPosition();
        const Vector2Int size = room.GetSize();
        const int innerW = (std::max)(0, size.x - 2);
        const int innerH = (std::max)(0, size.y - 2);
        const int side = (std::min)(innerW, innerH);
        if (side <= 0) continue;

        const int left = pos.x + 1 + (innerW - side) / 2;
        const int top = pos.y + 1 + (innerH - side) / 2;
        if (x >= left && x < left + side && y >= top && y < top + side && room.Contains({ x, y }))
        {
            return true;
        }
    }

    return false;
}

unsigned int MiniMapRenderer::GetTileColor(int x, int y) const
{
    if (!m_Map || ((m_LookMode || !m_ShowFullMap) && !IsDiscovered(x, y))) return 0x00000000;

    const TileType type = m_Map->GetTile(x, y);
    if (type == TileType::Stair)
    {
        return 0xFF00FFFF;
    }
    if (IsShopFloorTile(x, y))
    {
        return 0xFF00AAFF;
    }
    if (m_Map->IsRoomTile(x, y) || m_Map->IsEntranceTile(x, y))
    {
        return 0xFF00FF00;
    }
    if (type == TileType::Corridor)
    {
        return 0xFF888888;
    }

    return 0x00000000;
}

void MiniMapRenderer::BuildStaticLayer()
{
    std::fill(m_Pixels.begin(), m_Pixels.end(), 0x00000000);

    const Vector2Int topLeft = GetViewportTopLeft();
    for (int y = 0; y < m_TextureHeight; ++y)
    {
        for (int x = 0; x < m_TextureWidth; ++x)
        {
            const int mapX = topLeft.x + x;
            const int mapY = topLeft.y + y;
            m_Pixels[y * m_TextureWidth + x] = GetTileColor(mapX, mapY);
        }
    }
}

void MiniMapRenderer::BuildDynamicLayer()
{
    UnitManager* unitManager = UnitManager::Instance();
    if (!unitManager) return;

    Player* player = unitManager->GetPlayer();
    if (m_Map)
    {
        const Vector2Int topLeft = GetViewportTopLeft();
        for (int y = 0; y < m_TextureHeight; ++y)
        {
            for (int x = 0; x < m_TextureWidth; ++x)
            {
                const int mapX = topLeft.x + x;
                const int mapY = topLeft.y + y;
                if (!player || !m_Map->IsInside(mapX, mapY)) continue;
                if (!m_ShowFullMap)
                {
                    if (!IsDiscovered(mapX, mapY)) continue;

                    // アイテムは敵と同じ表示条件にそろえる。
                    const Room* playerRoom = m_Map->GetRoomAt(player->GetGridPos());
                    const Room* itemRoom = m_Map->GetRoomAt(Vector2Int(mapX, mapY));
                    const bool revealSameRoomItemInLookMode = m_LookMode && playerRoom && playerRoom == itemRoom;
                    if (!revealSameRoomItemInLookMode && !player->IsInView(Vector2Int(mapX, mapY))) continue;
                }
                MapObject* obj = m_Map->GetObjectAt(mapX, mapY);

                if (auto* item = dynamic_cast<Item*>(obj))
                {
                    m_Pixels[y * m_TextureWidth + x] = 0xFFFFFF00;
                }
                else if (auto* trap = dynamic_cast<Trap*>(obj))
                {
                    if (trap->IsVisible()) 
                    {
                        m_Pixels[y * m_TextureWidth + x] = 0xFF4000A0;
                    }
                }
            }
        }
    }
    if (player)
    {
        int vx = 0;
        int vy = 0;
        if (WorldToViewport(player->GetGridPos(), vx, vy))
        {
            m_Pixels[vy * m_TextureWidth + vx] = 0xffff0000;
        }
    }

    for (Enemy* enemy : unitManager->GetEnemies())
    {
        if (!enemy || !player) continue;

        enemy->RepairInvalidGridPos("MiniMapRenderer::Enemy");
        const Vector2Int ep = enemy->GetGridPos();
        if (m_Map && !m_Map->IsInBounds(ep)) {
            continue;
        }
        if (!m_ShowFullMap)
        {
            if (!IsDiscovered(ep.x, ep.y)) continue;

            // Tab中は、プレイヤーと同じ部屋にいる敵だけ視界制限を外して表示する。
            const Room* playerRoom = m_Map ? m_Map->GetRoomAt(player->GetGridPos()) : nullptr;
            const Room* enemyRoom = m_Map ? m_Map->GetRoomAt(ep) : nullptr;
            const bool revealSameRoomEnemyInLookMode = m_LookMode && playerRoom && playerRoom == enemyRoom;
            if (!revealSameRoomEnemyInLookMode && !player->IsInView(ep)) continue;
        }

        int vx = 0;
        int vy = 0;
        if (WorldToViewport(ep, vx, vy))
        {
            m_Pixels[vy * m_TextureWidth + vx] = 0xff0000ff;
        }
    }

    for (Ally* ally : unitManager->GetAllies())
    {
        if (!ally || !player) continue;

        ally->RepairInvalidGridPos("MiniMapRenderer::Ally");
        const Vector2Int ap = ally->GetGridPos();
        if (m_Map && !m_Map->IsInBounds(ap)) {
            continue;
        }
        if (!m_ShowFullMap && (!IsDiscovered(ap.x, ap.y) || !player->IsInView(ap))) continue;

        int vx = 0;
        int vy = 0;
        if (WorldToViewport(ap, vx, vy))
        {
            m_Pixels[vy * m_TextureWidth + vx] = 0xffff0000;
        }
    }
}

void MiniMapRenderer::ApplyToGPU()
{
    if (!m_Texture) return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(Renderer::GetDeviceContext()->Map(m_Texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    if (mapped.pData == nullptr)
    {
        Renderer::GetDeviceContext()->Unmap(m_Texture, 0);
        return;
    }

    for (int y = 0; y < m_TextureHeight; ++y)
    {
        const int srcY = m_TextureHeight - 1 - y;
        std::memcpy(
            (uint8_t*)mapped.pData + y * mapped.RowPitch,
            &m_Pixels[srcY * m_TextureWidth],
            m_TextureWidth * 4
        );
    }

    Renderer::GetDeviceContext()->Unmap(m_Texture, 0);
}

void MiniMapRenderer::Update()
{
    UpdateLookModeInput();
    if (!m_ShowFullMap || m_LookMode)
    {
        RevealFromPlayer();
    }
    if (m_ShowDiscoveredMap)
    {
        ResizeTextureForCurrentMode();
    }
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}

void MiniMapRenderer::Draw()
{
    if (m_ShaderResourceView == nullptr) return;

    Renderer::SetDepthEnable(false);

    ID3D11SamplerState* oldSampler = nullptr;
    if (m_MapSampler)
    {
        Renderer::GetDeviceContext()->PSGetSamplers(0, 1, &oldSampler);
        Renderer::GetDeviceContext()->PSSetSamplers(0, 1, &m_MapSampler);
    }

    if (m_LookMode)
    {
        m_BlackOverlayPoly.Draw();
        m_LookMapPoly.SetTexture(m_ShaderResourceView);
        m_LookMapPoly.Draw();
    }
    else
    {
        m_MiniMapPoly.SetTexture(m_ShaderResourceView);
        m_MiniMapPoly.Draw();
    }

    if (m_MapSampler)
    {
        Renderer::GetDeviceContext()->PSSetSamplers(0, 1, &oldSampler);
        if (oldSampler) oldSampler->Release();
    }
    Renderer::SetDepthEnable(true);
}

void MiniMapRenderer::Uninit()
{
    ReleaseTexture();
    ReleaseMapSampler();
    m_MiniMapPoly.Uninit();
    m_LookMapPoly.Uninit();
    m_BlackOverlayPoly.Uninit();
}

void MiniMapRenderer::ResetMap(MapData* newData)
{
    if (!newData) return;

    m_Map = newData;
    m_MapWidth = newData->GetWidth();
    m_MapHeight = newData->GetHeight();

    ResizeTextureForCurrentMode();
    m_DiscoveredTiles.assign(m_MapWidth * m_MapHeight, false);
    m_DiscoveredRooms.assign(m_Map->GetRooms().size(), false);

    if (!m_Texture || !m_ShaderResourceView)
    {
        CreateTexture();
    }

    if (!m_ShowFullMap)
    {
        RevealFromPlayer();
    }
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}
