/*
  ==============================================================================

    LutSincInterpolation.h
    Created: 18 Jan 2026 8:39:07am
    Author:  matjo

  ==============================================================================
*/

#pragma once


#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <cstddef>

namespace ttvst::lutSinc
{

    double sinc_pi(double x) noexcept
    {
        // sinc_pi(x) = sin(pi*x)/(pi*x)
        if (std::abs(x) < 1e-12)
            return 1.0;
        const double pix = juce::MathConstants<double>::pi * x;
        return std::sin(pix) / pix;
    }

    // Classic Blackman coefficients: a0=0.42, a1=0.5, a2=0.08
    double blackman_window(double n, double M) noexcept
    {
        if (M <= 0.0)
            return 1.0;

        const double a0 = 0.42;
        const double a1 = 0.50;
        const double a2 = 0.08;

        const double twoPi = juce::MathConstants<double>::twoPi;
        const double w1 = std::cos(twoPi * n / M);
        const double w2 = std::cos(2.0 * twoPi * n / M);
        return a0 - a1 * w1 + a2 * w2;
    }


    // P: number of phases (e.g. 512..4096)
    // N: taps per phase (odd), e.g. 9, 15, 17, 31
    // fc: normalized cutoff in cycles/sample, (0, 0.5]. Use ~0.45 for SRC safety.
    //
    // Returns flat vector of size P*N (phase-major).
    std::vector<float> generateLutSinc(int P, int N, double fc)
    {
        if (P <= 0)
            throw std::invalid_argument("generateLutSinc: P must be > 0");
        if (N <= 1 || (N % 2) == 0)
            throw std::invalid_argument("generateLutSinc: N must be an odd integer > 1");
        if (!(fc > 0.0 && fc <= 0.5))
            throw std::invalid_argument("generateLutSinc: fc must be in (0, 0.5]");

        const int K = (N - 1) / 2;
        (void)K;

        std::vector<float> lut(static_cast<std::size_t>(P) * static_cast<std::size_t>(N), 0.0f);

        // Blackman window is defined on discrete n=0..N-1; we use a continuous n.
        const double M = static_cast<double>(N - 1);

        for (int p = 0; p < P; ++p)
        {
            const double alpha = static_cast<double>(p) / static_cast<double>(P); // [0,1)
            double sum = 0.0;

            for (int tap = 0; tap < N; ++tap)
            {
                const int k = tap - ((N - 1) / 2);             // integer offset [-K..K]
                const double u = static_cast<double>(k) - alpha; // fractional offset

                // "Fractionally shifted" window index:
                // tap is 0..N-1, shift by alpha so window aligns with the fractional delay.
                double n_cont = static_cast<double>(tap) - alpha;
                n_cont = std::clamp(n_cont, 0.0, M);

                //const double w = blackman_window(n_cont, M);
                const double w = blackman_window((double)tap, M);
                // Lowpass windowed-sinc kernel:
                // h(u) = 2*fc * sinc(2*fc*u) * w(...)
                const double h = (2.0 * fc) * sinc_pi(2.0 * fc * u) * w;

                const std::size_t idx = static_cast<std::size_t>(p) * static_cast<std::size_t>(N)
                    + static_cast<std::size_t>(tap);

                lut[idx] = static_cast<float>(h);
                sum += h;
            }

            // DC normalization per phase (prevents amplitude flutter vs alpha)
            if (std::abs(sum) > 1e-18)
            {
                const double invSum = 1.0 / sum;
                const std::size_t base = static_cast<std::size_t>(p) * static_cast<std::size_t>(N);
                for (int tap = 0; tap < N; ++tap)
                {
                    lut[base + static_cast<std::size_t>(tap)] =
                        static_cast<float>(static_cast<double>(lut[base + static_cast<std::size_t>(tap)]) * invSum);
                }
            }
        }

        return lut;
    }

    //--------------------------------------------------------------------------
    // Interpolation: LUT-sinc (looping source)

