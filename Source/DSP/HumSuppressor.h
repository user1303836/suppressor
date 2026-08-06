#pragma once

// Learn-and-lock hum/buzz suppressor.
//
// On a Learn trigger the module captures up to two seconds of (ideally
// noise-only) input, measures the 50/60 Hz family content with Goertzel
// analysis, and configures a cascade of minimum-phase resonant dips at the
// measured harmonic frequencies. Per-harmonic dip depth is derived from the
// measured level so that the expected residual lands near -80 dBFS, then
// scaled by the Strength control. Dips use Q=64: narrow enough that adjacent
// program content is essentially untouched (skirt < 0.1 dB a few hundred Hz
// away) while mains drift is covered by +/-3% frequency refinement at learn. After learning, the measurement is
// locked; Strength and harmonic-count changes re-derive coefficients from
// the stored measurement without a new capture. Changing the base family
// (50/60/auto) requires a new learn.

#include "Biquad.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace suppressor
{
class HumSuppressor
{
public:
    static constexpr int kMaxHarmonics = 16;

    void prepare (double fs)
    {
        sampleRate = fs;
        capture.assign ((size_t) (2.0 * fs), 0.0f);
        capturePos = 0;
        resetDipStates();
    }

    void setEnabled (bool e) noexcept          { enabled = e; }
    bool isEnabled() const noexcept            { return enabled; }
    void setStrength (double s01) noexcept
    {
        strength = std::clamp (s01, 0.0, 1.0);
        if (isLocked)
            recomputeDips();
    }
    void setNumHarmonics (int n) noexcept
    {
        numHarmonics = std::clamp (n, 1, kMaxHarmonics);
        if (isLocked)
            recomputeDips();
    }
    // baseHz: 50, 60, or 0 = auto-detect during learn
    void setBaseHz (double hz) noexcept
    {
        if (hz != requestedBase)
        {
            requestedBase = hz;
            isLocked = false; // family change requires a new learn
            for (int i = 0; i < kMaxHarmonics; ++i) dipActive[i] = false;
        }
    }

    void startLearn() noexcept
    {
        capturePos = 0;
        isLearning = true;
        isLocked = false;
    }

    bool learning() const noexcept { return isLearning; }
    bool locked() const noexcept   { return isLocked; }
    double baseHz() const noexcept { return detectedBase; }
    int activeHarmonicCount() const noexcept
    {
        int c = 0;
        for (int i = 0; i < kMaxHarmonics; ++i) if (dipActive[i]) ++c;
        return c;
    }
    double harmonicFreq (int i) const noexcept   { return (i >= 0 && i < kMaxHarmonics) ? measFreq[i] : 0.0; }
    double harmonicAmpDb (int i) const noexcept  { return (i >= 0 && i < kMaxHarmonics) ? measAmpDb[i] : -999.0; }
    double harmonicDepthDb (int i) const noexcept { return (i >= 0 && i < kMaxHarmonics) ? dipDepthDb[i] : 0.0; }
    bool harmonicActive (int i) const noexcept    { return (i >= 0 && i < kMaxHarmonics) && dipActive[i]; }

    void reset() noexcept { resetDipStates(); }

    inline float process (float x) noexcept
    {
        if (isLearning)
        {
            if (capturePos < capture.size())
                capture[capturePos++] = x;
            if (capturePos >= capture.size())
                finishLearn();
            return x;
        }

        if (! enabled || ! isLocked)
            return x;

        float y = x;
        for (int i = 0; i < kMaxHarmonics; ++i)
            if (dipActive[i])
                y = dips[i].process (y);
        return y;
    }

    void finishLearn() noexcept
    {
        if (! isLearning)
            return;
        isLearning = false;

        const size_t n = capturePos;
        const size_t minNeeded = (size_t) (0.5 * sampleRate);
        if (n < minNeeded)
        {
            // not enough material: abort learn, stay unlocked
            for (int i = 0; i < kMaxHarmonics; ++i) dipActive[i] = false;
            return;
        }

        // ---- choose base family ---------------------------------------------
        if (requestedBase > 0.0)
        {
            detectedBase = requestedBase;
        }
        else
        {
            double p50 = 0.0, p60 = 0.0;
            for (int k = 1; k <= 8; ++k)
            {
                p50 += goertzelPower (capture.data(), n, 50.0 * k);
                p60 += goertzelPower (capture.data(), n, 60.0 * k);
            }
            detectedBase = (p60 >= p50) ? 60.0 : 50.0;
        }

        // ---- measure every harmonic -----------------------------------------
        for (int k = 1; k <= kMaxHarmonics; ++k)
        {
            const int i = k - 1;
            const double fNominal = detectedBase * (double) k;
            if (fNominal > 0.45 * sampleRate)
            {
                measFreq[i] = fNominal;
                measAmpDb[i] = -999.0;
                continue;
            }

            // refine frequency within +/-3 % (mains drift / detune)
            double bestF = fNominal, bestP = -1.0;
            for (double ratio = 0.97; ratio <= 1.030001; ratio += 0.005)
            {
                const double f = fNominal * ratio;
                const double p = goertzelPower (capture.data(), n, f);
                if (p > bestP) { bestP = p; bestF = f; }
            }

            const double amp = 2.0 * std::sqrt (std::max (bestP, 0.0)) / (double) n;
            measFreq[i] = bestF;
            measAmpDb[i] = 20.0 * std::log10 (amp + 1.0e-12);
        }

        isLocked = true;
        recomputeDips();
    }

private:
    void recomputeDips() noexcept
    {
        for (int k = 1; k <= kMaxHarmonics; ++k)
        {
            const int i = k - 1;
            const bool inRange = k <= numHarmonics;
            const bool measurable = measAmpDb[i] > -70.0;
            dipActive[i] = inRange && measurable;
            if (dipActive[i])
            {
                // depth: bring measured level down toward ~-80 dBFS, scaled
                const double target = std::clamp (measAmpDb[i] + 80.0, 6.0, 60.0);
                dipDepthDb[i] = target * strength;
                dips[i].setCoeffs (resonantDip (measFreq[i], 64.0, dipDepthDb[i], sampleRate));
                dips[i].reset();
            }
            else
            {
                dipDepthDb[i] = 0.0;
            }
        }
    }

    void resetDipStates() noexcept
    {
        for (int i = 0; i < kMaxHarmonics; ++i) dips[i].reset();
    }

    double goertzelPower (const float* x, size_t n, double f) const noexcept
    {
        const double w  = 2.0 * kPi * f / sampleRate;
        const double cw = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double s0 = (double) x[i] + cw * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return s1 * s1 + s2 * s2 - cw * s1 * s2;
    }

    double sampleRate = 48000.0;
    bool enabled = false, isLearning = false, isLocked = false;
    double requestedBase = 0.0;
    double detectedBase = 60.0;
    int numHarmonics = 8;
    double strength = 1.0;

    BiquadTDF2 dips[kMaxHarmonics];
    bool dipActive[kMaxHarmonics] = {};
    double dipDepthDb[kMaxHarmonics] = {};
    double measFreq[kMaxHarmonics] = {};
    double measAmpDb[kMaxHarmonics] = {};

    std::vector<float> capture;
    size_t capturePos = 0;
};

} // namespace suppressor
