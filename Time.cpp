#include "Time.h"

float Time::m_DeltaTime = 0.0f;

void Time::Update()
{
    static LARGE_INTEGER prev = { 0 };
    static LARGE_INTEGER freq = { 0 };

    if (freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&prev);
        return;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    m_DeltaTime = (float)(now.QuadPart - prev.QuadPart) / (float)freq.QuadPart;
    prev = now;
}
