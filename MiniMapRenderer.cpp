#include "MiniMapRenderer.h"
#include "renderer.h"
#include "MapData.h"
#include "UnitManager.h"
#include "Player.h"
#include "Enemy.h"
#include "Ally.h"
#include "Input.h"
#include "Time.h"
#include "manager.h"
#include "scene.h"
#include "PlayerInventoryUI.h"
#include "ShopUI.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

void MiniMapRenderer::Init(MapData* data, int drawX, int drawY, int drawW, int drawH)
{
    map = data;
    mapW = data ? data->GetWidth() : 0;
    mapH = data ? data->GetHeight() : 0;

    posX = drawX;
    posY = drawY;
    width = drawW;
    height = drawH;

    ResizeTextureForCurrentMode();
    discoveredTiles.assign(mapW * mapH, false);
    discoveredRooms.assign(map ? map->GetRooms().size() : 0, false);

    miniMapPoly.Init((float)posX, (float)posY, (float)width, (float)height, nullptr, 1.0f);
    lookMapPoly.Init((float)(SCREEN_WIDTH - 560) * 0.5f, (float)(SCREEN_HEIGHT - 560) * 0.5f, 560.0f, 560.0f, nullptr, 1.0f);
    blackOverlayPoly.Init(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, nullptr, 1.0f);
    blackOverlayPoly.SetColor(XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));

    if (!showFullMap)
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

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = texW;
    desc.Height = texH;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(Renderer::GetDevice()->CreateTexture2D(&desc, nullptr, &tex)))
    {
        tex = nullptr;
        return;
    }

    if (FAILED(Renderer::GetDevice()->CreateShaderResourceView(tex, nullptr, &srv)))
    {
        srv = nullptr;
    }
}

void MiniMapRenderer::ReleaseTexture()
{
    if (srv) { srv->Release(); srv = nullptr; }
    if (tex) { tex->Release(); tex = nullptr; }
}
void MiniMapRenderer::ResizeTextureForCurrentMode()
{
    int desiredW = DRAW_CELL_COUNT;
    int desiredH = DRAW_CELL_COUNT;
    if (!lookMode && showFullMap)
    {
        desiredW = (std::max)(1, mapW);
        desiredH = (std::max)(1, mapH);
    }
    else if (!lookMode && showDiscoveredMap)
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

    const bool needsRecreate = (tex == nullptr || srv == nullptr || texW != desiredW || texH != desiredH);
    texW = desiredW;
    texH = desiredH;
    pixels.assign(texW * texH, 0);

    if (needsRecreate)
    {
        CreateTexture();
    }
}

void MiniMapRenderer::SetShowFullMap(bool showFull)
{
    if (showFullMap == showFull) return;

    showFullMap = showFull;
    if (showFullMap)
    {
        showDiscoveredMap = false;
    }
    ResizeTextureForCurrentMode();
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}

void MiniMapRenderer::SetShowDiscoveredMap(bool showDiscovered)
{
    if (showDiscoveredMap == showDiscovered) return;

    showDiscoveredMap = showDiscovered;
    if (showDiscoveredMap)
    {
        showFullMap = false;
    }
    ResizeTextureForCurrentMode();
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}

void MiniMapRenderer::SetLookMode(bool enabled)
{
    if (lookMode == enabled) return;

    lookMode = enabled;
    if (lookMode)
    {
        UnitManager* unitManager = UnitManager::Instance();
        Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
        lookCenter = player ? player->GetGridPos() : Vector2Int(mapW / 2, mapH / 2);
        ClampLookCenterToDiscoveredBounds();
    }
    lookPanTimer = 0.0f;
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
        if (!lookMode)
        {
            Scene* scene = Manager::GetScene();
            PlayerInventoryUI* ui = scene ? scene->GetGameObject<PlayerInventoryUI>() : nullptr;
            ShopUI* shopUi = scene ? scene->GetGameObject<ShopUI>() : nullptr;
            if ((ui && ui->IsAnyMenuOpen()) || (shopUi && shopUi->IsAnyMenuOpen()))
            {
                return;
            }
        }
        SetLookMode(!lookMode);
        return;
    }
    if (!lookMode) return;

    lookPanTimer -= Time::DeltaTime();

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

    if ((dir.x != 0 || dir.y != 0) && (triggerMove || lookPanTimer <= 0.0f))
    {
        lookCenter = lookCenter + dir;
        ClampLookCenterToDiscoveredBounds();
        lookPanTimer = 0.08f;
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
        lookCenter.x = (std::max)(0, (std::min)(mapW - 1, lookCenter.x));
        lookCenter.y = (std::max)(0, (std::min)(mapH - 1, lookCenter.y));
        return;
    }

    lookCenter.x = (std::max)(left, (std::min)(right - 1, lookCenter.x));
    lookCenter.y = (std::max)(top, (std::min)(bottom - 1, lookCenter.y));
}

