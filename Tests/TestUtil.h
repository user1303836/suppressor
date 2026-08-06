#pragma once
// Shared helpers for DSP unit tests.
#include <cmath>
#include <vector>
#include <functional>
#include "../Source/DSP/Biquad.h"

namespace testutil
{
inline std::vector<float> sine (double freq, double fs, double ampDb, int n, double phase0 = 0.0)
{
    std::vector<float> x ((size_t) n);
    const double a = std::pow (10.0, ampDb / 20.0);
    const double w = 2.0 * suppressor::kPi * freq / fs;
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = (float) (a * std::sin (w * i + phase0));
    return x;
}

inline std::vector<float> impulse (int n, int at = 0, float amp = 1.0f)
{
    std::vector<float> x ((size_t) n, 0.0f);
    if (at >= 0 && at < n) x[(size_t) at] = amp;
    return x;
}

inline double rms (const std::vector<float>& x, int from, int to)
{
    double s = 0.0;
    for (int i = from; i < to; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return std::sqrt (s / std::max (1, to - from));
}

// steady-state magnitude of a biquad cascade at frequency f
inline double cascadeMagDb (const std::vector<suppressor::BiquadCoeffs>& secs, double f, double fs)
{
    double mag = 1.0, ph = 0.0;
    for (auto& c : secs)
    {
        double m, p;
        suppressor::biquadResponse (c, 2.0 * suppressor::kPi * f / fs, m, p);
        mag *= m;
        ph += p;
    }
    return 20.0 * std::log10 (mag);
}

// run a mono engine-like functor over x in blocks of blockSize; returns y
template <typename ProcessFn>
inline std::vector<float> renderBlocked (const std::vector<float>& x, int blockSize, ProcessFn&& resetAndProcess)
{
    std::vector<float> y (x.size(), 0.0f);
    resetAndProcess (x, y, blockSize);
    return y;
}
} // namespace testutil
