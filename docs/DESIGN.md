# suppressor — DSP design specification

This document describes the complete signal processing of **suppressor** in
self-contained terms. Everything here is implemented in `Source/DSP/` and
covered by unit tests in `Tests/`.

## 1. Core topology (one-split mode, the default)

Per channel, independently (dual mono):

```
                ┌─ LP2(fc) ─ LP2(fc) ───────────────────────┐
x[n] ──────────┤                                            ├─ Σ ─ y[n]
                └─ HP2(fc) ─ HP2(fc) ─ × branch gain g[n] ──┘
                                          ▲
                        detector: |high|  │   (or external key / pre-delay tap)
                        threshold ─ hold ─ attack/release smoother
```

- `LP2`/`HP2` are second-order Butterworth sections (Q = 1/√2, bilinear
  transform). Cascaded pairs form an LR4-style split: each branch is
  −6.02 dB at `fc`, and the two branches sum to a flat-magnitude all-pass.
- Closed (`g = 0`): output is the fourth-order low-pass — the guitar body
  always passes.
- Open (`g = 1`): output is `L4 + H4`, flat magnitude with an all-pass
  phase rotation. Dedicated bypass is truly dry.
- Transitioning: a smooth causal dynamic low-pass.

The detector taps **|high|** — the high branch itself — so low-frequency
energy (palm-muted fundamentals, hum) cannot reopen the noisy branch, while
pick attack and high harmonics open it in microseconds.

### 1.1 Parameter laws

| Control | Law |
|---|---|
| Strength `s ∈ [0,1]` | `fc = min(8000 − 6600·s, 0.45·fs) Hz` (8 kHz → 1.4 kHz, Nyquist guard) |
| Threshold `T dB` | open comparator at `10^(T/20)` on \|high\| |
| Release `R ms` | e-folding time constant of the gain one-pole; UI map `R = 2·15^r`, 2–30 ms |
| Attack | fixed **10 µs** e-folding (10–90 % rise ≈ 22 µs) |
| Hold | retriggerable, `⌊holdMs·fs/1000⌋` samples (default 2 ms) |
| Snap | while opening, `g > 0.999 → 1` (removes asymptotic tail) |

### 1.2 Gate state machine (hysteresis)

```
above  = det > openT
below  = det < openT − hysteresisDb

if above:            open = true; hold = holdSamples
else if !below && open: hold = holdSamples        // inside band: keep open
else if below:       hold > 0 ? --hold : open = false

target = open ? 1 : floor
g = a·g + (1−a)·target,  a = open ? attackA : releaseA
```

Separate open/close thresholds (level hysteresis, default 6 dB) prevent
chatter near threshold; the retriggerable hold bridges waveform
zero-crossings.

### 1.3 Reduction vs Tight Gate + Depth

- **Tight Gate**: floor = hard zero (modern rhythm cleanup).
- **Reduction** (default): floor = `10^(−depth/20)`, preserving pick/fret
  texture instead of blinking the high branch fully off.

## 2. Improvements over a plain one-split gate

All of the following are implemented and tested.

### 2.1 Sample-accurate, block-independent control (P0 engineering)

Parameters are smoothed per sample over fixed **time** ramps (10 ms) and
applied to the DSP at a fixed 16-sample control quantum aligned to an
absolute sample counter, reading the value at the quantum *start*.
Rendered output is therefore **bit-identical for any host block
partitioning**, for both static and automated parameters (unit-tested with
block sizes 1…1024 and a sample-accurate parameter event). Cutoff is
smoothed in the log-frequency domain and all four sections always derive
from the same coherent cutoff.

### 2.2 Hysteresis + adjustable hold (P0)

See §1.2. Hold is user-adjustable 0–50 ms (default 2 ms).

### 2.3 Transient cue (P1)

