#include "VisionMaskRenderer.h"
#include "renderer.h"
#include "main.h"
#include "manager.h"
#include "scene.h"
#include "camera.h"
#include "MapManager.h"
#include "MapData.h"
#include "UnitManager.h"
#include "Player.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
    float Distance2D(float ax, float ay, float bx, float by)
    {
        const float dx = ax - bx;
        const float dy = ay - by;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool IsDenseCorridorArea(MapData* map, const Vector2Int& pos)
    {
        if (!map || !map->IsInside(pos) || map->GetTile(pos.x, pos.y) != TileType::Corridor) return false;

        const int mapW = map->GetWidth();
        const int mapH = map->GetHeight();
        std::vector<bool> visited(mapW * mapH, false);
        std::vector<Vector2Int> stack;
        stack.push_back(pos);

        int count = 0;
        int left = pos.x;
        int top = pos.y;
        int right = pos.x + 1;
        int bottom = pos.y + 1;

        static const Vector2Int dirs[4] =
        {
            { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
        };

        while (!stack.empty())
        {
            const Vector2Int current = stack.back();
            stack.pop_back();
            if (!map->IsInside(current)) continue;

            const int index = current.y * mapW + current.x;
            if (visited[index]) continue;
            visited[index] = true;

            if (map->GetTile(current.x, current.y) != TileType::Corridor) continue;
            if (map->GetRoomAt(current) != nullptr) continue;

            ++count;
            left = (std::min)(left, current.x);
            top = (std::min)(top, current.y);
            right = (std::max)(right, current.x + 1);
            bottom = (std::max)(bottom, current.y + 1);

            for (const Vector2Int& dir : dirs)
            {
                const Vector2Int next = current + dir;
                if (!map->IsInside(next)) continue;
                const int nextIndex = next.y * mapW + next.x;
                if (!visited[nextIndex] && map->GetTile(next.x, next.y) == TileType::Corridor)
                {
                    stack.push_back(next);
                }
            }
        }

        const int width = right - left;
        const int height = bottom - top;
        const int area = (std::max)(1, width * height);
        return width >= 3 && height >= 3 && count >= 9 && count * 10 >= area * 6;

        return false;
    }

    bool IsRoomLikeTile(MapData* map, const Vector2Int& pos)
    {
        if (!map || !map->IsInside(pos)) return false;

        const TileType tile = map->GetTile(pos.x, pos.y);
        if (tile == TileType::Floor || tile == TileType::Stair) return true;
        if (tile != TileType::Corridor) return false;

        if (map->GetRoomAt(pos) != nullptr) return true;
        return IsDenseCorridorArea(map, pos);
    }

    Vector3 GridToMaskWorld(const Vector2Int& gridPos)
    {
        // ワープ中はユニットの見た目だけ上空へ動くため、視界中心は安定したグリッド座標から作る。
        return Vector3(
            (float)gridPos.x * (float)TILE_DISTANCE,
            0.0f,
            (float)gridPos.y * (float)TILE_DISTANCE);
    }
}

void VisionMaskRenderer::Init()
{
    pixels.assign(MASK_WIDTH * MASK_HEIGHT, 0x00000000);
    CreateTexture();
    maskPoly.Init(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, nullptr, 1.0f);
}

void VisionMaskRenderer::CreateTexture()
{
    ReleaseTexture();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = MASK_WIDTH;
    desc.Height = MASK_HEIGHT;
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

void VisionMaskRenderer::ReleaseTexture()
{
    if (srv) { srv->Release(); srv = nullptr; }
    if (tex) { tex->Release(); tex = nullptr; }
}

bool VisionMaskRenderer::ShouldDrawMask(int& outViewDistance) const
{
    MapManager* mapManager = MapManager::Instance();
    if (!mapManager || !mapManager->HasMap()) return false;

    const FloorData& floor = mapManager->GetCurrentFloorData();
    if (floor.playerVisionClear) return false;

    UnitManager* unitManager = UnitManager::Instance();
    Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
    MapData* map = mapManager->GetCurrentMap();
    if (!player || !map) return false;

    outViewDistance = (std::max)(0, floor.viewDistance);
    return true;
}

bool VisionMaskRenderer::WorldToScreen(const Vector3& world, float& outX, float& outY) const
{
    Scene* scene = Manager::GetScene();
    Camera* camera = scene ? scene->GetGameObject<Camera>() : nullptr;
    if (!camera) return false;

    XMFLOAT3 position(world.x, world.y, world.z);
    XMVECTOR worldVec = XMLoadFloat3(&position);
    XMMATRIX viewProjection = camera->GetViewMatrix() * camera->GetProjectionMatrix();
    XMVECTOR projected = XMVector3TransformCoord(worldVec, viewProjection);

    XMFLOAT3 ndc{};
    XMStoreFloat3(&ndc, projected);
    if (ndc.z < 0.0f || ndc.z > 1.0f) return false;

    outX = (ndc.x * 0.5f + 0.5f) * SCREEN_WIDTH;
    outY = (-ndc.y * 0.5f + 0.5f) * SCREEN_HEIGHT;
    return true;
}

void VisionMaskRenderer::SetFocusOverride(const Vector2Int& gridPos, const Vector3& worldPos)
{
    focusGridPos = gridPos;
    focusWorldPos = worldPos;
    hasFocusOverride = true;
}

void VisionMaskRenderer::ClearFocusOverride()
{
    hasFocusOverride = false;
}

bool VisionMaskRenderer::GetMaskCircle(const Vector3& focusWorld, int viewDistance, float& outCenterX, float& outCenterY, float& outRadius) const
{
    Vector3 centerWorld = focusWorld;
    centerWorld.y += 1.0f;

    if (!WorldToScreen(centerWorld, outCenterX, outCenterY))
    {
        outCenterX = SCREEN_WIDTH * 0.5f;
        outCenterY = SCREEN_HEIGHT * 0.5f;
    }

    const float radiusWorld = (std::max)(1.0f, (float)viewDistance * (float)TILE_DISTANCE);
    float xEdgeX = 0.0f;
    float xEdgeY = 0.0f;
    float zEdgeX = 0.0f;
    float zEdgeY = 0.0f;

    const bool hasX = WorldToScreen(centerWorld + Vector3(radiusWorld, 0.0f, 0.0f), xEdgeX, xEdgeY);
    const bool hasZ = WorldToScreen(centerWorld + Vector3(0.0f, 0.0f, radiusWorld), zEdgeX, zEdgeY);

    float radius = 48.0f;
    if (hasX) radius = (std::max)(radius, Distance2D(outCenterX, outCenterY, xEdgeX, xEdgeY));
    if (hasZ) radius = (std::max)(radius, Distance2D(outCenterX, outCenterY, zEdgeX, zEdgeY));

    outRadius = radius;
    return true;
}

void VisionMaskRenderer::BuildMask(float centerX, float centerY, float radius)
{
    const float edgeWidth = 24.0f;

    for (int y = 0; y < MASK_HEIGHT; ++y)
    {
        const float screenY = ((float)y + 0.5f) * SCREEN_HEIGHT / MASK_HEIGHT;
        for (int x = 0; x < MASK_WIDTH; ++x)
        {
            const float screenX = ((float)x + 0.5f) * SCREEN_WIDTH / MASK_WIDTH;
            const float dist = Distance2D(screenX, screenY, centerX, centerY);

            unsigned int alpha = MAX_ALPHA;
            if (dist <= radius)
            {
                alpha = 0;
            }
            else if (dist < radius + edgeWidth)
            {
                const float t = (dist - radius) / edgeWidth;
                alpha = (unsigned int)(MAX_ALPHA * t);
            }

            pixels[y * MASK_WIDTH + x] = (alpha << 24);
        }
    }
}

bool VisionMaskRenderer::GetGridAreaScreenRect(int left, int top, int right, int bottom, ScreenRect& outRect) const
{
    const float halfTile = (float)TILE_DISTANCE * 0.5f;
    const float worldLeft = (float)left * (float)TILE_DISTANCE - halfTile;
    const float worldTop = (float)top * (float)TILE_DISTANCE - halfTile;
    const float worldRight = (float)(right - 1) * (float)TILE_DISTANCE + halfTile;
    const float worldBottom = (float)(bottom - 1) * (float)TILE_DISTANCE + halfTile;

    bool hasPoint = false;
    outRect = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f };

    const float xs[] = { worldLeft, worldRight };
    const float zs[] = { worldTop, worldBottom };

    for (float x : xs)
    {
        for (float z : zs)
        {
            float screenX = 0.0f;
            float screenY = 0.0f;
            if (!WorldToScreen(Vector3(x, 0.0f, z), screenX, screenY)) continue;

            outRect.left = (std::min)(outRect.left, screenX);
            outRect.top = (std::min)(outRect.top, screenY);
            outRect.right = (std::max)(outRect.right, screenX);
            outRect.bottom = (std::max)(outRect.bottom, screenY);
            hasPoint = true;
        }
    }

    if (!hasPoint) return false;

    const float padding = 2.0f;
    outRect.left -= padding;
    outRect.top -= padding;
    outRect.right += padding;
    outRect.bottom += padding;

    if (outRect.right < 0.0f || outRect.bottom < 0.0f ||
        outRect.left > (float)SCREEN_WIDTH || outRect.top > (float)SCREEN_HEIGHT)
    {
        return false;
    }

    outRect.left = (std::max)(0.0f, outRect.left);
    outRect.top = (std::max)(0.0f, outRect.top);
    outRect.right = (std::min)((float)SCREEN_WIDTH, outRect.right);
    outRect.bottom = (std::min)((float)SCREEN_HEIGHT, outRect.bottom);
    return outRect.left < outRect.right && outRect.top < outRect.bottom;
}

bool VisionMaskRenderer::GetGridAreaScreenQuad(int left, int top, int right, int bottom, ScreenQuad& outQuad) const
{
    const float halfTile = (float)TILE_DISTANCE * 0.5f;
    const float worldLeft = (float)left * (float)TILE_DISTANCE - halfTile;
    const float worldTop = (float)top * (float)TILE_DISTANCE - halfTile;
    const float worldRight = (float)(right - 1) * (float)TILE_DISTANCE + halfTile;
    const float worldBottom = (float)(bottom - 1) * (float)TILE_DISTANCE + halfTile;

    const Vector3 corners[4] =
    {
        Vector3(worldLeft, 0.0f, worldTop),
        Vector3(worldRight, 0.0f, worldTop),
        Vector3(worldRight, 0.0f, worldBottom),
        Vector3(worldLeft, 0.0f, worldBottom)
    };

    outQuad.bounds = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f };
    for (int i = 0; i < 4; ++i)
    {
        if (!WorldToScreen(corners[i], outQuad.points[i].x, outQuad.points[i].y)) return false;

        outQuad.bounds.left = (std::min)(outQuad.bounds.left, outQuad.points[i].x);
        outQuad.bounds.top = (std::min)(outQuad.bounds.top, outQuad.points[i].y);
        outQuad.bounds.right = (std::max)(outQuad.bounds.right, outQuad.points[i].x);
        outQuad.bounds.bottom = (std::max)(outQuad.bounds.bottom, outQuad.points[i].y);
    }

    const float padding = 2.0f;
    outQuad.bounds.left -= padding;
    outQuad.bounds.top -= padding;
    outQuad.bounds.right += padding;
    outQuad.bounds.bottom += padding;

    if (outQuad.bounds.right < 0.0f || outQuad.bounds.bottom < 0.0f ||
        outQuad.bounds.left > (float)SCREEN_WIDTH || outQuad.bounds.top > (float)SCREEN_HEIGHT)
    {
        return false;
    }

    outQuad.bounds.left = (std::max)(0.0f, outQuad.bounds.left);
    outQuad.bounds.top = (std::max)(0.0f, outQuad.bounds.top);
    outQuad.bounds.right = (std::min)((float)SCREEN_WIDTH, outQuad.bounds.right);
    outQuad.bounds.bottom = (std::min)((float)SCREEN_HEIGHT, outQuad.bounds.bottom);
    return outQuad.bounds.left < outQuad.bounds.right && outQuad.bounds.top < outQuad.bounds.bottom;
}

