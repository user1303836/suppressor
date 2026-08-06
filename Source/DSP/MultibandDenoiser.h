#pragma once

// Conservative learned-threshold multiband mode (4 or 6 bands).
//
// A tree of LR4 crossovers splits the signal; each band gets its own
// envelope gate with independently learned thresholds (Learn captures a
// noise-only passage and sets per-band open thresholds just above the
// measured band peak). Timing (hysteresis, hold, attack, release, floor)
// is shared across bands. With every band open the band sum approximates
// an all-pass of the input (each LR4 pair sums flat); exact magnitude
// flatness is not claimed near crossover frequencies -- this mode is
// offered as an advanced option, while the one-split core remains the
// default.

#include "Crossover.h"
#include "EnvelopeGate.h"
#include <cmath>
#include <algorithm>

namespace suppressor
{
class MultibandDenoiser
{
public:
    static constexpr int kMaxBands = 6;

    void prepare (double fs)
    {
        sampleRate = fs;
        for (auto& s : splits) s.prepare (fs);
        for (auto& s : keySplits) s.prepare (fs);
        for (auto& g : gates) g.prepare (fs);
        setBandCount (bandCount);
        reset();
    }

    void setBandCount (int n) noexcept
    {
        bandCount = std::clamp (n, 2, kMaxBands);
        const double* f = (bandCount >= 6) ? freqs6 : freqs4;
        const int nSplits = bandCount - 1;
        for (int i = 0; i < kMaxBands - 1; ++i)
        {
            const double fc = (i < nSplits) ? f[i] : 4000.0;
            splits[i].setCutoff (fc);
            keySplits[i].setCutoff (fc);
        }
    }

    int bands() const noexcept { return bandCount; }

    // Shared timing for all bands (thresholds are per-band via learn or setter)
    void setGateTiming (const GateParams& p) noexcept
    {
        sharedTiming = p;
        for (int b = 0; b < kMaxBands; ++b)
        {
            auto gp = p;
            gp.openThresholdDb = bandThrDb[b];
            gates[b].setParams (gp);
        }
    }

    void setBandThresholdDb (int b, double db) noexcept
    {
        if (b < 0 || b >= kMaxBands) return;
        bandThrDb[b] = db;
        auto gp = sharedTiming;
        gp.openThresholdDb = db;
        gates[b].setParams (gp);
    }

    double bandThresholdDb (int b) const noexcept
    {
        return (b >= 0 && b < kMaxBands) ? bandThrDb[b] : 0.0;
    }

    void setExternalKey (bool ext) noexcept { externalKey = ext; }

    void startLearn() noexcept
    {
        for (int b = 0; b < kMaxBands; ++b) learnMax[b] = 0.0f;
        isLearning = true;
    }

    void finishLearn() noexcept
    {
        if (! isLearning) return;
        isLearning = false;
        for (int b = 0; b < kMaxBands; ++b)
        {
            const float peak = learnMax[b];
            const double peakDb = (peak > 1.0e-9f)
                ? 20.0 * std::log10 ((double) peak)
                : -120.0;
            setBandThresholdDb (b, std::clamp (peakDb + 3.0, -80.0, -20.0));
        }
        isLocked = true;
    }

    bool learning() const noexcept { return isLearning; }
    bool locked() const noexcept   { return isLocked; }

    void reset() noexcept
    {
        for (auto& s : splits) s.reset();
        for (auto& s : keySplits) s.reset();
        for (auto& g : gates) g.reset();
        for (int b = 0; b < kMaxBands; ++b) { lastBandGain[b] = 0.0f; lastBandDet[b] = 0.0f; }
    }

    inline float process (float x, float key, bool cueOpen) noexcept
    {
        float band[kMaxBands];
        float kb = key;
        float r = x;
        const int nSplits = bandCount - 1;
        for (int i = 0; i < nSplits; ++i)
        {
            splits[i].process (r, band[i], r);
            if (externalKey)
            {
                float lo, hi;
                keySplits[i].process (kb, lo, hi);
                keyBand[i] = lo;
                kb = hi;
            }
        }
        band[nSplits] = r;
        if (externalKey)
            keyBand[nSplits] = kb;

        if (isLearning)
        {
            for (int b = 0; b < bandCount; ++b)
            {
                const float d = std::fabs (externalKey ? keyBand[b] : band[b]);
                if (d > learnMax[b]) learnMax[b] = d;
            }
            return x; // pass through unmodified while learning
        }

        float y = 0.0f;
        for (int b = 0; b < bandCount; ++b)
        {
            const float det = std::fabs (externalKey ? keyBand[b] : band[b]);
            const float g = cueOpen ? gates[b].processForcedOpen()
                                    : gates[b].process (det);
            lastBandGain[b] = g;
            lastBandDet[b] = det;
            y += g * band[b];
        }
        return y;
    }

    float bandGain (int b) const noexcept { return (b >= 0 && b < kMaxBands) ? lastBandGain[b] : 0.0f; }
    float bandDetector (int b) const noexcept { return (b >= 0 && b < kMaxBands) ? lastBandDet[b] : 0.0f; }
    float minGain() const noexcept
    {
        float m = 1.0f;
        for (int b = 0; b < bandCount; ++b) m = std::min (m, lastBandGain[b]);
        return m;
    }

private:
    static constexpr double freqs4[3] = { 250.0, 1000.0, 4000.0 };
    static constexpr double freqs6[5] = { 150.0, 350.0, 800.0, 1800.0, 4500.0 };

    double sampleRate = 48000.0;
    int bandCount = 4;
    CrossoverLR4 splits[kMaxBands - 1];
    CrossoverLR4 keySplits[kMaxBands - 1];
    EnvelopeGate gates[kMaxBands];
    GateParams sharedTiming;
    double bandThrDb[kMaxBands] = { -40.0, -40.0, -40.0, -40.0, -40.0, -40.0 };
    float keyBand[kMaxBands] = {};
    float learnMax[kMaxBands] = {};
    float lastBandGain[kMaxBands] = {};
    float lastBandDet[kMaxBands] = {};
    bool externalKey = false;
    bool isLearning = false, isLocked = false;
};

} // namespace suppressor
