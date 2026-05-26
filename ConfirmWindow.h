#pragma once
#include "Polygon.h"
#include "GameObject.h"
class ConfirmWindow :public GameObject {
public:


    void Init();
    void Uninit();
    void Update();
    void Draw();

    bool GetConfirm() { return m_Yes; };
    void SetEnable(bool e) { m_Enable = e; };
    bool GetEnable() { return m_Enable; };
    void SetDecided(bool d) { m_Decided = d; };
    bool GetDecided(){ return m_Decided; };

private:
    Polygon2D* m_Window = nullptr;
    Polygon2D* m_Arrow = nullptr;
    bool m_Enable;
    bool m_Decided;
    bool m_Yes;
};
