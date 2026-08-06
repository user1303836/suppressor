#pragma once

// One-split denoiser core (per channel).
//
// Signal flow:
//   x -> [LR4 crossover] -> low branch (always passed)
//                      \-> high branch -> x gate gain -> +
// The gate detector taps |high| (the high branch itself) by default, or a
// separately high-pass-filtered external key signal. The result is a causal
// dynamic low-pass: the body always passes, the high branch opens almost
// instantly for genuine high-frequency events, and a transient cue can
// force the branch open for a few milliseconds.

#include "Crossover.h"
#include "EnvelopeGate.h"
#include <cmath>

namespace suppressor
{
class OneSplitDenoiser
{
public:
    void prepare (double fs) noexcept
    {
        sampleRate = fs;
        crossover.prepare (fs);
        gate.prepare (fs);
        detHp1.reset();
        detHp2.reset();
        reset();
    }

    void setCutoff (double fcHz) noexcept
    {
        crossover.setCutoff (fcHz);
        const auto hp = butterworthHp2 (fcHz, sampleRate);
        detHp1.setCoeffs (hp);
        detHp2.setCoeffs (hp);
    }

    double cutoff() const noexcept { return crossover.getCutoff(); }

    void setGateParams (const GateParams& p) noexcept { gate.setParams (p); }

    // When the detector source differs from the audio entering the
    // crossover (external sidechain key, or a pre-delay tap when lookahead
    // is active), a dedicated detector filter is engaged.
    void setSeparateDetectorPath (bool separate) noexcept { separateDetector = separate; }

    EnvelopeGate& envelopeGate() noexcept { return gate; }

    void reset() noexcept
    {
        crossover.reset();
        detHp1.reset();
        detHp2.reset();
        gate.reset();
        lastLow_ = lastHigh_ = lastDet_ = 0.0f;
    }

    // x:      audio sample (already lookahead-delayed by the engine)
    // detSrc: undelayed detector source (internal input or external key)
    // cueOpen: transient cue forces the gate target open this sample
    inline float process (float x, float detSrc, bool cueOpen) noexcept
    {
        float low, high;
        crossover.process (x, low, high);

        float det;
        if (separateDetector)
            det = std::fabs (detHp2.process (detHp1.process (detSrc)));
        else
            det = std::fabs (high);

        float g;
        if (cueOpen)
        {
            // force the gate fully open at attack speed
            g = gate.processForcedOpen();
        }
        else
        {
            g = gate.process (det);
        }

        lastLow_ = low;
        lastHigh_ = high;
        lastDet_ = det;
        return low + g * high;
    }

    float lastDetector() const noexcept { return lastDet_; }
    float lastGain() const noexcept { return gate.currentGain(); }
    float lastHighBranch() const noexcept { return lastHigh_; }
    float lastLowBranch() const noexcept { return lastLow_; }

private:
    double sampleRate = 48000.0;
    CrossoverLR4 crossover;
    BiquadTDF2 detHp1, detHp2;
    EnvelopeGate gate;
    bool separateDetector = false;
    float lastLow_ = 0.0f, lastHigh_ = 0.0f, lastDet_ = 0.0f;
};

} // namespace suppressor
