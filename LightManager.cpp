#include "LightManager.h"

void LightManager::Init()
{

    m_Light.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
    m_Light.Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    m_Light.Direction = XMFLOAT4(0.5f, -0.7f, 0.5f, 0.0f);
}
