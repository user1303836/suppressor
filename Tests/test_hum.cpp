// Learn-and-lock hum suppression.
#include <doctest/doctest.h>
#include "TestUtil.h"
#include "../Source/DSP/HumSuppressor.h"

using namespace suppressor;

static double goertzelAmp (const std::vector<float>& x, double f, double fs, int from, int to)
{
    const double w = 2.0 * kPi * f / fs;
    const double cw = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (int i = from; i < to; ++i)
    {
        const double s0 = (double) x[(size_t) i] + cw * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double p = s1 * s1 + s2 * s2 - cw * s1 * s2;
    return 2.0 * std::sqrt (std::max (p, 0.0)) / (to - from);
}

TEST_CASE ("learn detects 60 Hz family and locks narrow dips; program passes otherwise")
{
    const double fs = 48000.0;
    const int nLearn = (int) fs; // 1 second of noise-only material

    HumSuppressor hum;
    hum.prepare (fs);
    hum.setBaseHz (0.0); // auto
    hum.setNumHarmonics (8);
    hum.setStrength (1.0);
    hum.setEnabled (true);

    // noise-only capture: 60 Hz + harmonics, no program content
    std::vector<float> noise ((size_t) nLearn);
    for (int i = 0; i < nLearn; ++i)
    {
        const double t = i / fs;
        noise[(size_t) i] = (float) (0.05 * std::sin (2 * kPi * 60.0 * t)
                                   + 0.03 * std::sin (2 * kPi * 120.0 * t)
                                   + 0.02 * std::sin (2 * kPi * 180.0 * t)
                                   + 0.015 * std::sin (2 * kPi * 240.0 * t));
    }

    hum.startLearn();
    for (int i = 0; i < nLearn; ++i) hum.process (noise[(size_t) i]);
    hum.finishLearn();

    REQUIRE (hum.locked());
    CHECK (hum.baseHz() == doctest::Approx (60.0));
    CHECK (hum.activeHarmonicCount() >= 4);

    // post-learn: same hum + a 1 kHz program tone
    const int n = (int) fs;
    std::vector<float> in ((size_t) n), out ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        in[(size_t) i] = (float) (0.05 * std::sin (2 * kPi * 60.0 * t)
                                + 0.03 * std::sin (2 * kPi * 120.0 * t)
                                + 0.02 * std::sin (2 * kPi * 180.0 * t)
                                + 0.015 * std::sin (2 * kPi * 240.0 * t)
                                + 0.10 * std::sin (2 * kPi * 1000.0 * t));
    }
    for (int i = 0; i < n; ++i) out[(size_t) i] = hum.process (in[(size_t) i]);

    const int skip = 4800; // allow the dips to settle
    const double in60 = goertzelAmp (in, 60.0, fs, skip, n);
    const double out60 = goertzelAmp (out, 60.0, fs, skip, n);
    const double redDb = 20.0 * std::log10 (out60 / in60);
    CHECK (redDb < -40.0); // deep, learned dip at the fundamental

    const double out120 = goertzelAmp (out, 120.0, fs, skip, n);
    const double in120 = goertzelAmp (in, 120.0, fs, skip, n);
    CHECK (20.0 * std::log10 (out120 / in120) < -30.0);

    // program content survives essentially untouched
    const double in1k = goertzelAmp (in, 1000.0, fs, skip, n);
    const double out1k = goertzelAmp (out, 1000.0, fs, skip, n);
    CHECK (20.0 * std::log10 (out1k / in1k) == doctest::Approx (0.0).epsilon (0.05));
}

TEST_CASE ("learn with insufficient material does not lock")
{
    const double fs = 48000.0;
    HumSuppressor hum;
    hum.prepare (fs);
    hum.startLearn();
    for (int i = 0; i < 4800; ++i) hum.process (0.01f); // 0.1 s only
    hum.finishLearn();
    CHECK_FALSE (hum.locked());
    CHECK (hum.activeHarmonicCount() == 0);
}

TEST_CASE ("strength scales dip depth without re-learning")
{
    const double fs = 48000.0;
    const int nLearn = (int) fs;
    HumSuppressor hum;
    hum.prepare (fs);
    hum.setBaseHz (60.0);
    hum.setNumHarmonics (4);
    hum.setStrength (1.0);
    hum.setEnabled (true);

    std::vector<float> noise ((size_t) nLearn);
    for (int i = 0; i < nLearn; ++i)
        noise[(size_t) i] = (float) (0.05 * std::sin (2 * kPi * 60.0 * i / fs));
    hum.startLearn();
    for (int i = 0; i < nLearn; ++i) hum.process (noise[(size_t) i]);
    hum.finishLearn();
    REQUIRE (hum.locked());

    const double fullDepth = hum.harmonicDepthDb (0);
    hum.setStrength (0.5);
    CHECK (hum.harmonicDepthDb (0) == doctest::Approx (fullDepth * 0.5));
}