void MiniMapRenderer::MarkDiscovered(int x, int y)
{
    if (!map || !map->IsInside(x, y)) return;
    discoveredTiles[y * mapW + x] = true;
}

bool MiniMapRenderer::IsDiscovered(int x, int y) const
{
    if (!map || !map->IsInside(x, y)) return false;
    if (discoveredTiles.size() != static_cast<size_t>(mapW * mapH)) return false;
    return discoveredTiles[y * mapW + x];
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
    if (!map) return;
    const auto& rooms = map->GetRooms();
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
            if (!map->IsInside(x, y) || room.Contains({ x, y })) continue;

            const TileType tile = map->GetTile(x, y);
            if (tile == TileType::Corridor || tile == TileType::Stair)
            {
                MarkDiscovered(x, y);
            }
        }
    }

    if (roomIndex >= (int)discoveredRooms.size())
    {
        discoveredRooms.resize(rooms.size(), false);
    }
    discoveredRooms[roomIndex] = true;
}

void MiniMapRenderer::RevealFromPlayer()
{
    if (!map) return;

    UnitManager* unitManager = UnitManager::Instance();
    Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
    if (!player) return;

    RevealFromPosition(player->GetGridPos(), player->GetViewDistance());
}

void MiniMapRenderer::RevealFromPosition(const Vector2Int& center, int viewDistance)
{
    if (!map) return;

    RevealViewArea(center, viewDistance);

    const int roomIndex = map->GetRoomIndexAt(center.x, center.y);
    if (roomIndex >= 0)
    {
        RevealRoom(roomIndex, viewDistance);
    }
}

Vector2Int MiniMapRenderer::GetViewportTopLeft() const
{
    if (lookMode)
    {
        return { lookCenter.x - texW / 2, lookCenter.y - texH / 2 };
    }
    if (showFullMap)
    {
        return { 0, 0 };
    }
    if (showDiscoveredMap)
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

    Vector2Int center(mapW / 2, mapH / 2);

    UnitManager* unitManager = UnitManager::Instance();
    Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
    if (player)
    {
        center = player->GetGridPos();
    }

    return { center.x - texW / 2, center.y - texH / 2 };
}

