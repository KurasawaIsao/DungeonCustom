#include "MapRenderer.h"
#include "modelRenderer.h"
#include "renderer.h"
#include "LightManager.h"
#include "input.h"
#include <algorithm>
#include <cmath>
#include <cstring>
ModelRenderer* MapRenderer::s_FloorModel = nullptr;
ModelRenderer* MapRenderer::s_WallModel = nullptr;
ModelRenderer* MapRenderer::s_StairModel = nullptr;
ModelRenderer* MapRenderer::s_CorridorModel= nullptr;
ModelRenderer* MapRenderer::s_EditorCorridorModel = nullptr;
ModelRenderer* MapRenderer::s_ShopFloorModel = nullptr;
ID3D11VertexShader* MapRenderer::s_GridVertexShader = nullptr;
ID3D11PixelShader* MapRenderer::s_GridPixelShader = nullptr;
ID3D11InputLayout* MapRenderer::s_GridVertexLayout = nullptr;
void MapRenderer::Init()
{
    if (!s_FloorModel)
    {
        s_FloorModel = new ModelRenderer();
        s_FloorModel->Load("Asset\\Model\\Floor.obj");
    }

    if (!s_WallModel)
    {
        s_WallModel = new ModelRenderer();
        s_WallModel->Load("Asset\\Model\\Wall.obj");
    }

    if (!s_StairModel)
    {
        s_StairModel = new ModelRenderer();
        s_StairModel->Load("Asset\\Model\\Stair.obj");
    }
    if (!s_CorridorModel)
    {
        s_CorridorModel = new ModelRenderer();
        s_CorridorModel->Load("Asset\\Model\\Floor.obj");
    }
    if (!s_EditorCorridorModel)
    {
        s_EditorCorridorModel = new ModelRenderer();
        s_EditorCorridorModel->Load("Asset\\Model\\Corridor.obj");
    }
    if (!s_ShopFloorModel)
    {
        s_ShopFloorModel = new ModelRenderer();
        s_ShopFloorModel->Load("Asset\\Model\\ShopFloor.obj");
    }
    if (!s_GridVertexShader)
    {
        Renderer::CreateVertexShader(&s_GridVertexShader, &s_GridVertexLayout, "shader\\unlitColorVS.cso");
        Renderer::CreatePixelShader(&s_GridPixelShader, "shader\\unlitColorPS.cso");
    }


    m_Scale = { 1.0f, 1.0f, 1.0f };
}

void MapRenderer::Build(const MapData& map)
{
    m_Width = map.GetWidth();
    m_Height = map.GetHeight();

    // 領域を確保
    m_AllMatrices.assign(m_Width * m_Height, XMMatrixIdentity());
    m_TileTypes.assign(m_Width * m_Height, TileType::Wall);
    m_ActiveTiles.assign(m_Width * m_Height, false);
    m_ShopMatrices.clear();

    for (int y = 0; y < m_Height; y++)
    {
        for (int x = 0; x < m_Width; x++)
        {
            m_ActiveTiles[y * m_Width + x] = map.IsActiveTile(x, y);
            UpdateTile(x, y, map.GetTile(x, y));
        }
    }

    for (const Room& room : map.GetRooms())
    {
        if (room.specialType != RoomSpecialType::Shop) continue;
        const Vector2Int pos = room.GetPosition();
        const Vector2Int size = room.GetSize();
        const int innerW = (std::max)(0, size.x - 2);
        const int innerH = (std::max)(0, size.y - 2);
        const int side = (std::min)(innerW, innerH);
        if (side <= 0) continue;

        // 店床は部屋外周1マスを避け、内側に取れる最大の正方形として敷く。
        const int left = pos.x + 1 + (innerW - side) / 2;
        const int top = pos.y + 1 + (innerH - side) / 2;
        const float cx = (left + (side - 1) * 0.5f) * TILE_DISTANCE;
        const float cz = (top + (side - 1) * 0.5f) * TILE_DISTANCE;
        XMMATRIX scale = XMMatrixScaling((float)side, 1.0f, (float)side);
        XMMATRIX trans = XMMatrixTranslation(cx, 0.03f, cz);
        m_ShopMatrices.push_back(scale * trans);
    }

    BuildGridVertices();
    CreateGridVertexBuffer();
}

