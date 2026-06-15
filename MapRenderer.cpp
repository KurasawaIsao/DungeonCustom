#include "MapRenderer.h"
#include "modelRenderer.h"
#include "renderer.h"
#include "LightManager.h"
#include "DungeonThemeDatabase.h"
#include "input.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    // マップ本体の外側に追加で敷く壁の厚み。
    constexpr int kOuterWallPadding = 10;
}
ModelRenderer* MapRenderer::m_FloorModel = nullptr;
ModelRenderer* MapRenderer::m_WallModel = nullptr;
ModelRenderer* MapRenderer::m_StairModel = nullptr;
ModelRenderer* MapRenderer::m_CorridorModel= nullptr;
ModelRenderer* MapRenderer::m_EditorCorridorModel = nullptr;
ModelRenderer* MapRenderer::m_ShopFloorModel = nullptr;
std::string MapRenderer::m_CurrentThemeId = "default";
ID3D11VertexShader* MapRenderer::m_GridVertexShader = nullptr;
ID3D11PixelShader* MapRenderer::m_GridPixelShader = nullptr;
ID3D11InputLayout* MapRenderer::m_GridVertexLayout = nullptr;
void MapRenderer::SetTheme(const std::string& themeId)
{
    m_CurrentThemeId = themeId.empty() ? "default" : themeId;

    // 既にモデルが作られている場合は、次の描画から指定テーマのモデルに差し替える。
    if (m_FloorModel || m_WallModel || m_StairModel || m_CorridorModel || m_EditorCorridorModel || m_ShopFloorModel)
        LoadCurrentThemeModels();
}

void MapRenderer::LoadCurrentThemeModels()
{
    const DungeonThemeData& theme = DungeonThemeDatabase::GetOrDefault(m_CurrentThemeId);
    m_CurrentThemeId = theme.id;

    auto loadModel = [](ModelRenderer*& model, const std::string& path)
        {
            if (path.empty()) return;
            if (!model) model = new ModelRenderer();
            model->Load(path.c_str());
        };

    // タイル種別ごとのモデルをテーマ設定から読み込む。
    loadModel(m_FloorModel, theme.models.floor);
    loadModel(m_WallModel, theme.models.wall);
}
void MapRenderer::Init()
{
    if (!m_FloorModel)
    {
        m_FloorModel = new ModelRenderer();
        m_FloorModel->Load("Asset\\Model\\Floor.obj");
    }

    if (!m_WallModel)
    {
        m_WallModel = new ModelRenderer();
        m_WallModel->Load("Asset\\Model\\Wall.obj");
    }

    if (!m_StairModel)
    {
        m_StairModel = new ModelRenderer();
        m_StairModel->Load("Asset\\Model\\Stair.obj");
    }
    if (!m_CorridorModel)
    {
        m_CorridorModel = new ModelRenderer();
        m_CorridorModel->Load("Asset\\Model\\Floor.obj");
    }
    if (!m_EditorCorridorModel)
    {
        m_EditorCorridorModel = new ModelRenderer();
        m_EditorCorridorModel->Load("Asset\\Model\\Corridor.obj");
    }
    if (!m_ShopFloorModel)
    {
        m_ShopFloorModel = new ModelRenderer();
        m_ShopFloorModel->Load("Asset\\Model\\ShopFloor.obj");
    }
    LoadCurrentThemeModels();

    if (!m_GridVertexShader)
    {
        Renderer::CreateVertexShader(&m_GridVertexShader, &m_GridVertexLayout, "shader\\unlitColorVS.cso");
        Renderer::CreatePixelShader(&m_GridPixelShader, "shader\\unlitColorPS.cso");
    }


    m_Scale = { 1.0f, 1.0f, 1.0f };
}

void MapRenderer::Build(const MapData& map)
{
    m_Width = map.GetWidth();
    m_Height = map.GetHeight();

    // 領域を確保
    m_AllMatrices.assign(m_Width * m_Height, XMMatrixIdentity());
    m_OuterWallTiles.clear();
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
        if (room.m_SpecialType != RoomSpecialType::Shop) continue;
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
    BuildOuterWallTiles();

    BuildGridVertices();
    CreateGridVertexBuffer();
}

