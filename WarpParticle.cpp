#include "GameRandom.h"
#include "WarpParticle.h"
#include <cstdlib>

void WarpParticle::OnParticleSpawn(PARTICLE& p)
{
    SetTexture("texture\\snow.jpg");
    p.MaxLife = p.Life = 40;

    // â~èÛÇ…èâä˙îzíu
    float angle = GameRandom::Range(0.0f, 360.0f) * XM_PI / 180.0f;
    float radius = GameRandom::Range(0.0f, 1.5f);

    p.Position += Vector3(
        cosf(angle) * radius,
        GameRandom::Range(-0.25f, 0.25f),
        sinf(angle) * radius
    );

    p.Velocity = Vector3(0, 0, 0);
    p.Scale = 0.3f;
}

void WarpParticle::UpdateParticle(PARTICLE& p)
{
    Vector3 toCenter = Center - p.Position;

    // ãzà¯
    p.Velocity += toCenter * 0.05f;

    p.Position += p.Velocity;

    float t = 1.0f - (float)p.Life / p.MaxLife;

    // âÒì]ä¥Åiâ°óhÇÍÅj
    p.Position.x += sinf(t * 20.0f) * 0.02f;
    p.Position.z += cosf(t * 20.0f) * 0.02f;

    // èkè¨Å{ÉtÉFÅ[Éh
    p.Scale *= 0.95f;
    p.Alpha = 1.0f - t;
}
