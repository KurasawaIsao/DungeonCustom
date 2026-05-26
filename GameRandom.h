#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <random>

class GameRandom
{
public:
    static void Seed(unsigned int seed)
    {
        Engine().seed(seed);
    }

    static void Reseed()
    {
        const unsigned int timeSeed = static_cast<unsigned int>(std::time(nullptr));
        const unsigned int clockSeed = static_cast<unsigned int>(std::clock());
        Seed(timeSeed ^ (clockSeed << 1));
    }

    static int Range(int minValue, int maxValue)
    {
        if (maxValue <= minValue) return minValue;
        std::uniform_int_distribution<int> dist(minValue, maxValue);
        return dist(Engine());
    }

    static float Range(float minValue, float maxValue)
    {
        if (maxValue <= minValue) return minValue;
        std::uniform_real_distribution<float> dist(minValue, maxValue);
        return dist(Engine());
    }

    static double Range(double minValue, double maxValue)
    {
        if (maxValue <= minValue) return minValue;
        std::uniform_real_distribution<double> dist(minValue, maxValue);
        return dist(Engine());
    }

    static float Value()
    {
        return Range(0.0f, 1.0f);
    }

    static bool Chance(float probability)
    {
        if (probability <= 0.0f) return false;
        if (probability >= 1.0f) return true;
        return Value() < probability;
    }

    static bool Percent(int percent)
    {
        const int clamped = (std::max)(0, (std::min)(100, percent));
        return Range(1, 100) <= clamped;
    }

    static size_t Index(size_t count)
    {
        if (count == 0) return 0;
        std::uniform_int_distribution<size_t> dist(0, count - 1);
        return dist(Engine());
    }

    static std::mt19937& Engine()
    {
        static std::mt19937 engine(CreateSeed());
        return engine;
    }

private:
    static unsigned int CreateSeed()
    {
        const unsigned int timeSeed = static_cast<unsigned int>(std::time(nullptr));
        const unsigned int clockSeed = static_cast<unsigned int>(std::clock());
        return timeSeed ^ (clockSeed << 1);
    }
};
