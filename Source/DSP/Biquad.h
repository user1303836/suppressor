#pragma once

// Biquad filtering primitives.
//
// Coefficients are always computed in double precision and stored/executed
// in single precision using the transposed-direct-form-II structure (two
// state registers per section). All math is sample-domain and causal.

#include <cmath>

namespace suppressor
{
constexpr double kPi    = 3.14159265358979323846;
constexpr double kSqrt2 = 1.41421356237309504880;

struct BiquadCoeffs
{
    double b0 = 0.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// Second-order Butterworth low-pass (Q = 1/sqrt(2)), bilinear transform.
inline BiquadCoeffs butterworthLp2 (double fcHz, double fs) noexcept
{
    const double K  = std::tan (kPi * fcHz / fs);
    const double K2 = K * K;
    const double n  = 1.0 / (1.0 + kSqrt2 * K + K2);
    return { K2 * n, 2.0 * K2 * n, K2 * n,
             2.0 * (K2 - 1.0) * n,
             (1.0 - kSqrt2 * K + K2) * n };
}

// Second-order Butterworth high-pass (Q = 1/sqrt(2)), bilinear transform.
inline BiquadCoeffs butterworthHp2 (double fcHz, double fs) noexcept
{
    const double K  = std::tan (kPi * fcHz / fs);
    const double K2 = K * K;
    const double n  = 1.0 / (1.0 + kSqrt2 * K + K2);
    return { n, -2.0 * n, n,
             2.0 * (K2 - 1.0) * n,
             (1.0 - kSqrt2 * K + K2) * n };
}

// Resonant dip with finite depth (a "partial notch"), minimum phase.
// depthDb > 0 produces a cut of depthDb dB exactly at fc.
inline BiquadCoeffs resonantDip (double fcHz, double Q, double depthDb, double fs) noexcept
{
    const double A     = std::pow (10.0, -depthDb / 40.0);
    const double w0    = 2.0 * kPi * fcHz / fs;
    const double alpha = std::sin (w0) / (2.0 * Q);
    const double cw    = std::cos (w0);
    const double a0    = 1.0 + alpha / A;
    return { (1.0 + alpha * A) / a0, (-2.0 * cw) / a0, (1.0 - alpha * A) / a0,
             (-2.0 * cw) / a0, (1.0 - alpha / A) / a0 };
}

// Complex transfer function of a biquad at normalised frequency w = 2*pi*f/fs.
inline void biquadResponse (const BiquadCoeffs& c, double w, double& mag, double& phaseRad) noexcept
{
    const double c1 = std::cos (w), s1 = std::sin (w);
    const double c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
    const double nr = c.b0 + c.b1 * c1 + c.b2 * c2;
    const double ni = -(c.b1 * s1 + c.b2 * s2);
    const double dr = 1.0 + c.a1 * c1 + c.a2 * c2;
    const double di = -(c.a1 * s1 + c.a2 * s2);
    const double denom = dr * dr + di * di;
    const double rr = (nr * dr + ni * di) / denom;
    const double ri = (ni * dr - nr * di) / denom;
    mag      = std::sqrt (rr * rr + ri * ri);
    phaseRad = std::atan2 (ri, rr);
}

class BiquadTDF2
{
public:
    void setCoeffs (const BiquadCoeffs& c) noexcept
    {
        b0 = (float) c.b0; b1 = (float) c.b1; b2 = (float) c.b2;
        a1 = (float) c.a1; a2 = (float) c.a2;
    }

    void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

} // namespace suppressor
