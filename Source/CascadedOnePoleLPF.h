/*
  ==============================================================================

    CascadedOnePoleLPF.h
    Created: 6 Jan 2026 6:05:35pm
    Author:  matjo

  ==============================================================================
*/
/*
#pragma once
#include <cmath>

class CascadedOnePoleLPF {

public:
    CascadedOnePoleLPF() = default;

    void prepare(double sampleRate) {
        fs = sampleRate;
        reset();
    }

    void reset() {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    inline float processSample(float input, float cutoffHz) {
        if (cutoffHz < 10.0f)
            cutoffHz = 10.0f;

        const float g = 1.0f - std::exp(-twoPi * cutoffHz / fs);

        z1 += g * (input - z1);

        z2 += g * (z1 - z2);

        return z2;

    }




private:
    double fs = 48000.0f;
    float z1 = 0.0f;
    float z2 = 0.0f;

    static constexpr float twoPi = 6.283185307179586f;

};
*/
