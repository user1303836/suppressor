#pragma once

// Per-channel processing engine.
//
// Order: hum suppressor -> lookahead delay (audio) -> denoiser core
// (one-split or multiband). The detector path is tapped BEFORE the delay,
// so lookahead lets the gate pre-open by the chosen number of samples.
// The transient cue observes the broadband hum-suppressed signal.

#include "OneSplitDenoiser.h"
#include "MultibandDenoiser.h"
#include "HumSuppressor.h"
#include "TransientCue.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace suppressor
{
class ChannelEngine
{
public:
    enum Mode { oneSplit = 0, fourBand = 1, sixBand = 2 };

    void prepare (double fs, int maxLookaheadSamples)
    {
        sampleRate = fs;
        maxLookahead = std::max (1, maxLookaheadSamples);
        delayLine.assign ((size_t) maxLookahead + 1, 0.0f);
        hum.prepare (fs);
        oneSplitCore.prepare (fs);
        multiCore.prepare (fs);
        cue.prepare (fs);
        reset();
    }

    void setMode (Mode m) noexcept
    {
        mode = m;
        multiCore.setBandCount (m == sixBand ? 6 : 4);
    }

    void setLookaheadSamples (int n) noexcept
    {
        lookahead = std::clamp (n, 0, maxLookahead);
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);
        delayPos = 0;
        updateDetectorRouting();
    }

    int lookaheadSamples() const noexcept { return lookahead; }

    void setExternalKey (bool ext) noexcept
    {
        externalKey = ext;
        multiCore.setExternalKey (ext);
        updateDetectorRouting();
    }

    void setCutoff (double fcHz) noexcept { oneSplitCore.setCutoff (fcHz); }
    void setGateParams (const GateParams& p) noexcept
    {
        oneSplitCore.setGateParams (p);
        multiCore.setGateTiming (p);
    }
    void setCueAmount (double a) noexcept { cue.setAmount (a); }

    void reset() noexcept
    {
        hum.reset();
        oneSplitCore.reset();
        multiCore.reset();
        cue.reset();
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);
        delayPos = 0;
        lastIn = lastOut = 0.0f;
    }

    // key: external sidechain sample for this channel (ignored when the
    // detector source is internal). Returns the processed sample.
    inline float process (float x, float key) noexcept
    {
        lastIn = x;
        const float h = hum.process (x);

        // audio through lookahead delay
        float audio = h;
        if (lookahead > 0)
        {
            delayLine[(size_t) delayPos] = h;
            const int readPos = (delayPos - lookahead + (int) delayLine.size()) % (int) delayLine.size();
            audio = delayLine[(size_t) readPos];
            delayPos = (delayPos + 1) % (int) delayLine.size();
        }

        // detector source: pre-delay signal (internal) or external key
        const float detSrc = externalKey ? key : h;
        const bool cueOpen = cue.process (std::fabs (h));

        float y;
        if (mode == oneSplit)
            y = oneSplitCore.process (audio, detSrc, cueOpen);
        else
            y = multiCore.process (audio, detSrc, cueOpen);

        lastOut = y;
        return y;
    }

    float lastInput() const noexcept  { return lastIn; }
    float lastOutput() const noexcept { return lastOut; }

    float lastGain() const noexcept
    {
        return (mode == oneSplit) ? oneSplitCore.lastGain() : multiCore.minGain();
    }
    float lastDetector() const noexcept { return oneSplitCore.lastDetector(); }

    HumSuppressor hum;
    MultibandDenoiser multiCore;   // public for learn triggers / band info

private:
    void updateDetectorRouting() noexcept
    {
        oneSplitCore.setSeparateDetectorPath (externalKey || lookahead > 0);
    }

    double sampleRate = 48000.0;
    Mode mode = oneSplit;
    bool externalKey = false;

    TransientCue cue;
    OneSplitDenoiser oneSplitCore;

    std::vector<float> delayLine;
    int delayPos = 0, lookahead = 0, maxLookahead = 96;
    float lastIn = 0.0f, lastOut = 0.0f;
};

} // namespace suppressor
