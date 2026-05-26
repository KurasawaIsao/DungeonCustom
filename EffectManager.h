#pragma once
#include <string>
#include "audio.h"
#include "Vector3.h"

class EffectManager {
public:
    // --- 単発エフェクトと音を同時に再生 ---
    static void Play(Vector3 pos, const char* texPath, const char* sePath = nullptr);

    // --- 音だけ再生 ---
    static void PlaySE(const char* sePath);
    static void PlayBGM(const char* sePath);

    static class EffectBillboard* CreateLoopEffect(Vector3 pos, const char* texPath);
    static class EffectBillboard* CreateSpriteEffect(Vector3 pos, const char* texPath);
};