#pragma once
#include <windows.h>

class Time
{
private:
    static float deltaTime;

public:
    static void Update();
    static float DeltaTime() { return deltaTime; }
};
