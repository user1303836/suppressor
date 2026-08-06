#pragma once

// Simple one-pole smoother (e-folding time constant form):
//   y[n] = a*y[n-1] + (1-a)*x[n],  a = exp(-1/(tau*fs)).

#include <cmath>

namespace suppressor
{
class OnePole
{
public:
    void prepare (double fs) noexcept { sampleRate = fs; }

    void setTimeConstantMs (double ms) noexcept
    {
        a = std::exp (-1.0 / (0.001 * ms * sampleRate));
    }

    void setCoeff (double coeff) noexcept { a = coeff; }

    double coeff() const noexcept { return a; }

    inline float process (float x) noexcept
    {
        y = (float) (a * y + (1.0 - a) * x);
        return y;
    }

    float value() const noexcept { return y; }
    void reset (float v = 0.0f) noexcept { y = v; }

private:
    double sampleRate = 48000.0;
    double a = 0.0;
    float y = 0.0f;
};

} // namespace suppressor
