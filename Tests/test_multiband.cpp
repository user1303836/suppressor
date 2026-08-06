// Multiband mode: learned thresholds, reconstruction with open gates,
// closed-gate noise reduction.
#include <doctest/doctest.h>
#include "TestUtil.h"
#include "../Source/DSP/MultibandDenoiser.h"
#include <random>

using namespace suppressor;

TEST_CASE ("multiband learn sets per-band thresholds above the noise floor")
{
    const double fs = 48000.0;
    MultibandDenoiser mb;
    mb.prepare (fs);
    mb.setBandCount (4);
    GateParams gp; // shared timing; thresholds come from learn
    mb.setGateTiming (gp);

    std::mt19937 rng (42);
    std::normal_distribution<float> nd (0.0f, 0.01f); // ~-40 dBFS RMS noise
    mb.startLearn();
    for (int i = 0; i < 8192; ++i) mb.process (nd (rng), 0.0f, false);
    mb.finishLearn();

    REQUIRE (mb.locked());
    for (int b = 0; b < mb.bands(); ++b)
    {
        const double t = mb.bandThresholdDb (b);
        CHECK (t >= -80.0);
        CHECK (t <= -20.0);
        CHECK (t > -60.0); // noise peaks are well above -60 dB in every band
    }
}

TEST_CASE ("multiband closed gates attenuate steady noise")
{
    const double fs = 48000.0;
    MultibandDenoiser mb;
    mb.prepare (fs);
    mb.setBandCount (4);
    GateParams gp;
    gp.floorDb = -60.0;
    gp.tightGate = true;
    mb.setGateTiming (gp);

    std::mt19937 rng (7);
    std::normal_distribution<float> nd (0.0f, 0.01f);
    mb.startLearn();
    for (int i = 0; i < 8192; ++i) mb.process (nd (rng), 0.0f, false);
    mb.finishLearn();
    REQUIRE (mb.locked());

    // continue with the same character of noise: every band should close
    double inE = 0.0, outE = 0.0;
    for (int i = 0; i < 16384; ++i)
    {
        const float x = nd (rng);
        const float y = mb.process (x, 0.0f, false);
        if (i >= 8192)
        {
            inE += (double) x * x;
            outE += (double) y * y;
        }
    }
    CHECK (outE < 0.01 * inE); // at least 20 dB down (Tight Gate, closed)
}

TEST_CASE ("multiband with all bands open reconstructs program material")
{
    const double fs = 48000.0;
    MultibandDenoiser mb;
    mb.prepare (fs);
    mb.setBandCount (6);
    GateParams gp;
    gp.openThresholdDb = -90.0; // everything opens on program material
    gp.hysteresisDb = 0.0;
    mb.setGateTiming (gp);

    // mid-band sine, far from crossover frequencies
    auto x = testutil::sine (3000.0, fs, -20.0, 16384);
    for (int i = 0; i < 8192; ++i) mb.process (x[(size_t) i], 0.0f, false);
    std::vector<float> y (8192);
    for (int i = 0; i < 8192; ++i) y[(size_t) i] = mb.process (x[8192 + (size_t) i], 0.0f, false);

    const double inR = testutil::rms (x, 8192, 16384);
    const double outR = testutil::rms (y, 0, 8192);
    const double errDb = 20.0 * std::log10 (outR / inR);
    // LR-tree band sums are not perfectly flat near crossover regions;
    // the documented worst-case ripple for the 6-band tree is about +/-2 dB.
    // (The one-split core, which IS flat when open, remains the default mode.)
    CHECK (std::fabs (errDb) < 2.0);
}
