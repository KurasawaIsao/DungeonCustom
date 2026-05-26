#pragma once
#include "particle.h"

class SplashParticle : public Particle
{
protected:
    void OnParticleSpawn(PARTICLE& p) override;
    void UpdateParticle(PARTICLE& p) override;
};