void MapRenderer::UpdateTile(int x, int y, TileType type)
{
    if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;

    int index = y * m_Width + x;

    // 行列を計算して上書き
    m_AllMatrices[index] = XMMatrixTranslation(x * TILE_DISTANCE, 0.0f, y * TILE_DISTANCE);
    // タイプを更新
    m_TileTypes[index] = type;
}

void MapRenderer::Draw()
{
    Renderer::SetCommonShader();
    Renderer::SetLight(LightManager::Instance().GetLight());

    for (const auto& shopMatrix : m_ShopMatrices)
    {
        Renderer::SetWorldMatrix(shopMatrix);
        s_ShopFloorModel->Draw();
    }

    for (int i = 0; i < (int)m_AllMatrices.size(); i++)
    {
        if (!m_ActiveTiles.empty() && !m_ActiveTiles[i]) continue;

        Renderer::SetWorldMatrix(m_AllMatrices[i]);

        // そのマスのタイプに応じてモデルを切り替えて描画
        switch (m_TileTypes[i])
        {
        case TileType::Floor:
            s_FloorModel->Draw();
            break;
        case TileType::Corridor:
            // Editor view uses Corridor.obj so manually placed passages are easy to identify.
            if (m_IsEditor) s_EditorCorridorModel->Draw();
            else s_CorridorModel->Draw();
            break;
        case TileType::Wall:
            s_WallModel->Draw();
            break;
        case TileType::Stair:
            s_StairModel->Draw();
            break;
        }
    }

    DrawGrid();
}
void MapRenderer::Uninit()
{
    s_WallModel->Uninit();
    s_FloorModel->Uninit();
    s_StairModel->Uninit();
    if (s_CorridorModel) s_CorridorModel->Uninit();
    if (s_EditorCorridorModel) s_EditorCorridorModel->Uninit();
    if (s_ShopFloorModel) s_ShopFloorModel->Uninit();
    if (m_GridVertexBuffer) { m_GridVertexBuffer->Release(); m_GridVertexBuffer = nullptr; }
    m_GridVertexBufferCapacity = 0;
    if (s_GridVertexShader) { s_GridVertexShader->Release(); s_GridVertexShader = nullptr; }
    if (s_GridPixelShader) { s_GridPixelShader->Release(); s_GridPixelShader = nullptr; }
    if (s_GridVertexLayout) { s_GridVertexLayout->Release(); s_GridVertexLayout = nullptr; }
}

void MapRenderer::Clear()
{
    m_AllMatrices.clear();
    m_TileTypes.clear();
    m_ActiveTiles.clear();
    m_ShopMatrices.clear();
    m_GridVertices.clear();
    if (m_GridVertexBuffer) { m_GridVertexBuffer->Release(); m_GridVertexBuffer = nullptr; }
    m_GridVertexBufferCapacity = 0;
}

void MapRenderer::AddGridLine(const XMFLOAT3& start, const XMFLOAT3& end)
{
    VERTEX_3D v{};
    v.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    v.Diffuse = XMFLOAT4(0.55f, 0.92f, 1.0f, 1.0f);
    v.TexCoord = XMFLOAT2(0.0f, 0.0f);

    v.Position = start;
    m_GridVertices.push_back(v);
    v.Position = end;
    m_GridVertices.push_back(v);
}

