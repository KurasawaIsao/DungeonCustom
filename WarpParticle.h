#pragma once
#include "Particle.h"

class WarpParticle : public Particle
{
public:
    Vector3 Center;

protected:
    void OnParticleSpawn(PARTICLE& p) override;
    void UpdateParticle(PARTICLE& p) override;
};