A broadband onset detector (1 ms vs 60 ms envelope difference) briefly
opens the high branch on genuine onsets, protecting pick/fret harmonics
before the HF detector fully opens. The cue window is 8 ms, rate-limited
by a 60 ms refractory timer, and requires the onset signal to fall below
half threshold before re-arming — so sustained energy (palm-mute ring,
hum) triggers exactly one cue and cannot hold hiss open. Amount control
scales sensitivity; 0 disables.

### 2.4 External clean-key detector (P1)

The detector can be driven from an external sidechain input (passed through
the same HP4 weighting) while processing the main path — for setups that
detect on an untouched DI and process a later, noisier point in the chain.
Default: internal.

### 2.5 Learn-and-lock hum removal (P1)

A separately switchable module **before** the split: on Learn, up to 2 s of
input is captured; the 50/60 Hz family is measured with Goertzel analysis
(±3 % frequency refinement, auto family detect), and a cascade of
minimum-phase resonant dips (Q = 64) is configured per present harmonic.
Dip depth targets a ≈ −80 dBFS residual and scales with Hum Strength. The
measurement is locked after learning; Strength/harmonic-count changes
re-derive coefficients without a new capture. Learns with < 0.5 s of
material abort safely. Delta audition outputs the removed signal for
verification.

### 2.6 Conservative multiband mode (P2, advanced)

Optional 4- or 6-band mode: an LR4 crossover tree, one envelope gate per
band, shared timing, and a Learn pass that sets per-band thresholds just
above the measured noise peaks. The one-split core remains the default.
Note: LR-tree band sums are not perfectly flat near crossover regions
(≤ ~±2 dB worst case with all bands open); this mode trades a little phase
linearity for tracking irregular noise spectra.

### 2.7 Program-dependent release (P2)

When enabled, the release time is selected between the Release knob (fast)
and 8× that value (slow) by the detector's short-term **crest factor**:
spiky/transient program (chugs) closes fast, smooth sustained program
closes slowly. The release coefficient is slewed over 200 ms so adaptation
is click-free. This is a deliberately independent mechanism — a single
crest-driven selector rather than any windowed fast/slow integrator
scheme.

### 2.8 Optional lookahead

0 (default), 1, or 2 ms. The audio path is delayed exactly N samples while
the detector observes the undelayed signal, pre-opening the gate for soft
onsets. Latency is reported to the host exactly; the default live mode is
zero samples.

### 2.9 Metering and delta audition

Detector level and branch-gain reduction are exposed for the UI, and Delta
Audition outputs `aligned_input − output` so the removed signal can be
inspected directly.

## 3. Implementation notes

- Four transposed-DF-II biquads per channel for the split (two states per
  section), coefficients computed in double precision, states always live.
- No allocations or locks in the audio path; FTZ/DAZ via
  `juce::ScopedNoDenormals`; recursive states never reset at gate edges.
- Detection filter for external key/lookahead is separate from the audio
  crossover so the audio path stays single-rate.
- The `high = allpass − low` algebraic shortcut was considered and rejected:
  exact in theory but it raises the float32 detector floor at extreme rates
  for negligible wall-clock savings, so the high branch is computed
  directly.

## 4. Validation summary

- Golden LP4/HP4 magnitude points across strength and 8–384 kHz sample
  rates; cutoff law and Nyquist cap.
- Gate timing: attack sequence (0.8755 / 0.9845 / 0.9981 / snap on 4th
  sample @48 kHz), `⌊0.002·fs⌋` hold at 8–384 kHz, 2/6/30 ms release fits.
- Detector weighting: frequency-dependent key thresholds follow HP4 gain;
  low keys never reopen the branch.
- Zero-latency impulse alignment; exact N-sample lookahead; dual-mono
  channel independence.
- Bit-exact block-partition invariance, static and automated.
- Hum: 60 Hz family learned, ≥ 40 dB fundamental rejection, program
  content within ±0.1 dB; learn aborts safely on short material.
- Multiband: learned thresholds, ≥ 20 dB closed-noise reduction,
  reconstruction within documented ripple.
- pluginval (strictness 10) and clap-validator pass on all three OSes in CI.