void VisionMaskRenderer::ClearScreenRectSmooth(const ScreenRect& rect)
{
    const float edgeWidth = 8.0f;
    const float paddedLeft = rect.left - edgeWidth;
    const float paddedTop = rect.top - edgeWidth;
    const float paddedRight = rect.right + edgeWidth;
    const float paddedBottom = rect.bottom + edgeWidth;

    const int left = (std::max)(0, (int)std::floor(paddedLeft * MASK_WIDTH / (float)SCREEN_WIDTH));
    const int top = (std::max)(0, (int)std::floor(paddedTop * MASK_HEIGHT / (float)SCREEN_HEIGHT));
    const int right = (std::min)(MASK_WIDTH, (int)std::ceil(paddedRight * MASK_WIDTH / (float)SCREEN_WIDTH));
    const int bottom = (std::min)(MASK_HEIGHT, (int)std::ceil(paddedBottom * MASK_HEIGHT / (float)SCREEN_HEIGHT));

    for (int y = top; y < bottom; ++y)
    {
        const float screenY = ((float)y + 0.5f) * SCREEN_HEIGHT / MASK_HEIGHT;
        for (int x = left; x < right; ++x)
        {
            const float screenX = ((float)x + 0.5f) * SCREEN_WIDTH / MASK_WIDTH;
            const float outsideX = (std::max)((std::max)(rect.left - screenX, screenX - rect.right), 0.0f);
            const float outsideY = (std::max)((std::max)(rect.top - screenY, screenY - rect.bottom), 0.0f);
            const float outside = (std::max)(outsideX, outsideY);
            if (outside >= edgeWidth) continue;

            const unsigned int alpha = (unsigned int)(MAX_ALPHA * (outside / edgeWidth));
            unsigned int& pixel = pixels[y * MASK_WIDTH + x];
            const unsigned int currentAlpha = pixel >> 24;
            pixel = ((std::min)(currentAlpha, alpha) << 24);
        }
    }
}

