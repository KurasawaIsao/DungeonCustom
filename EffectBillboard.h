#pragma once
#include "GameObject.h"
#include "renderer.h"

class EffectBillboard : public GameObject
{
private:
    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11ShaderResourceView* m_Texture = nullptr;

    int   m_FrameCounter = 0;
    int   m_MaxFrames = 16;
    int   m_Rows = 4;
    int   m_Cols = 4;
    float m_AnimSpeed = 1.0f;
    bool  m_IsLoop = false;

public:
    static EffectBillboard* Create(Vector3 pos, const char* fileName, int rows = 4, int cols = 4, float speed = 1.0f, Vector3 scale = { 1,1,1 });

    void Init() override;
    void Update() override;
    void Draw() override;
    void Uninit() override;

    void SetLoop(bool loop) { m_IsLoop = loop; }
    void SetParam(const char* fileName, int rows, int cols, float speed);
};