#pragma once

// Branch-gain envelope gate with:
//  - separate open/close thresholds (level hysteresis),
//  - a retriggerable hold timer,
//  - asymmetric exponential attack/release smoothing (e-folding constants),
//  - a finite reduction floor (Reduction mode) or hard zero (Tight Gate mode),
//  - a near-unity snap while opening, and
//  - optional program-dependent release time selected by the detector's
//    short-term crest factor (spiky/transient material -> fast release,
//    smooth/sustained material -> slow release). The release coefficient is
//    slewed slowly toward its selected value so the adaptation itself is
//    click-free.

#include <cmath>
#include <algorithm>

namespace suppressor
{
struct GateParams
{
    double openThresholdDb = -40.0;
    double hysteresisDb    = 6.0;
    double holdMs          = 2.0;
    double attackMs        = 0.01;   // 10 us e-folding
    double releaseMs       = 6.0;    // e-folding
    double floorDb         = -60.0;  // Reduction-mode floor
    bool   tightGate       = false;  // true: floor is hard zero (Tight Gate)
    bool   adaptiveRelease = false;
    double adaptiveMaxMs   = 60.0;   // adaptive range is [releaseMs, adaptiveMaxMs]
};

class EnvelopeGate
{
public:
    void prepare (double fs) noexcept
    {
        sampleRate = fs;
        applyDerived();
        reset();
    }

    void setParams (const GateParams& p) noexcept
    {
        params = p;
        applyDerived();
    }

    const GateParams& getParams() const noexcept { return params; }

    void reset() noexcept
    {
        gain = 0.0f;
        holdCounter = 0;
        isOpen = false;
        peakEnv = 0.0f;
        rmsEnv = 0.0f;
        currentReleaseA = releaseA;
    }

    inline float process (float detAbs) noexcept
    {
        // ---- state machine: hysteresis + retriggerable hold -----------------
        const bool above = detAbs > (float) openLin;
        const bool below = detAbs < (float) closeLin;

        if (above)
        {
            isOpen = true;
            holdCounter = holdSamples;
        }
        else if (! below && isOpen)
        {
            // inside the hysteresis band while open: keep refreshing the hold
            holdCounter = holdSamples;
        }
        else if (below)
        {
            if (holdCounter > 0)
                --holdCounter;
            else
                isOpen = false;
        }

        const float target = isOpen ? 1.0f : (float) floorLin;

        // ---- adaptive release selection (crest-based) -----------------------
        if (params.adaptiveRelease)
            updateAdaptive (detAbs);
        else
            currentReleaseA = releaseA;

        // ---- asymmetric one-pole gain smoothing -----------------------------
        const double a = isOpen ? attackA : currentReleaseA;
        gain = (float) (a * gain + (1.0 - a) * target);

        // near-unity snap while opening
        if (isOpen && gain > 0.999f)
            gain = 1.0f;

        return gain;
    }

    // Force the gate open for this sample at attack speed (transient cue).
    inline float processForcedOpen() noexcept
    {
        isOpen = true;
        holdCounter = holdSamples;
        gain = (float) (attackA * gain + (1.0 - attackA) * 1.0f);
        if (gain > 0.999f)
            gain = 1.0f;
        return gain;
    }

    float currentGain() const noexcept { return gain; }
    bool open() const noexcept { return isOpen; }

    // For metering/adaptation insight
    float crestDb() const noexcept { return lastCrestDb; }
    double currentReleaseMs() const noexcept { return lastReleaseMs; }

private:
    void applyDerived() noexcept
    {
        openLin  = std::pow (10.0, params.openThresholdDb / 20.0);
        closeLin = std::pow (10.0, (params.openThresholdDb - params.hysteresisDb) / 20.0);
        floorLin = params.tightGate ? 0.0
                                    : std::pow (10.0, std::min (0.0, params.floorDb) / 20.0);
        holdSamples = (int) std::floor (0.001 * params.holdMs * sampleRate);
        attackA  = std::exp (-1.0 / (0.001 * params.attackMs * sampleRate));
        releaseA = std::exp (-1.0 / (0.001 * params.releaseMs * sampleRate));
        if (! params.adaptiveRelease)
            currentReleaseA = releaseA;

        peakDecayA = std::exp (-1.0 / (0.020 * sampleRate));
        rmsA       = std::exp (-1.0 / (0.050 * sampleRate));
        morphA     = std::exp (-1.0 / (0.200 * sampleRate));
    }

    inline void updateAdaptive (float detAbs) noexcept
    {
        // fast-attack/slow-decay peak follower
        peakEnv = std::max (detAbs, (float) (peakDecayA * peakEnv));
        // slow mean-square follower
        rmsEnv = (float) (rmsA * rmsEnv + (1.0 - rmsA) * (double) (detAbs * detAbs));

        const float rms = std::sqrt (std::max (rmsEnv, 1.0e-12f));
        const float crest = 20.0f * std::log10 ((peakEnv + 1.0e-9f) / (rms + 1.0e-9f));
        lastCrestDb = crest;

        // t = 1 -> spiky (fast release), t = 0 -> smooth sustain (slow release)
        const double t = std::clamp ((crest - 3.0f) / 15.0f, 0.0f, 1.0f);
        const double maxMs = std::max (params.adaptiveMaxMs, params.releaseMs + 0.001);
        const double targetMs = params.releaseMs * std::pow (maxMs / params.releaseMs, 1.0 - t);
        lastReleaseMs = targetMs;

        const double targetA = std::exp (-1.0 / (0.001 * targetMs * sampleRate));
        currentReleaseA = morphA * currentReleaseA + (1.0 - morphA) * targetA;
    }

    double sampleRate = 48000.0;
    GateParams params;

    double openLin = 0.01, closeLin = 0.005, floorLin = 0.0;
    double attackA = 0.0, releaseA = 0.0, currentReleaseA = 0.0;
    double peakDecayA = 0.0, rmsA = 0.0, morphA = 0.0;
    int holdSamples = 0, holdCounter = 0;
    float gain = 0.0f;
    bool isOpen = false;
    float peakEnv = 0.0f, rmsEnv = 0.0f;
    float lastCrestDb = 0.0f;
    double lastReleaseMs = 6.0;
};

} // namespace suppressor
