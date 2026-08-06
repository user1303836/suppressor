#pragma once

// Noise-qualified transient cue.
//
// A short-window onset detector on the broadband signal. When a genuine
// onset is found, the cue briefly opens the noise-suppression branch so
// legitimate pick/fret harmonics pass before the high-frequency detector
// has fully opened. The cue window is deliberately short and rate-limited
// by a refractory timer, so sustained low-frequency energy (palm-muted
// fundamental ringing, hum) cannot hold the noisy branch open.

#include <cmath>
#include <algorithm>

namespace suppressor
{
class TransientCue
{
public:
    void prepare (double fs) noexcept
    {
        sampleRate = fs;
        fastA = std::exp (-1.0 / (0.001 * fs));   // 1 ms fast envelope
        slowA = std::exp (-1.0 / (0.060 * fs));   // 60 ms slow envelope
        windowSamples   = (int) std::floor (0.008 * fs);  // 8 ms cue window
        refractorySamples = (int) std::floor (0.060 * fs); // 60 ms refractory
        reset();
    }

    // amount in [0,1]; 0 disables the cue entirely
    void setAmount (double amount01) noexcept { amount = std::clamp (amount01, 0.0, 1.0); }
    double getAmount() const noexcept { return amount; }

    void reset() noexcept
    {
        fast = slow = 0.0f;
        windowCounter = 0;
        refractoryCounter = 0;
        armed = true;
    }

    // Returns true while the cue window is open.
    inline bool process (float absBroadband) noexcept
    {
        fast = (float) (fastA * fast + (1.0 - fastA) * absBroadband);
        slow = (float) (slowA * slow + (1.0 - slowA) * absBroadband);

        const float onset = fast - slow;

        if (refractoryCounter > 0)
            --refractoryCounter;

        if (amount > 0.0 && refractoryCounter == 0)
        {
            // higher amount -> more sensitive. The detector must re-arm
            // (onset drops below half threshold) before it can fire again,
            // so sustained levels trigger exactly one cue.
            const double threshold = 0.05 * (1.0 + 3.0 * (1.0 - amount));
            if (armed && onset > (float) threshold)
            {
                windowCounter = windowSamples;
                refractoryCounter = refractorySamples;
                armed = false;
            }
            else if (onset < (float) (0.5 * threshold))
            {
                armed = true;
            }
        }

        const bool active = windowCounter > 0;
        if (windowCounter > 0)
            --windowCounter;

        return active;
    }

private:
    double sampleRate = 48000.0;
    double amount = 0.0;
    double fastA = 0.0, slowA = 0.0;
    float fast = 0.0f, slow = 0.0f;
    int windowSamples = 0, windowCounter = 0;
    int refractorySamples = 0, refractoryCounter = 0;
    bool armed = true;
};

} // namespace suppressor
