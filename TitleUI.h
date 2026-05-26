#pragma once
#include "GameObject.h"
#include "Polygon.h"
class TitleUI : public GameObject
{
public:
    void Init();
    void Update();
    void Draw();
    void Uninit();
    void OnStartButtonPressed();
    void Start();   // •\Ž¦ŠJŽn

    ID3D11ShaderResourceView* m_StartButtonSRV;


private:
    Polygon2D* m_TitleLogo = nullptr;


    float   m_LogoSpeed = 0.0f;
    bool    m_LogoArrived = false;


    float m_Time = 0.0f;
    bool  m_Started = false;
};
