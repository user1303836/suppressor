#pragma once

// Fourth-order Linkwitz-Riley style crossover: two cascaded second-order
// Butterworth low-pass sections and two cascaded matching high-pass
// sections sharing one cutoff. At cutoff each branch is -6.02 dB and the
// two branches sum to a flat-magnitude all-pass.

#include "Biquad.h"

namespace suppressor
{
class CrossoverLR4
{
public:
    void prepare (double fs) noexcept
    {
        sampleRate = fs;
        setCutoff (cutoff);
        reset();
    }

    void setCutoff (double fcHz) noexcept
    {
        cutoff = fcHz;
        const auto lp = butterworthLp2 (cutoff, sampleRate);
        const auto hp = butterworthHp2 (cutoff, sampleRate);
        lp1.setCoeffs (lp);
        lp2.setCoeffs (lp);
        hp1.setCoeffs (hp);
        hp2.setCoeffs (hp);
    }

    double getCutoff() const noexcept { return cutoff; }

    void reset() noexcept
    {
        lp1.reset(); lp2.reset();
        hp1.reset(); hp2.reset();
    }

    inline void process (float x, float& low, float& high) noexcept
    {
        low  = lp2.process (lp1.process (x));
        high = hp2.process (hp1.process (x));
    }

private:
    BiquadTDF2 lp1, lp2, hp1, hp2;
    double sampleRate = 48000.0;
    double cutoff     = 1400.0;
};

} // namespace suppressor
