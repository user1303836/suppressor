// Coefficient- and frequency-domain golden tests for the biquad sections
// and the LR4 crossover pair.
#include <doctest/doctest.h>
#include "TestUtil.h"
#include "../Source/DSP/DenoiserEngine.h"

using namespace suppressor;

TEST_CASE ("LP4 closed-response golden points (fs=48k, fc=1400)")
{
    const auto lp = butterworthLp2 (1400.0, 48000.0);
    const std::vector<BiquadCoeffs> l4 { lp, lp };

    CHECK (testutil::cascadeMagDb (l4, 1000.0, 48000.0) == doctest::Approx (-2.000).epsilon (0.002));
    CHECK (testutil::cascadeMagDb (l4, 1400.0, 48000.0) == doctest::Approx (-6.021).epsilon (0.002));
    CHECK (testutil::cascadeMagDb (l4, 2000.0, 48000.0) == doctest::Approx (-14.343).epsilon (0.002));
    CHECK (testutil::cascadeMagDb (l4, 4000.0, 48000.0) == doctest::Approx (-37.303).epsilon (0.002));
    CHECK (testutil::cascadeMagDb (l4, 8000.0, 48000.0) == doctest::Approx (-63.860).epsilon (0.002));
}

TEST_CASE ("LP4 closed-response at fc=2060 (fs=48k)")
{
    const auto lp = butterworthLp2 (2060.0, 48000.0);
    const std::vector<BiquadCoeffs> l4 { lp, lp };

    CHECK (testutil::cascadeMagDb (l4, 1000.0, 48000.0) == doctest::Approx (-0.46).epsilon (0.01));
    CHECK (testutil::cascadeMagDb (l4, 2000.0, 48000.0) == doctest::Approx (-5.52).epsilon (0.01));
    CHECK (testutil::cascadeMagDb (l4, 4000.0, 48000.0) == doctest::Approx (-24.20).epsilon (0.01));
    CHECK (testutil::cascadeMagDb (l4, 8000.0, 48000.0) == doctest::Approx (-50.35).epsilon (0.01));
}

TEST_CASE ("HP4 detector weighting golden points (fs=48k, fc=1400)")
{
    const auto hp = butterworthHp2 (1400.0, 48000.0);
    const std::vector<BiquadCoeffs> h4 { hp, hp };

    CHECK (testutil::cascadeMagDb (h4, 1000.0, 48000.0) == doctest::Approx (-13.738).epsilon (0.002));
    CHECK (testutil::cascadeMagDb (h4, 1400.0, 48000.0) == doctest::Approx (-6.021).epsilon (0.002));
    CHECK (testutil::cascadeMagDb (h4, 2000.0, 48000.0) == doctest::Approx (-1.850).epsilon (0.002));
    CHECK (testutil::cascadeMagDb (h4, 4000.0, 48000.0) == doctest::Approx (-0.119).epsilon (0.002));
}

TEST_CASE ("LR4 pair sums to flat magnitude (all-pass) across frequency")
{
    const auto lp = butterworthLp2 (1400.0, 48000.0);
    const auto hp = butterworthHp2 (1400.0, 48000.0);
    for (double f : { 20.0, 100.0, 500.0, 1000.0, 1400.0, 2000.0, 4000.0, 8000.0, 16000.0 })
    {
        // L4 + H4 complex sum
        double w = 2.0 * kPi * f / 48000.0;
        double ml1, pl1, mh1, ph1;
        biquadResponse (lp, w, ml1, pl1);
        biquadResponse (hp, w, mh1, ph1);
        // cascade: square magnitudes, double phases
        const double re = ml1 * ml1 * std::cos (2 * pl1) + mh1 * mh1 * std::cos (2 * ph1);
        const double im = ml1 * ml1 * std::sin (2 * pl1) + mh1 * mh1 * std::sin (2 * ph1);
        const double sumDb = 20.0 * std::log10 (std::sqrt (re * re + im * im));
        CHECK (sumDb == doctest::Approx (0.0).epsilon (1.0e-9));
    }
}

TEST_CASE ("cutoff law: fc = min(8000 - 6600*s, 0.45*fs)")
{
    CHECK (DenoiserEngine::strengthToCutoff (0.0, 48000.0) == doctest::Approx (8000.0));
    CHECK (DenoiserEngine::strengthToCutoff (0.5, 48000.0) == doctest::Approx (4700.0));
    CHECK (DenoiserEngine::strengthToCutoff (0.9, 48000.0) == doctest::Approx (2060.0));
    CHECK (DenoiserEngine::strengthToCutoff (1.0, 48000.0) == doctest::Approx (1400.0));
    // Nyquist guard
    CHECK (DenoiserEngine::strengthToCutoff (0.0, 8000.0) == doctest::Approx (3600.0));
    CHECK (DenoiserEngine::strengthToCutoff (0.0, 11025.0) == doctest::Approx (4961.25));
    CHECK (DenoiserEngine::strengthToCutoff (0.0, 16000.0) == doctest::Approx (7200.0));
}

TEST_CASE ("coefficients stay finite at extreme rates")
{
    for (double fs : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0 })
    {
        for (double fc : { 100.0, 1400.0, 0.45 * fs * 0.999 })
        {
            const auto lp = butterworthLp2 (fc, fs);
            const auto hp = butterworthHp2 (fc, fs);
            CHECK (std::isfinite (lp.b0 + lp.b1 + lp.b2 + lp.a1 + lp.a2));
            CHECK (std::isfinite (hp.b0 + hp.b1 + hp.b2 + hp.a1 + hp.a2));
        }
    }
}
