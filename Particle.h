#pragma once
#include "gameobject.h"
#include "Vector3.h"
#include "Vector4.h"
#include "renderer.h"
#include "texture.h"

#define PARTICLE_MAX 512

struct PARTICLE
{
    bool Enable = false;

    Vector3 Position;
    Vector3 Velocity;

    float Scale = 1.0f;
    float Rotation = 0.0f;

    float Alpha = 1.0f;

    int Life = 0;
    int MaxLife = 0;

    Vector4 Color = Vector4(1, 1, 1, 1);
};

class Particle : public GameObject
{
protected:

    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11ShaderResourceView* m_Texture = nullptr;

    PARTICLE m_particle[PARTICLE_MAX];

public:
    virtual void Init() override;
    virtual void Update() override;
    virtual void Draw() override;
    virtual void Uninit() override;

protected:
    // 派生クラスがカスタムする
    virtual void OnParticleSpawn(PARTICLE& p);
    virtual void UpdateParticle(PARTICLE& p);
    virtual void ApplyBillboard(PARTICLE& p, XMMATRIX& outMatrix);
    void SetTexture(const char* File)
    {
        m_Texture = Texture::Load(File);
    }

public:
    virtual void Spawn(const Vector3& pos);
    virtual void Spawn(const Vector3& pos, const Vector4& color);

};
