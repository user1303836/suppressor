// Gate timing: attack, hold, release, hysteresis, depth floor, snap,
// adaptive (program-dependent) release.
#include <doctest/doctest.h>
#include "TestUtil.h"
#include "../Source/DSP/EnvelopeGate.h"

using namespace suppressor;

static EnvelopeGate makeGate (double fs, const GateParams& p)
{
    EnvelopeGate g;
    g.prepare (fs);
    g.setParams (p);
    return g;
}

TEST_CASE ("attack: 10 us e-folding sequence and near-unity snap at 48 kHz")
{
    GateParams p;
    p.openThresholdDb = -40.0;
    p.holdMs = 2.0;
    auto g = makeGate (48000.0, p);

    const float det = 0.05f; // above threshold (0.01)
    const double g1 = g.process (det);
    const double g2 = g.process (det);
    const double g3 = g.process (det);
    const double g4 = g.process (det);

    CHECK (g1 == doctest::Approx (0.8754855).epsilon (1e-5));
    CHECK (g2 == doctest::Approx (0.9844960).epsilon (1e-5));
    CHECK (g3 == doctest::Approx (0.9980697).epsilon (1e-5));
    CHECK (g4 == doctest::Approx (1.0)); // snapped
}

TEST_CASE ("hold: floor(0.002*fs) retriggerable samples at several rates")
{
    for (double fs : { 8000.0, 44100.0, 48000.0, 96000.0, 384000.0 })
    {
        GateParams p;
        p.openThresholdDb = -40.0;
        p.holdMs = 2.0;
        p.attackMs = 10.0; // slow attack so we can observe open state via gain growth
        p.releaseMs = 1.0;
        auto g = makeGate (fs, p);

        const float above = 0.05f, below = 0.001f;
        // single above-threshold sample, then below
        g.process (above);
        const int expectedHold = (int) std::floor (0.002 * fs);

        int openSamples = 0;
        for (int n = 0; n < expectedHold + 4; ++n)
        {
            g.process (below);
            if (g.open()) ++openSamples;   // gate state DURING this sample
        }
        CHECK (openSamples == expectedHold);
    }
}

TEST_CASE ("release: fitted e-folding tau matches 2 / 6 / 30 ms")
{
    for (double relMs : { 2.0, 6.0, 30.0 })
    {
        GateParams p;
        p.openThresholdDb = -200.0; // effectively always open while signal present
        p.releaseMs = relMs;
        p.holdMs = 0.0;
        auto g = makeGate (48000.0, p);

        // open fully
        for (int n = 0; n < 64; ++n) g.process (1.0f);
        REQUIRE (g.currentGain() == doctest::Approx (1.0));

        // close: detector at zero. Count samples until gain crosses e^-1.
        const double target = std::exp (-1.0);
        int n = 0;
        while (g.currentGain() > (float) target && n < 48000)
        {
            g.process (0.0f);
            ++n;
        }
        const double fittedMs = 1000.0 * n / 48000.0;
        CHECK (fittedMs == doctest::Approx (relMs).epsilon (0.02));
    }
}

TEST_CASE ("hysteresis: distinct open/close thresholds suppress chatter")
{
    GateParams p;
    p.openThresholdDb = -40.0;  // 0.01
    p.hysteresisDb = 6.0;       // close at -46 dB (0.00501)
    p.holdMs = 0.0;
    auto g = makeGate (48000.0, p);

    const float aboveOpen = 0.012f, midBand = 0.007f, belowClose = 0.004f;

    // closed state: mid-band level must NOT open
    g.process (midBand);
    CHECK_FALSE (g.open());

    // open it
    g.process (aboveOpen);
    CHECK (g.open());

    // mid-band keeps it open
    g.process (midBand);
    CHECK (g.open());

    // below close threshold closes
    g.process (belowClose);
    CHECK_FALSE (g.open());
}

TEST_CASE ("depth floor: Reduction mode keeps a finite floor, Tight Gate is hard zero")
{
    GateParams p;
    p.openThresholdDb = -40.0;
    p.holdMs = 0.0;
    p.releaseMs = 2.0;
    p.tightGate = false;
    p.floorDb = -40.0; // floor 0.01
    auto g = makeGate (48000.0, p);

    for (int n = 0; n < 4800; ++n) g.process (0.0f); // long settle
    CHECK (g.currentGain() == doctest::Approx (0.01).epsilon (0.05));

    p.tightGate = true;
    auto g2 = makeGate (48000.0, p);
    for (int n = 0; n < 4800; ++n) g2.process (0.0f);
    CHECK (g2.currentGain() < 1.0e-6f);
}

TEST_CASE ("adaptive release: spiky program closes faster than smooth program")
{
    auto measureRelease = [&] (bool spiky)
    {
        GateParams p;
        p.openThresholdDb = -60.0;
        p.holdMs = 0.0;
        p.releaseMs = 2.0;
        p.adaptiveRelease = true;
        p.adaptiveMaxMs = 60.0;
        auto g = makeGate (48000.0, p);

        // condition the crest estimator with program material above threshold
        for (int n = 0; n < 9600; ++n)
        {
            float det;
            if (spiky)
                det = (n % 480 == 0) ? 0.5f : 0.001f; // click every 10 ms
            else
                det = 0.05f + 0.001f * std::sin (2.0 * kPi * n / 480.0); // smooth tone-ish
            g.process (det);
        }

        // close the gate on silence and measure time to half gain
        int n = 0;
        const float g0 = g.currentGain();
        while (g.currentGain() > g0 * 0.5f && n < 96000)
        {
            g.process (0.0f);
            ++n;
        }
        return n;
    };

    const int spiky = measureRelease (true);
    const int smooth = measureRelease (false);
    CHECK (spiky < smooth);            // chuggy -> faster cleanup
    CHECK (smooth > 2 * spiky / 3);    // sanity bound
}
