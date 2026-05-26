#include "particle.h"
#include "manager.h"
#include "camera.h"
#include "manager.h"
#include "scene.h"
#include <cmath>

void Particle::Init()
{
    VERTEX_3D vertex[4]{};

    vertex[0].Position = XMFLOAT3(-1, 1, 0);
    vertex[0].TexCoord = XMFLOAT2(0, 0);
    vertex[1].Position = XMFLOAT3(1, 1, 0);
    vertex[1].TexCoord = XMFLOAT2(1, 0);
    vertex[2].Position = XMFLOAT3(-1, -1, 0);
    vertex[2].TexCoord = XMFLOAT2(0, 1);
    vertex[3].Position = XMFLOAT3(1, -1, 0);
    vertex[3].TexCoord = XMFLOAT2(1, 1);
    for (int i = 0; i < 4; i++)
    {
        vertex[i].Diffuse = XMFLOAT4(1, 1, 1, 1);
    }


    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertex);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex;

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\particleVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "shader\\particlePS.cso");

    m_Texture = Texture::Load("Asset\\Texture\\particle.png");

    for (auto& p : m_particle)
        p.Enable = false;
}

void Particle::Spawn(const Vector3& pos)
{
    Spawn(pos, Vector4(1, 1, 1, 1));
}

void Particle::Spawn(const Vector3& pos, const Vector4& color)
{
    for (int i = 0; i < PARTICLE_MAX; i++)
    {
        if (!m_particle[i].Enable)
        {
            m_particle[i].Enable = true;
            m_particle[i].Position = pos;
            m_particle[i].Color = color;
            OnParticleSpawn(m_particle[i]);
            return;
        }
    }
}


void Particle::Update()
{
    for (auto& p : m_particle)
    {
        if (!p.Enable) continue;

        UpdateParticle(p); // 派生クラスで動作差し替え可

        p.Position += p.Velocity;

        p.Life--;
        if (p.Life <= 0)
            p.Enable = false;
    }
}

void Particle::UpdateParticle(PARTICLE& p)
{
    // デフォルト重力のみ
    p.Velocity.y -= 0.01f;

    float t = (float)p.Life / p.MaxLife;
    p.Alpha = t;
}

void Particle::Draw()
{
    Camera* cam = Manager::GetScene()->GetGameObject<Camera>();

    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, nullptr, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, nullptr, 0);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

  


    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    Renderer::SetDepthEnable(false);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    for (auto& p : m_particle)
    {
        if (!p.Enable) continue;

        MATERIAL mat{};
        mat.Diffuse = {
            p.Color.x,
            p.Color.y,
            p.Color.z,
            p.Alpha
        };
        mat.TextureEnable = true;
        Renderer::SetMaterial(mat);

        XMMATRIX m;
        ApplyBillboard(p, m);

        Renderer::SetWorldMatrix(m);
        Renderer::GetDeviceContext()->Draw(4, 0);
    }

    Renderer::SetDepthEnable(true);
}

void Particle::ApplyBillboard(PARTICLE& p, XMMATRIX& outMatrix)
{
    Camera* cam = Manager::GetScene()->GetGameObject<Camera>();

    XMMATRIX view = cam->GetViewMatrix();
    XMMATRIX inv = XMMatrixInverse(nullptr, view);

    inv.r[3] = XMVectorSet(0, 0, 0, 1);

    XMMATRIX rot = XMMatrixRotationZ(p.Rotation);
    XMMATRIX scale = XMMatrixScaling(p.Scale, p.Scale, 1);
    XMMATRIX trans = XMMatrixTranslation(p.Position.x, p.Position.y, p.Position.z);

    outMatrix = scale * rot * inv * trans;
}

void Particle::OnParticleSpawn(PARTICLE& p)
{
    // デフォルトは何もしない
}

void Particle::Uninit()
{
    if (m_VertexBuffer) m_VertexBuffer->Release();
    if (m_VertexLayout) m_VertexLayout->Release();
    if (m_VertexShader) m_VertexShader->Release();
    if (m_PixelShader) m_PixelShader->Release();
}