    // Evaluates the signal at `playhead` using a windowed-sinc LUT.
    //
    // Requirements:
    // - playhead should be wrapped to [0, srcN) before calling (as in your current code).
    // - srcN must match src.getNumSamples().
    //
    // Returns: one interpolated sample (float) for channel ch.
    float interpolateSincLUT(
        const juce::AudioBuffer<float>& src,
        int ch,
        double playhead,
        int srcN,
        const float* lut,
        int P,
        int N
    ) noexcept
    {
        // Assumes playhead >= 0 due to external wrapping.
        const int K = (N - 1) / 2;

        const int i0 = static_cast<int>(playhead); // floor for non-negative
        const float alpha = static_cast<float>(playhead - static_cast<double>(i0));

        int phase = static_cast<int>(alpha * static_cast<float>(P) + 0.5f);
        if (phase >= P) phase = P - 1;

        const float* coeff = lut + static_cast<std::size_t>(phase) * static_cast<std::size_t>(N);
        const float* x = src.getReadPointer(ch);

        float y = 0.0f;

        for (int tap = 0; tap < N; ++tap)
        {
            const int offset = tap - K;
            int idx = i0 + offset;

            // Wrap indices for looping/circular playback
            idx %= srcN;
            if (idx < 0) idx += srcN;

            y += x[idx] * coeff[tap];
        }

        return y;
    }

    float interpolateSincLUT_PhaseLerp(
        const juce::AudioBuffer<float>& src,
        int ch,
        double playhead,
        int srcN,
        const float* lut,
        int P,
        int N
    ) noexcept
    {
        const int K = (N - 1) / 2;

        // floor for non-negative playhead
        const int i0 = static_cast<int>(playhead);

        // keep fractional part in double
        const double alpha = playhead - static_cast<double>(i0); // [0, 1)

        // continuous phase position
        const double p = alpha * static_cast<double>(P);
        int p0 = static_cast<int>(std::floor(p));                // 0..P-1 (except alpha==1)
        double mu = p - static_cast<double>(p0);                 // 0..1

        // clamp (safety)
        if (p0 < 0) { p0 = 0; mu = 0.0; }
        if (p0 >= P) { p0 = P - 1; mu = 0.0; }

        int p1 = p0 + 1;
        if (p1 >= P) { p1 = P - 1; } // edge: last phase lerps with itself

        const float* coeff0 = lut + static_cast<std::size_t>(p0) * static_cast<std::size_t>(N);
        const float* coeff1 = lut + static_cast<std::size_t>(p1) * static_cast<std::size_t>(N);

        const float* x = src.getReadPointer(ch);

        double y0 = 0.0;
        double y1 = 0.0;

        for (int tap = 0; tap < N; ++tap)
        {
            const int offset = tap - K;
            int idx = i0 + offset;

            // Wrap indices for looping/circular playback
            idx %= srcN;
            if (idx < 0) idx += srcN;

            const float s = x[idx];
            y0 += static_cast<double>(s) * static_cast<double>(coeff0[tap]);
            y1 += static_cast<double>(s) * static_cast<double>(coeff1[tap]);
        }

        const double y = (1.0 - mu) * y0 + mu * y1;
        return static_cast<float>(y);
    }


    //--------------------------------------------------------------------------
    // Interpolation: Catmull-Rom / cubic Hermite (looping source)

    // Your existing 4-point cubic Hermite (Catmull-Rom) as a function.
    float interpolateHermiteCatmullRom(
        const juce::AudioBuffer<float>& src,
        int ch,
        double playhead,
        int srcN
    ) noexcept
    {
        const int index1 = static_cast<int>(playhead);
        const int index0 = (index1 - 1 + srcN) % srcN;
        const int index2 = (index1 + 1) % srcN;
        const int index3 = (index1 + 2) % srcN;

        const double frac = playhead - static_cast<double>(index1);
        const double frac2 = frac * frac;
        const double frac3 = frac2 * frac;

        const float* x = src.getReadPointer(ch);

        const float y0 = x[index0];
        const float y1 = x[index1];
        const float y2 = x[index2];
        const float y3 = x[index3];

        const double a0 = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
        const double a1 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double a2 = -0.5 * y0 + 0.5 * y2;
        const double a3 = y1;

        return static_cast<float>(a0 * frac3 + a1 * frac2 + a2 * frac + a3);
    }

} // namespace lut_sinc
