// Engine-level invariants: block-partition independence (static AND
// automated parameters), lookahead alignment, delta audition.
#include <doctest/doctest.h>
#include "TestUtil.h"
#include "../Source/DSP/DenoiserEngine.h"

using namespace suppressor;

static std::vector<float> renderEngine (const std::vector<float>& in, int blockSize,
                                        double fs, const EngineParams& p,
                                        bool automateStrength)
{
    constexpr int kEventSample = 1024; // automation lands at this ABSOLUTE sample
    DenoiserEngine e;
    e.prepare (fs, 1, 96);
    EngineParams p0 = p;
    if (automateStrength)
        p0.strength01 = 0.0;
    e.setTargets (p0);

    std::vector<float> x = in;
    std::vector<float> y (x.size(), 0.0f);

    for (size_t off = 0; off < x.size(); off += (size_t) blockSize)
    {
        const int n = (int) std::min ((size_t) blockSize, x.size() - off);
        float* ch[1] = { x.data() + off };

        std::vector<DenoiserEngine::ParamEvent> events;
        if (automateStrength && off <= (size_t) kEventSample && (size_t) kEventSample < off + (size_t) n)
        {
            EngineParams p1 = p;
            p1.strength01 = 1.0;
            events.push_back ({ kEventSample - (int) off, p1 });
        }

        e.processBlock (ch, nullptr, 0, n, 1, events.empty() ? nullptr : &events);
        std::copy (ch[0], ch[0] + n, y.begin() + (long) off);
    }
    return y;
}

TEST_CASE ("output is identical for any host block partitioning")
{
    const double fs = 48000.0;
    const int n = 4096;
    auto in = testutil::sine (3000.0, fs, -24.0, n);

    EngineParams p;
    p.strength01 = 0.7;
    p.thresholdDb = -35.0;
    p.holdMs = 2.0;
    p.releaseMs = 6.0;

    SUBCASE ("static parameters")
    {
        auto ref = renderEngine (in, 256, fs, p, false);
        for (int bs : { 1, 7, 16, 64, 255, 257, 1024 })
        {
            auto y = renderEngine (in, bs, fs, p, false);
            REQUIRE (y.size() == ref.size());
            for (int i = 0; i < n; ++i)
                CHECK (y[(size_t) i] == ref[(size_t) i]);
        }
    }

    SUBCASE ("one sample-accurate parameter event (any partition)")
    {
        auto ref = renderEngine (in, 256, fs, p, true);
        for (int bs : { 1, 7, 16, 64, 255, 257, 1024 })
        {
            auto y = renderEngine (in, bs, fs, p, true);
            REQUIRE (y.size() == ref.size());
            for (int i = 0; i < n; ++i)
                CHECK (y[(size_t) i] == ref[(size_t) i]);
        }
    }
}

TEST_CASE ("lookahead: audio delayed exactly N samples, detector pre-opens")
{
    const double fs = 48000.0;
    const int look = 48; // 1 ms

    DenoiserEngine e;
    e.prepare (fs, 1, 96);
    EngineParams p;
    p.strength01 = 1.0;
    p.thresholdDb = -40.0;
    p.lookaheadSamples = look;
    e.setTargets (p);

    auto x = testutil::impulse (4096, 1000);
    float* ch[1] = { x.data() };
    e.processBlock (ch, nullptr, 0, 4096, 1);

    int first = -1;
    for (int n = 0; n < 4096; ++n)
        if (x[n] != 0.0f) { first = n; break; }
    CHECK (first == 1000 + look);
    CHECK (e.latencySamples() == look);
}

TEST_CASE ("lookahead pre-open preserves a soft high-frequency onset")
{
    const double fs = 48000.0;
    const int n = 8192, burstStart = 2000, look = 48, rampLen = 48;

    // 8 kHz burst with a 1 ms raised-cosine onset ramp: the detector only
    // crosses threshold near the top of the ramp, so without lookahead the
    // ramp itself is gated away; with lookahead the gate is already open.
    auto makeInput = [&]
    {
        std::vector<float> x ((size_t) n, 0.0f);
        const double w = 2.0 * kPi * 8000.0 / fs;
        const double a = std::pow (10.0, -20.0 / 20.0);
        for (int i = burstStart; i < n; ++i)
        {
            const double t = std::min (1.0, (double) (i - burstStart) / rampLen);
            const double env = 0.5 - 0.5 * std::cos (kPi * t);
            x[(size_t) i] = (float) (a * env * std::sin (w * i));
        }
        return x;
    };

    // gate gain at the absolute time the burst audio ARRIVES at the core
    auto gainAtArrival = [&] (int lookahead)
    {
        DenoiserEngine e;
        e.prepare (fs, 1, 96);
        EngineParams p;
        p.strength01 = 1.0;      // fc=1400: 8 kHz lives on the gated branch
        p.thresholdDb = -40.0;
        p.lookaheadSamples = lookahead;
        e.setTargets (p);
        auto x = makeInput();
        float g = 0.0f;
        for (int t = 0; t < n; ++t)
        {
            float* ch[1] = { x.data() + t };
            e.processBlock (ch, nullptr, 0, 1, 1);
            if (t == burstStart + lookahead)
                g = e.channel (0).lastGain();
        }
        return g;
    };

    CHECK (gainAtArrival (0) < 0.05);       // gate still closed when burst arrives
    CHECK (gainAtArrival (look) > 0.9);     // pre-opened by lookahead
}

TEST_CASE ("delta audition returns removed signal (aligned input minus output)")
{
    const double fs = 48000.0;
    const int n = 8192;

    DenoiserEngine e;
    e.prepare (fs, 1, 96);
    EngineParams p;
    p.strength01 = 1.0;
    p.thresholdDb = 0.0;     // gate never opens on this quiet key
    p.tightGate = true;      // hard-zero floor => delta = x - LP4(x) exactly
    p.lookaheadSamples = 0;
    p.deltaAudition = true;
    e.setTargets (p);

    auto x = testutil::sine (2000.0, fs, -20.0, n);

    // reference: y should be LP4(x); delta = x - LP4(x)
    CrossoverLR4 ref;
    ref.prepare (fs);
    ref.setCutoff (1400.0);

    auto in = x;
    float* ch[1] = { in.data() };
    e.processBlock (ch, nullptr, 0, n, 1);

    double maxErr = 0.0;
    for (int i = 0; i < n; ++i)
    {
        float low, high;
        ref.process (x[(size_t) i], low, high);   // run reference from t=0
        if (i < 4096)
            continue;                              // allow engine-side settling
        // (startup threshold ramp briefly opens the gate; its release tail
        //  needs ~4k samples to decay below the 2e-5 comparison floor)
        const float expected = x[(size_t) i] - low; // gate closed => y = low
        maxErr = std::max (maxErr, (double) std::fabs (expected - in[(size_t) i]));
    }
    CHECK (maxErr < 2.0e-5);
}