bool MiniMapRenderer::GetDiscoveredBounds(int& outLeft, int& outTop, int& outRight, int& outBottom) const
{
    if (!map || discoveredTiles.empty()) return false;
    if (discoveredTiles.size() != static_cast<size_t>(mapW * mapH)) return false;

    int left = mapW;
    int top = mapH;
    int right = 0;
    int bottom = 0;
    bool found = false;

    for (int y = 0; y < mapH; ++y)
    {
        for (int x = 0; x < mapW; ++x)
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
    outRight = (std::min)(mapW, right);
    outBottom = (std::min)(mapH, bottom);
    return outLeft < outRight && outTop < outBottom;
}

bool MiniMapRenderer::WorldToViewport(const Vector2Int& worldPos, int& outX, int& outY) const
{
    const Vector2Int topLeft = GetViewportTopLeft();
    outX = worldPos.x - topLeft.x;
    outY = worldPos.y - topLeft.y;
    return outX >= 0 && outX < texW && outY >= 0 && outY < texH;
}

bool MiniMapRenderer::IsShopFloorTile(int x, int y) const
{
    if (!map) return false;

    for (const Room& room : map->GetRooms())
    {
        if (room.specialType != RoomSpecialType::Shop) continue;

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
    if (!map || ((lookMode || !showFullMap) && !IsDiscovered(x, y))) return 0x00000000;

    const TileType type = map->GetTile(x, y);
    if (type == TileType::Stair)
    {
        return 0xFF00FFFF;
    }
    if (IsShopFloorTile(x, y))
    {
        return 0xFF00AAFF;
    }
    if (map->IsRoomTile(x, y) || map->IsEntranceTile(x, y))
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
    std::fill(pixels.begin(), pixels.end(), 0x00000000);

    const Vector2Int topLeft = GetViewportTopLeft();
    for (int y = 0; y < texH; ++y)
    {
        for (int x = 0; x < texW; ++x)
        {
            const int mapX = topLeft.x + x;
            const int mapY = topLeft.y + y;
            pixels[y * texW + x] = GetTileColor(mapX, mapY);
        }
    }
}

void MiniMapRenderer::BuildDynamicLayer()
{
    UnitManager* unitManager = UnitManager::Instance();
    if (!unitManager) return;

    Player* player = unitManager->GetPlayer();
    if (player)
    {
        int vx = 0;
        int vy = 0;
        if (WorldToViewport(player->GetGridPos(), vx, vy))
        {
            pixels[vy * texW + vx] = 0xffff0000;
        }
    }

    for (Enemy* enemy : unitManager->GetEnemies())
    {
        if (!enemy || !player) continue;

        enemy->RepairInvalidGridPos("MiniMapRenderer::Enemy");
        const Vector2Int ep = enemy->GetGridPos();
        if (map && !map->IsInBounds(ep)) {
            continue;
        }
        if (!showFullMap && (!IsDiscovered(ep.x, ep.y) || !player->IsInView(ep))) continue;

        int vx = 0;
        int vy = 0;
        if (WorldToViewport(ep, vx, vy))
        {
            pixels[vy * texW + vx] = 0xff0000ff;
        }
    }

    for (Ally* ally : unitManager->GetAllies())
    {
        if (!ally || !player) continue;

        ally->RepairInvalidGridPos("MiniMapRenderer::Ally");
        const Vector2Int ap = ally->GetGridPos();
        if (map && !map->IsInBounds(ap)) {
            continue;
        }
        if (!showFullMap && (!IsDiscovered(ap.x, ap.y) || !player->IsInView(ap))) continue;

        int vx = 0;
        int vy = 0;
        if (WorldToViewport(ap, vx, vy))
        {
            pixels[vy * texW + vx] = 0xffff0000;
        }
    }
}

void MiniMapRenderer::ApplyToGPU()
{
    if (!tex) return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(Renderer::GetDeviceContext()->Map(tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    if (mapped.pData == nullptr)
    {
        Renderer::GetDeviceContext()->Unmap(tex, 0);
        return;
    }

    for (int y = 0; y < texH; ++y)
    {
        const int srcY = texH - 1 - y;
        std::memcpy(
            (uint8_t*)mapped.pData + y * mapped.RowPitch,
            &pixels[srcY * texW],
            texW * 4
        );
    }

    Renderer::GetDeviceContext()->Unmap(tex, 0);
}

void MiniMapRenderer::Update()
{
    UpdateLookModeInput();
    if (!showFullMap || lookMode)
    {
        RevealFromPlayer();
    }
    if (showDiscoveredMap)
    {
        ResizeTextureForCurrentMode();
    }
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}

void MiniMapRenderer::Draw()
{
    if (srv == nullptr) return;

    Renderer::SetDepthEnable(false);
    if (lookMode)
    {
        blackOverlayPoly.Draw();
        lookMapPoly.SetTexture(srv);
        lookMapPoly.Draw();
    }
    else
    {
        miniMapPoly.SetTexture(srv);
        miniMapPoly.Draw();
    }
    Renderer::SetDepthEnable(true);
}

void MiniMapRenderer::Uninit()
{
    ReleaseTexture();
    miniMapPoly.Uninit();
    lookMapPoly.Uninit();
    blackOverlayPoly.Uninit();
}

void MiniMapRenderer::ResetMap(MapData* newData)
{
    if (!newData) return;

    map = newData;
    mapW = newData->GetWidth();
    mapH = newData->GetHeight();

    ResizeTextureForCurrentMode();
    discoveredTiles.assign(mapW * mapH, false);
    discoveredRooms.assign(map->GetRooms().size(), false);

    if (!tex || !srv)
    {
        CreateTexture();
    }

    if (!showFullMap)
    {
        RevealFromPlayer();
    }
    BuildStaticLayer();
    BuildDynamicLayer();
    ApplyToGPU();
}