void VisionMaskRenderer::ClearScreenQuadSmooth(const ScreenQuad& quad)
{
    const float edgeWidth = 8.0f;
    const float paddedLeft = quad.bounds.left - edgeWidth;
    const float paddedTop = quad.bounds.top - edgeWidth;
    const float paddedRight = quad.bounds.right + edgeWidth;
    const float paddedBottom = quad.bounds.bottom + edgeWidth;

    const int left = (std::max)(0, (int)std::floor(paddedLeft * MASK_WIDTH / (float)SCREEN_WIDTH));
    const int top = (std::max)(0, (int)std::floor(paddedTop * MASK_HEIGHT / (float)SCREEN_HEIGHT));
    const int right = (std::min)(MASK_WIDTH, (int)std::ceil(paddedRight * MASK_WIDTH / (float)SCREEN_WIDTH));
    const int bottom = (std::min)(MASK_HEIGHT, (int)std::ceil(paddedBottom * MASK_HEIGHT / (float)SCREEN_HEIGHT));

    auto cross = [](const ScreenPoint& a, const ScreenPoint& b, const ScreenPoint& p) -> float
        {
            return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        };

    auto contains = [&](const ScreenPoint& p) -> bool
        {
            bool hasPositive = false;
            bool hasNegative = false;
            for (int i = 0; i < 4; ++i)
            {
                const float c = cross(quad.points[i], quad.points[(i + 1) % 4], p);
                hasPositive = hasPositive || c > 0.0f;
                hasNegative = hasNegative || c < 0.0f;
                if (hasPositive && hasNegative) return false;
            }
            return true;
        };

    auto distanceToSegment = [](const ScreenPoint& p, const ScreenPoint& a, const ScreenPoint& b) -> float
        {
            const float vx = b.x - a.x;
            const float vy = b.y - a.y;
            const float wx = p.x - a.x;
            const float wy = p.y - a.y;
            const float lenSq = vx * vx + vy * vy;
            float t = lenSq > 0.0f ? (wx * vx + wy * vy) / lenSq : 0.0f;
            t = (std::max)(0.0f, (std::min)(1.0f, t));

            const float px = a.x + vx * t;
            const float py = a.y + vy * t;
            return Distance2D(p.x, p.y, px, py);
        };

    for (int y = top; y < bottom; ++y)
    {
        const float screenY = ((float)y + 0.5f) * SCREEN_HEIGHT / MASK_HEIGHT;
        for (int x = left; x < right; ++x)
        {
            const float screenX = ((float)x + 0.5f) * SCREEN_WIDTH / MASK_WIDTH;
            const ScreenPoint p{ screenX, screenY };

            unsigned int alpha = 0;
            if (!contains(p))
            {
                float distance = distanceToSegment(p, quad.points[0], quad.points[1]);
                for (int i = 1; i < 4; ++i)
                {
                    distance = (std::min)(distance, distanceToSegment(p, quad.points[i], quad.points[(i + 1) % 4]));
                }

                if (distance >= edgeWidth) continue;
                alpha = (unsigned int)(MAX_ALPHA * (distance / edgeWidth));
            }

            unsigned int& pixel = pixels[y * MASK_WIDTH + x];
            const unsigned int currentAlpha = pixel >> 24;
            pixel = ((std::min)(currentAlpha, alpha) << 24);
        }
    }
}