void MapRenderer::BuildGridVertices()
{
    m_GridVertices.clear();
    if (m_Width <= 0 || m_Height <= 0) return;

    auto isActive = [&](int x, int y) -> bool
        {
            return x >= 0 && x < m_Width && y >= 0 && y < m_Height
                && m_ActiveTiles[y * m_Width + x];
        };

    const float tileSize = static_cast<float>(TILE_DISTANCE);
    const float half = tileSize * 0.5f;
    const float gridY = 0.08f;

    for (int x = 0; x <= m_Width; ++x)
    {
        const float worldX = x * tileSize - half;
		//y座標ごとに両端マスのどちらかが範囲内であれば左上から右下に線を引く
        for (int y = 0; y < m_Height; ++y)
        {
            if (!isActive(x - 1, y) && !isActive(x, y)) continue;

            const float z0 = y * tileSize - half;
            const float z1 = y * tileSize + half;
            AddGridLine(XMFLOAT3(worldX, gridY, z0), XMFLOAT3(worldX, gridY, z1));
        }
    }

    for (int y = 0; y <= m_Height; ++y)
    {
        const float worldZ = y * tileSize - half;
        for (int x = 0; x < m_Width; ++x)
        {
            if (!isActive(x, y - 1) && !isActive(x, y)) continue;

            const float x0 = x * tileSize - half;
            const float x1 = x * tileSize + half;
            AddGridLine(XMFLOAT3(x0, gridY, worldZ), XMFLOAT3(x1, gridY, worldZ));
        }
    }
}

void MapRenderer::CreateGridVertexBuffer()
{
    const int vertexCount = static_cast<int>(m_GridVertices.size());
    if (vertexCount <= 0)
    {
        if (m_GridVertexBuffer) { m_GridVertexBuffer->Release(); m_GridVertexBuffer = nullptr; }
        m_GridVertexBufferCapacity = 0;
        return;
    }

    if (m_GridVertexBuffer && m_GridVertexBufferCapacity >= vertexCount) return;

    if (m_GridVertexBuffer)
    {
        m_GridVertexBuffer->Release();
        m_GridVertexBuffer = nullptr;
    }

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(VERTEX_3D) * vertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (SUCCEEDED(Renderer::GetDevice()->CreateBuffer(&bd, nullptr, &m_GridVertexBuffer)))
    {
        m_GridVertexBufferCapacity = vertexCount;
    }
    else
    {
        m_GridVertexBufferCapacity = 0;
    }
}

void MapRenderer::DrawGrid()
{
    if (!m_IsEditor && !Input::GetKeyPress(VK_SPACE)) return;
    if (m_GridVertices.empty()) return;

    CreateGridVertexBuffer();

	//頂点バッファとシェーダーが揃ってないなら描画しない
    if (!m_GridVertexBuffer || !s_GridVertexShader || !s_GridPixelShader || !s_GridVertexLayout) return;

    const float t = timeGetTime() * 0.006f;
    const float pulse = (std::sin(t) + 1.0f) * 0.5f;
    const float alpha = 0.28f + 0.42f * pulse;

	//点滅するグリッドの色を計算
    const XMFLOAT4 color(0.55f + 0.25f * pulse, 0.90f + 0.10f * pulse, 1.0f, alpha);

    for (auto& v : m_GridVertices)
    {
        v.Diffuse = color;
    }

	// 頂点バッファにデータを転送
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(Renderer::GetDeviceContext()->Map(m_GridVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    std::memcpy(mapped.pData, m_GridVertices.data(), sizeof(VERTEX_3D) * m_GridVertices.size());
    Renderer::GetDeviceContext()->Unmap(m_GridVertexBuffer, 0);

    Renderer::GetDeviceContext()->IASetInputLayout(s_GridVertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(s_GridVertexShader, nullptr, 0);
    Renderer::GetDeviceContext()->PSSetShader(s_GridPixelShader, nullptr, 0);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_GridVertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    Renderer::SetWorldMatrix(XMMatrixIdentity());

    
    Renderer::GetDeviceContext()->Draw(static_cast<UINT>(m_GridVertices.size()), 0);

    Renderer::SetCommonShader();
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}