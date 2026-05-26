#include "GameRandom.h"
#include "splashParticle.h"
#include <cstdlib>
#include <cmath>

void SplashParticle::OnParticleSpawn(PARTICLE& p)
{
    float angle = GameRandom::Range(0.0f, 360.0f) * XM_PI / 180.0f;

    p.Velocity = Vector3(
        cosf(angle) * 0.2f,
        GameRandom::Range(0.7f, 1.03f),
        sinf(angle) * 0.2f
    );

    p.Scale = GameRandom::Range(0.15f, 0.4f);
    p.MaxLife = 60;
    p.Life = p.MaxLife;

    p.Color = Vector4(0.8f, 0.9f, 1.0f, 1.0f);
}

void SplashParticle::UpdateParticle(PARTICLE& p)
{
    p.Velocity.y -= 0.03f; // 重力
    p.Velocity.x *= 0.96f;  // 空気抵抗
    p.Velocity.y *= 0.96f;  // 空気抵抗
    p.Velocity.z *= 0.96f;  // 空気抵抗

    float t = (float)p.Life / p.MaxLife;
    p.Alpha = t;
    p.Color.w = t;
}