void VisionMaskRenderer::BuildRoomAndViewMask(MapData* map, const Vector2Int& centerPos, int viewDistance)
{
    std::fill(pixels.begin(), pixels.end(), (MAX_ALPHA << 24));
    if (!map) return;

    const Room* room = map->GetRoomAt(centerPos);

    auto findConnectedRoomBounds = [](MapData* map, const Vector2Int& start, int& outLeft, int& outTop, int& outRight, int& outBottom) -> bool
    {
        if (!IsRoomLikeTile(map, start)) return false;

        const int mapW = map->GetWidth();
        const int mapH = map->GetHeight();
        std::vector<bool> visited(mapW * mapH, false);
        std::vector<Vector2Int> stack;
        stack.push_back(start);

        outLeft = start.x;
        outTop = start.y;
        outRight = start.x + 1;
        outBottom = start.y + 1;

        static const Vector2Int dirs[4] =
        {
            { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
        };

        while (!stack.empty())
        {
            const Vector2Int current = stack.back();
            stack.pop_back();
            if (!map->IsInside(current)) continue;

            const int index = current.y * mapW + current.x;
            if (visited[index]) continue;
            visited[index] = true;

            if (!IsRoomLikeTile(map, current)) continue;

            outLeft = (std::min)(outLeft, current.x);
            outTop = (std::min)(outTop, current.y);
            outRight = (std::max)(outRight, current.x + 1);
            outBottom = (std::max)(outBottom, current.y + 1);

            for (const Vector2Int& dir : dirs)
            {
                const Vector2Int next = current + dir;
                if (!map->IsInside(next)) continue;
                const int nextIndex = next.y * mapW + next.x;
                if (!visited[nextIndex] && IsRoomLikeTile(map, next))
                {
                    stack.push_back(next);
                }
            }
        }

        return true;
    };

    auto clearGridArea = [this](int left, int top, int right, int bottom)
    {
        if (left >= right || top >= bottom) return;

        ScreenQuad quad{};
        if (GetGridAreaScreenQuad(left, top, right, bottom, quad))
        {
            ClearScreenQuadSmooth(quad);
            return;
        }

        for (int y = top; y < bottom; ++y)
        {
            for (int x = left; x < right; ++x)
            {
                ScreenQuad cellQuad{};
                if (GetGridAreaScreenQuad(x, y, x + 1, y + 1, cellQuad))
                {
                    ClearScreenQuadSmooth(cellQuad);
                }
            }
        }
    };

    viewDistance = (std::max)(0, viewDistance);
    const int viewLeft = (std::max)(0, centerPos.x - viewDistance);
    const int viewTop = (std::max)(0, centerPos.y - viewDistance);
    const int viewRight = (std::min)(map->GetWidth(), centerPos.x + viewDistance + 1);
    const int viewBottom = (std::min)(map->GetHeight(), centerPos.y + viewDistance + 1);

    if (room)
    {
        const Vector2Int roomPos = room->GetPosition();
        const Vector2Int roomSize = room->GetSize();
        const int roomLeft = (std::max)(0, roomPos.x);
        const int roomTop = (std::max)(0, roomPos.y);
        const int roomRight = (std::min)(map->GetWidth(), roomPos.x + roomSize.x);
        const int roomBottom = (std::min)(map->GetHeight(), roomPos.y + roomSize.y);

        const int maskLeft = (std::max)(0, roomLeft - viewDistance);
        const int maskTop = (std::max)(0, roomTop - viewDistance);
        const int maskRight = (std::min)(map->GetWidth(), roomRight + viewDistance);
        const int maskBottom = (std::min)(map->GetHeight(), roomBottom + viewDistance);
        clearGridArea(maskLeft, maskTop, maskRight, maskBottom);
    }
    else if (IsRoomLikeTile(map, centerPos))
    {
        int roomLeft = centerPos.x;
        int roomTop = centerPos.y;
        int roomRight = centerPos.x + 1;
        int roomBottom = centerPos.y + 1;
        if (findConnectedRoomBounds(map, centerPos, roomLeft, roomTop, roomRight, roomBottom))
        {
            const int maskLeft = (std::max)(0, roomLeft - viewDistance);
            const int maskTop = (std::max)(0, roomTop - viewDistance);
            const int maskRight = (std::min)(map->GetWidth(), roomRight + viewDistance);
            const int maskBottom = (std::min)(map->GetHeight(), roomBottom + viewDistance);
            clearGridArea(maskLeft, maskTop, maskRight, maskBottom);
        }
    }
    else
    {
        clearGridArea(viewLeft, viewTop, viewRight, viewBottom);
    }
}

void VisionMaskRenderer::ApplyToGPU()
{
    if (!tex) return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(Renderer::GetDeviceContext()->Map(tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    if (!mapped.pData)
    {
        Renderer::GetDeviceContext()->Unmap(tex, 0);
        return;
    }

    for (int y = 0; y < MASK_HEIGHT; ++y)
    {
        std::memcpy(
            (uint8_t*)mapped.pData + y * mapped.RowPitch,
            &pixels[y * MASK_WIDTH],
            MASK_WIDTH * 4
        );
    }

    Renderer::GetDeviceContext()->Unmap(tex, 0);
}

void VisionMaskRenderer::Draw()
{
    int viewDistance = 2;
    if (!ShouldDrawMask(viewDistance)) return;

    MapManager* mapManager = MapManager::Instance();
    UnitManager* unitManager = UnitManager::Instance();
    MapData* map = mapManager ? mapManager->GetCurrentMap() : nullptr;
    Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
    if (!map || !player) return;

    // プレイヤー移動中は開始マスを中心にして、視界マスクの切り替わりを到着後に遅らせる。
    const Vector2Int centerPos = hasFocusOverride ? focusGridPos : player->GetVisionGridPos();
    const Vector3 centerWorld = GridToMaskWorld(centerPos);

    if (map->GetRoomAt(centerPos) != nullptr || IsRoomLikeTile(map, centerPos))
    {
        BuildRoomAndViewMask(map, centerPos, viewDistance);
    }
    else
    {
        float centerX = SCREEN_WIDTH * 0.5f;
        float centerY = SCREEN_HEIGHT * 0.5f;
        float radius = 64.0f;
        GetMaskCircle(centerWorld, viewDistance, centerX, centerY, radius);
        BuildMask(centerX, centerY, radius);
    }
    ApplyToGPU();

    if (!srv) return;

    Renderer::SetDepthEnable(false);
    maskPoly.SetTexture(srv);
    maskPoly.Draw();
    Renderer::SetDepthEnable(true);
}

void VisionMaskRenderer::Uninit()
{
    ReleaseTexture();
    maskPoly.Uninit();
}
