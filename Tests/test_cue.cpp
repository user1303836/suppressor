// Transient cue behavior.
#include <doctest/doctest.h>
#include "TestUtil.h"
#include "../Source/DSP/TransientCue.h"

using namespace suppressor;

TEST_CASE ("cue opens on genuine onsets, closes after its window, refractory-limited")
{
    const double fs = 48000.0;
    TransientCue cue;
    cue.prepare (fs);
    cue.setAmount (1.0);

    // silence, then a step to a sustained level
    bool openedEarly = false;
    int openCountLate = 0;
    for (int i = 0; i < (int) (0.2 * fs); ++i)
    {
        const float x = (i < 4800) ? 0.0f : 0.3f;
        const bool active = cue.process (std::fabs (x));
        if (i >= 4800 && i < 4800 + 96) openedEarly = openedEarly || active;   // within 2 ms
        if (i > 4800 + (int) (0.05 * fs)) openCountLate += active ? 1 : 0;      // after 50 ms
    }
    CHECK (openedEarly);
    CHECK (openCountLate == 0); // sustained level must not hold the cue open

    // 1 ms pulses every 100 ms (pick-like transients): cue fires on each
    cue.reset();
    int fires = 0;
    bool wasActive = false;
    for (int i = 0; i < (int) (0.45 * fs); ++i)
    {
        const float x = ((i % 4800) < 48) ? 0.5f : 0.0f;
        const bool active = cue.process (x);
        if (active && ! wasActive) ++fires;
        wasActive = active;
    }
    CHECK (fires >= 4);
}

TEST_CASE ("cue disabled at amount 0")
{
    const double fs = 48000.0;
    TransientCue cue;
    cue.prepare (fs);
    cue.setAmount (0.0);
    for (int i = 0; i < 4800; ++i)
        CHECK_FALSE (cue.process ((i % 480 == 0) ? 0.5f : 0.0f));
}