void MapRenderer::BuildOuterWallTiles()
{
    m_OuterWallTiles.clear();

    // マップエディタでは編集範囲を見やすくするため、範囲外の壁を追加しない。
    if (m_IsEditor) return;
    if (m_Width <= 0 || m_Height <= 0) return;

    const int minX = -kOuterWallPadding;
    const int minY = -kOuterWallPadding;
    const int maxX = m_Width + kOuterWallPadding;
    const int maxY = m_Height + kOuterWallPadding;
    const int outerWidth = maxX - minX;
    const int outerHeight = maxY - minY;
    const int outerTileCount = outerWidth * outerHeight - m_Width * m_Height;
    m_OuterWallTiles.reserve((std::max)(0, outerTileCount));

    for (int y = minY; y < maxY; ++y)
    {
        for (int x = minX; x < maxX; ++x)
        {
            const bool isInsideMap = x >= 0 && x < m_Width && y >= 0 && y < m_Height;
            if (isInsideMap) continue;

            // マップ範囲外の座標だけを、表示専用の壁タイルとして追加する。
            m_OuterWallTiles.push_back({ x, y });
        }
    }
}
void MapRenderer::UpdateTile(int x, int y, TileType type)
{
    if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;

    int index = y * m_Width + x;

    // 行列を計算して上書き
    m_AllMatrices[index] = XMMatrixTranslation(static_cast<float>(x * TILE_DISTANCE), 0.0f, static_cast<float>(y * TILE_DISTANCE));
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
        // エディタ上の床は鏡面反射を無効化し、局所的な白い光が出ないようにする。
        m_ShopFloorModel->Draw(m_IsEditor);
    }

    // マップ本体の外側10マス分を壁として描画する。
    for (const auto& outerWallTile : m_OuterWallTiles)
    {
        XMMATRIX matrix = XMMatrixTranslation(static_cast<float>(outerWallTile.x * TILE_DISTANCE), 0.0f, static_cast<float>(outerWallTile.y * TILE_DISTANCE));
        Renderer::SetWorldMatrix(matrix);
        m_WallModel->Draw();
    }

    for (int i = 0; i < (int)m_AllMatrices.size(); i++)
    {
        if (!m_ActiveTiles.empty() && !m_ActiveTiles[i]) continue;

        Renderer::SetWorldMatrix(m_AllMatrices[i]);

        // そのマスのタイプに応じてモデルを切り替えて描画
        switch (m_TileTypes[i])
        {
        case TileType::Floor:
            m_FloorModel->Draw(m_IsEditor);
            break;
        case TileType::Corridor:
            if (m_IsEditor) m_EditorCorridorModel->Draw(true);
            else m_FloorModel->Draw();
            break;
        case TileType::Wall:
            m_WallModel->Draw();
            break;
        case TileType::Stair:
            m_StairModel->Draw();
            break;
        }
    }

    DrawGrid();
}
void MapRenderer::Uninit()
{
    m_WallModel->Uninit();
    m_FloorModel->Uninit();
    m_StairModel->Uninit();
    if (m_CorridorModel) m_CorridorModel->Uninit();
    if (m_EditorCorridorModel) m_EditorCorridorModel->Uninit();
    if (m_ShopFloorModel) m_ShopFloorModel->Uninit();
    if (m_GridVertexBuffer) { m_GridVertexBuffer->Release(); m_GridVertexBuffer = nullptr; }
    m_GridVertexBufferCapacity = 0;
    if (m_GridVertexShader) { m_GridVertexShader->Release(); m_GridVertexShader = nullptr; }
    if (m_GridPixelShader) { m_GridPixelShader->Release(); m_GridPixelShader = nullptr; }
    if (m_GridVertexLayout) { m_GridVertexLayout->Release(); m_GridVertexLayout = nullptr; }
}

void MapRenderer::Clear()
{
    m_AllMatrices.clear();
    m_OuterWallTiles.clear();
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

    // グリッド線の最大本数から頂点数を先に確保し、push_back中の再確保を避ける。
    const size_t maxGridLineCount = static_cast<size_t>(m_Width + 1) * static_cast<size_t>(m_Height)
        + static_cast<size_t>(m_Height + 1) * static_cast<size_t>(m_Width);
    m_GridVertices.reserve(maxGridLineCount * 2);

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
    if (!m_GridVertexBuffer || !m_GridVertexShader || !m_GridPixelShader || !m_GridVertexLayout) return;

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

    Renderer::GetDeviceContext()->IASetInputLayout(m_GridVertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_GridVertexShader, nullptr, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_GridPixelShader, nullptr, 0);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_GridVertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    Renderer::SetWorldMatrix(XMMatrixIdentity());

    
    Renderer::GetDeviceContext()->Draw(static_cast<UINT>(m_GridVertices.size()), 0);

    Renderer::SetCommonShader();
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}