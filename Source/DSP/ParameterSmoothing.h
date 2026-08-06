#pragma once

// Time-based, sample-accurate parameter smoothing.
//
// The ramp duration is specified in milliseconds and advanced once per
// sample, so the produced trajectory depends only on absolute sample
// position -- never on the host's block partitioning. This is a deliberate
// design choice: control trajectories must be identical for any block size.

#include <cmath>

namespace suppressor
{
class LinearSmoother
{
public:
    void prepare (double fs, double rampMs) noexcept
    {
        sampleRate = fs;
        rampSamples = std::max (1.0, std::floor (0.001 * rampMs * fs));
    }

    void setTarget (double v) noexcept
    {
        if (v == target_)
            return;
        target_ = v;
        remaining_ = (long) rampSamples;
        step_ = (remaining_ > 0) ? (target_ - current_) / (double) remaining_ : 0.0;
    }

    void snapTo (double v) noexcept
    {
        current_ = target_ = v;
        step_ = 0.0;
        remaining_ = 0;
    }

    inline double next() noexcept
    {
        if (remaining_ > 0)
        {
            current_ += step_;
            --remaining_;
            if (remaining_ == 0)
                current_ = target_;
        }
        return current_;
    }

    double current() const noexcept { return current_; }
    bool isActive() const noexcept { return remaining_ > 0; }

private:
    double sampleRate = 48000.0;
    long rampSamples = 480;
    double current_ = 0.0, target_ = 0.0, step_ = 0.0;
    long remaining_ = 0;
};

} // namespace suppressor
