#include "EffectBillboard.h"
#include "manager.h"
#include "renderer.h"
#include "texture.h"
#include "scene.h"
#include "camera.h"
#include "MapManager.h"
#include "UnitManager.h"
#include "Player.h"
#include "MapData.h"
#include <algorithm>
#include <cmath>

namespace
{
    bool IsHiddenByUnclearVision(const Vector3& position)
    {
        MapManager* mapManager = MapManager::Instance();
        if (!mapManager || !mapManager->HasMap()) return false;
        if (mapManager->GetCurrentFloorData().playerVisionClear) return false;

        UnitManager* unitManager = UnitManager::Instance();
        Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
        if (!player) return false;

        Vector2Int gridPos(
            (int)std::round(position.x / (float)TILE_DISTANCE),
            (int)std::round(position.z / (float)TILE_DISTANCE)
        );
        return !player->IsInView(gridPos);
    }
}
EffectBillboard* EffectBillboard::Create(Vector3 pos, const char* fileName, int rows, int cols, float speed, Vector3 scale)
{
    EffectBillboard* effect = Manager::GetScene()->AddGameObject<EffectBillboard>(1);
    effect->SetPosition(pos);
    effect->SetScale(scale);
    effect->SetParam(fileName, rows, cols, speed);
    return effect;
}

void EffectBillboard::SetParam(const char* fileName, int rows, int cols, float speed)
{
    m_Texture = Texture::Load(fileName);
    m_Rows = std::max(1, rows);
    m_Cols = std::max(1, cols);
    m_AnimSpeed = std::max(0.001f, speed);
    m_MaxFrames = m_Rows * m_Cols;
}

void EffectBillboard::Init()
{
    VERTEX_3D vertex[4]{};

    // 初期データ（Drawで更新されるため、ここでは枠組みの定義）
    vertex[0].Position = XMFLOAT3(-1.0f, 1.0f, 0.0f);
    vertex[0].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

    vertex[1].Position = XMFLOAT3(1.0f, 1.0f, 0.0f);
    vertex[1].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

    vertex[2].Position = XMFLOAT3(-1.0f, -1.0f, 0.0f);
    vertex[2].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

    vertex[3].Position = XMFLOAT3(1.0f, -1.0f, 0.0f);
    vertex[3].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

    // 頂点バッファ作成
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(VERTEX_3D) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex;

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    // シェーダー読み込み
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

    m_FrameCounter = 0;
}

void EffectBillboard::Update()
{
    m_FrameCounter++;

    // アニメーション終了判定 (スピードを考慮)
    if (m_FrameCounter >= (int)(m_MaxFrames * m_AnimSpeed))
    {
        if (m_IsLoop)
        {
            m_FrameCounter = 0;
        }
        else
        {
            SetDestroy();
        }
    }
}

void EffectBillboard::Draw()
{
    if (!m_VertexBuffer || !m_Texture) return;
    if (IsHiddenByUnclearVision(m_Position)) return;

    // --- 頂点データの更新 (UVアニメーション) ---
    D3D11_MAPPED_SUBRESOURCE msr;
    if (FAILED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
    {
        return;
    }

    VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;
    int currentIdx = (int)(m_FrameCounter / m_AnimSpeed);
    currentIdx = std::clamp(currentIdx, 0, m_MaxFrames - 1);

    float tw = 1.0f / (float)m_Cols;
    float th = 1.0f / (float)m_Rows;
    float tx = (currentIdx % m_Cols) * tw;
    float ty = (currentIdx / m_Cols) * th;

    // 四角形ポリゴンの構築
    vertex[0].Position = XMFLOAT3(-1.0f, 1.0f, 0.0f);
    vertex[0].TexCoord = XMFLOAT2(tx, ty);

    vertex[1].Position = XMFLOAT3(1.0f, 1.0f, 0.0f);
    vertex[1].TexCoord = XMFLOAT2(tx + tw, ty);

    vertex[2].Position = XMFLOAT3(-1.0f, -1.0f, 0.0f);
    vertex[2].TexCoord = XMFLOAT2(tx, ty + th);

    vertex[3].Position = XMFLOAT3(1.0f, -1.0f, 0.0f);
    vertex[3].TexCoord = XMFLOAT2(tx + tw, ty + th);

    // 全頂点共通の属性
    for (int i = 0; i < 4; i++) {
        vertex[i].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
        vertex[i].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

        for (int j = 0; j < 4; j++) {
            vertex[i].BoneIndices[j] = 0;
            vertex[i].BoneWeights[j] = 0.0f;
        }
    }

    Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);

 
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

    Camera* camera = Manager::GetScene()->GetGameObject<Camera>();
    XMMATRIX view = camera->GetViewMatrix();
    XMMATRIX invView = XMMatrixInverse(nullptr, view);

    // 平行移動成分をクリア
    invView.r[3].m128_f32[0] = 0.0f;
    invView.r[3].m128_f32[1] = 0.0f;
    invView.r[3].m128_f32[2] = 0.0f;

    XMMATRIX world = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) * invView * XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    Renderer::SetWorldMatrix(world);

    MATERIAL material{};
    material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    Renderer::SetDepthEnable(false);
    Renderer::SetCullMode(D3D11_CULL_NONE);

    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Renderer::GetDeviceContext()->Draw(4, 0);

    Renderer::SetCullMode(D3D11_CULL_BACK);
    Renderer::SetDepthEnable(true);
}

void EffectBillboard::Uninit()
{
    if (m_VertexShader) m_VertexShader->Release();
    if (m_PixelShader)  m_PixelShader->Release();
    if (m_VertexLayout) m_VertexLayout->Release();
    if (m_VertexBuffer) m_VertexBuffer->Release();
    // m_TextureはTexture::Loadの管理ルール（一括解放など）に従うためここでは保留
}
