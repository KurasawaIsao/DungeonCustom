#pragma once
#include "renderer.h"   

class LightManager
{
public:
    static LightManager& Instance()
    {
        static LightManager inst;
        return inst;
    }

    void Init();

    // Rendererd—l‚ÉŠ®‘S€‹’
    const LIGHT& GetLight() const { return m_Light; }

private:
    LIGHT m_Light;
};
