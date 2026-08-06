// One-split core behavior: closed response, detector weighting, latency,
// channel independence.
#include <doctest/doctest.h>
#include "TestUtil.h"
#include "../Source/DSP/DenoiserEngine.h"

using namespace suppressor;

static OneSplitDenoiser makeCore (double fs, double fc, double thrDb)
{
    OneSplitDenoiser c;
    c.prepare (fs);
    c.setCutoff (fc);
    GateParams gp;
    gp.openThresholdDb = thrDb;
    gp.hysteresisDb = 0.0;
    gp.holdMs = 2.0;
    c.setGateParams (gp);
    return c;
}

TEST_CASE ("closed-state response follows the LP4 branch")
{
    const double fs = 48000.0;
    auto c = makeCore (fs, 1400.0, 0.0); // threshold 0 dB: gate stays closed for quiet keys

    for (auto [freq, expectedDb] : { std::pair { 1000.0, -2.000 }, std::pair { 2000.0, -14.343 },
                                     std::pair { 500.0, -0.139 }, std::pair { 4000.0, -37.303 } })
    {
        c.reset();
        auto x = testutil::sine (freq, fs, -20.0, 8192);
        double inRef = testutil::rms (x, 4096, 8192);
        for (int n = 0; n < 4096; ++n) c.process (x[n], x[n], false);
        std::vector<float> y (4096);
        for (int n = 0; n < 4096; ++n) y[n] = c.process (x[4096 + n], x[4096 + n], false);
        const double outDb = 20.0 * std::log10 (testutil::rms (y, 0, 4096) / inRef);
        CHECK (outDb == doctest::Approx (expectedDb).epsilon (0.05));
    }
}

TEST_CASE ("detector taps the high branch: frequency-dependent key thresholds")
{
    const double fs = 48000.0;
    // fc=1400, T=-40 dB. 1 kHz key: HP4 -13.738 dB -> opens at input >= -26.3 dB.
    auto closed = makeCore (fs, 1400.0, -40.0);
    auto opened = makeCore (fs, 1400.0, -40.0);

    auto xc = testutil::sine (1000.0, fs, -28.0, 8192);  // detector -41.7 dB: closed
    auto xo = testutil::sine (1000.0, fs, -25.0, 8192);  // detector -38.7 dB: opens
    float gc = 0.0f, go = 0.0f;
    for (int n = 0; n < 8192; ++n)
    {
        closed.process (xc[n], xc[n], false);
        opened.process (xo[n], xo[n], false);
        gc = closed.lastGain();
        go = opened.lastGain();
    }
    CHECK (gc < 0.01f);
    CHECK (go == doctest::Approx (1.0f));

    // 50 Hz key is heavily rejected by the HP4 detector: never opens even hot
    auto low = makeCore (fs, 1400.0, -40.0);
    auto xl = testutil::sine (50.0, fs, -10.0, 8192);
    for (int n = 0; n < 8192; ++n) low.process (xl[n], xl[n], false);
    CHECK (low.lastGain() < 0.01f);
}

TEST_CASE ("zero-latency impulse alignment")
{
    const double fs = 48000.0;
    DenoiserEngine e;
    e.prepare (fs, 1, 96);
    EngineParams p;
    p.lookaheadSamples = 0;
    e.setTargets (p);

    auto x = testutil::impulse (4096, 1000);
    float* ch[1] = { x.data() };
    e.processBlock (ch, nullptr, 0, 4096, 1);

    int first = -1;
    for (int n = 0; n < 4096; ++n)
        if (x[n] != 0.0f) { first = n; break; }
    CHECK (first == 1000);
    CHECK (e.latencySamples() == 0);
}

TEST_CASE ("channels are independent (dual mono)")
{
    const double fs = 48000.0;
    DenoiserEngine e;
    e.prepare (fs, 2, 96);
    EngineParams p;
    p.strength01 = 1.0;         // fc = 1400
    p.thresholdDb = -40.0;
    p.hysteresisDb = 0.0;
    p.depthDb = 60.0;           // Reduction floor -60 dB (0.001)
    e.setTargets (p);

    const int n = 16384;
    std::vector<float> a = testutil::sine (8000.0, fs, -20.0, n);  // opens (HP4 passes)
    std::vector<float> b = testutil::sine (500.0, fs, -10.0, n);   // rejected by HP4 detector
    float* io[2] = { a.data(), b.data() };
    e.processBlock (io, nullptr, 0, n, 2);

    CHECK (e.channel (0).lastGain() == doctest::Approx (1.0f));
    CHECK (e.channel (1).lastGain() < 0.01f);
}
