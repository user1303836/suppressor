#pragma once

// Top-level multi-channel engine.
//
// Owns per-channel ChannelEngines, time-based parameter smoothers, and a
// fixed 16-sample control quantum aligned to an absolute sample counter --
// so rendered output is identical for any host block partitioning, for both
// static and automated parameters. Also owns the output gain, the removed-
// signal (delta) audition path, and block-level metering.

#include "ChannelEngine.h"
#include "ParameterSmoothing.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace suppressor
{
struct EngineParams
{
    double strength01      = 0.9;
    double thresholdDb     = -40.0;
    double hysteresisDb    = 6.0;
    double holdMs          = 2.0;
    double releaseMs       = 6.0;
    double depthDb         = 40.0;   // positive depth; Reduction floor = -depth
    bool   tightGate       = false;
    bool   adaptiveRelease = false;
    double cueAmount       = 0.0;
    bool   externalKey     = false;
    int    lookaheadSamples = 0;
    int    bandMode        = 0;      // 0 = one-split, 1 = 4-band, 2 = 6-band
    double outputGainDb    = 0.0;
    bool   deltaAudition   = false;
};

class DenoiserEngine
{
public:
    static constexpr int kControlQuantum = 16;

    static double strengthToCutoff (double s01, double fs) noexcept
    {
        return std::min (8000.0 - 6600.0 * s01, 0.45 * fs);
    }

    void prepare (double fs, int numChannels, int maxLookaheadSamples)
    {
        sampleRate = fs;
        channels.assign ((size_t) std::max (1, numChannels), {});
        for (auto& ch : channels)
            ch.prepare (fs, maxLookaheadSamples);

        deltaDelay.assign (channels.size(), {});
        for (auto& d : deltaDelay)
            d.assign ((size_t) maxLookaheadSamples + 1, 0.0f);
        deltaPos = 0;
        maxLookahead = maxLookaheadSamples;

        fcSmoother.prepare (fs, 10.0);
        thrSmoother.prepare (fs, 10.0);
        hysSmoother.prepare (fs, 10.0);
        holdSmoother.prepare (fs, 10.0);
        relSmoother.prepare (fs, 10.0);
        depSmoother.prepare (fs, 10.0);
        gainSmoother.prepare (fs, 10.0);

        targets = EngineParams{};
        fcSmoother.snapTo (std::log2 (strengthToCutoff (targets.strength01, fs)));
        thrSmoother.snapTo (targets.thresholdDb);
        hysSmoother.snapTo (targets.hysteresisDb);
        holdSmoother.snapTo (targets.holdMs);
        relSmoother.snapTo (targets.releaseMs);
        depSmoother.snapTo (targets.depthDb);
        gainSmoother.snapTo (1.0);
        applyStructural (targets);
        pushSmoothedToEngines();
        reset();
    }

    int numChannels() const noexcept { return (int) channels.size(); }
    int latencySamples() const noexcept { return targets.lookaheadSamples; }

    void setTargets (const EngineParams& p) noexcept
    {
        targets = p;
        fcSmoother.setTarget (std::log2 (strengthToCutoff (p.strength01, sampleRate)));
        thrSmoother.setTarget (p.thresholdDb);
        hysSmoother.setTarget (p.hysteresisDb);
        holdSmoother.setTarget (p.holdMs);
        relSmoother.setTarget (p.releaseMs);
        depSmoother.setTarget (p.depthDb);
        gainSmoother.setTarget (std::pow (10.0, p.outputGainDb / 20.0));
        applyStructural (p);
    }

    void reset() noexcept
    {
        for (auto& ch : channels) ch.reset();
        for (auto& d : deltaDelay) std::fill (d.begin(), d.end(), 0.0f);
        deltaPos = 0;
        quantumCounter = 0;
        blockMaxDet = 0.0f;
        blockMinGain = 1.0f;
    }

    HumSuppressor& hum (int ch) noexcept        { return channels[(size_t) ch].hum; }
    MultibandDenoiser& multi (int ch) noexcept  { return channels[(size_t) ch].multiCore; }
    const ChannelEngine& channel (int ch) const noexcept { return channels[(size_t) ch]; }

    // Sample-accurate parameter event: applied when the engine's absolute
    // sample counter reaches `offsetSamples` inside processBlock, so the
    // control trajectory is identical for any host block partitioning.
    struct ParamEvent
    {
        int offsetSamples = 0;
        EngineParams params;
    };

    // io: main audio (in-place). sc: optional sidechain key channels
    // (nullptr or fewer channels = silence key). numChannels <= prepared.
    void processBlock (float* const* io, const float* const* sc, int numScChannels,
                       int numSamples, int numCh,
                       const std::vector<ParamEvent>* events = nullptr) noexcept
    {
        blockMaxDet = 0.0f;
        blockMinGain = 1.0f;
        numCh = std::min (numCh, (int) channels.size());
        size_t nextEvent = 0;

        for (int n = 0; n < numSamples; ++n)
        {
            if (events != nullptr)
                while (nextEvent < events->size() && (*events)[nextEvent].offsetSamples == n)
                    setTargets ((*events)[nextEvent++].params);

            if ((quantumCounter % kControlQuantum) == 0)
                pushSmoothedToEngines();
            ++quantumCounter;

            // alignment delay for the delta path mirrors the audio lookahead
            float alignedKey0 = 0.0f;
            (void) alignedKey0;

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = io[ch][n];
                const float key = (sc != nullptr && ch < numScChannels) ? sc[ch][n] : 0.0f;

                // delta alignment copy (same delay as the audio lookahead)
                auto& dd = deltaDelay[(size_t) ch];
                dd[(size_t) deltaPos] = x;
                const int look = targets.lookaheadSamples;
                const float aligned = (look > 0)
                    ? dd[(size_t) ((deltaPos - look + (int) dd.size()) % (int) dd.size())]
                    : x;

                const float y = channels[(size_t) ch].process (x, key);

                const float g = (float) gainSmootherVal;
                float out = targets.deltaAudition ? (aligned - y) : y;
                io[ch][n] = out * g;

                blockMaxDet = std::max (blockMaxDet, channels[(size_t) ch].lastDetector());
                blockMinGain = std::min (blockMinGain, channels[(size_t) ch].lastGain());
            }

            deltaPos = (deltaPos + 1) % (int) deltaDelay[0].size();
            advanceSmoothersOneSample();
        }
    }

    float detectorLevelDb() const noexcept
    {
        return 20.0f * std::log10 (blockMaxDet + 1.0e-9f);
    }
    float gainReductionDb() const noexcept
    {
        return 20.0f * std::log10 (blockMinGain + 1.0e-7f);
    }

private:
    void applyStructural (const EngineParams& p) noexcept
    {
        for (auto& ch : channels)
        {
            ch.setMode ((ChannelEngine::Mode) std::clamp (p.bandMode, 0, 2));
            ch.setExternalKey (p.externalKey);
            ch.setLookaheadSamples (p.lookaheadSamples);
            ch.setCueAmount (p.cueAmount);
        }
    }

    inline void pushSmoothedToEngines() noexcept
    {
        GateParams gp;
        gp.openThresholdDb = thrSmoother.current();
        gp.hysteresisDb    = hysSmoother.current();
        gp.holdMs          = holdSmoother.current();
        gp.releaseMs       = relSmoother.current();
        gp.floorDb         = -std::fabs (depSmoother.current());
        gp.tightGate       = targets.tightGate;
        gp.adaptiveRelease = targets.adaptiveRelease;
        gp.adaptiveMaxMs   = std::max (gp.releaseMs * 8.0, gp.releaseMs + 1.0);
        gp.attackMs        = 0.01; // fixed 10 us e-folding

        const double fc = std::pow (2.0, fcSmoother.current());
        for (auto& ch : channels)
        {
            ch.setCutoff (fc);
            ch.setGateParams (gp);
        }
        gainSmootherVal = gainSmoother.current();
    }

    inline void advanceSmoothersOneSample() noexcept
    {
        fcSmoother.next();
        thrSmoother.next();
        hysSmoother.next();
        holdSmoother.next();
        relSmoother.next();
        depSmoother.next();
        gainSmoother.next();
    }

    double sampleRate = 48000.0;
    std::vector<ChannelEngine> channels;
    std::vector<std::vector<float>> deltaDelay;
    int deltaPos = 0, maxLookahead = 96;

    EngineParams targets;
    LinearSmoother fcSmoother, thrSmoother, hysSmoother, holdSmoother, relSmoother, depSmoother, gainSmoother;
    double gainSmootherVal = 1.0;
    long quantumCounter = 0;
    float blockMaxDet = 0.0f, blockMinGain = 1.0f;
};

} // namespace suppressor
