#pragma once
#include <windows.h>

class Time
{
private:
    static float m_DeltaTime;

public:
    static void Update();
    static float DeltaTime() { return m_DeltaTime; }
};
