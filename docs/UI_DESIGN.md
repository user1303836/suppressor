# Suppressor UI design brief

This brief applies the HDN Plugin UI Design skill v0.1.1 to Suppressor 0.1.0. It documents the editor contract and does not modify the central design system.

## Product

- **Purpose:** Zero-latency-by-default, frequency-split noise suppression for guitar DI before high-gain amplification.
- **Primary musician task:** Remove high-band noise between notes without sacrificing body, sustain, or pick attack, then verify the removed signal when needed.
- **Character:** Surgical, quiet, fast, spectral, trustworthy.
- **Users:** Guitarists and engineers tracking or reamping mono/stereo DI, optionally with an external detector key.
- **Three-second read:** Product identity; active topology; suppression activity; Strength, Threshold, and Release; Delta Audition state.
- **Branding:** The existing Suppressor Audio identity is retained. The HDN grammar is used without adding an unapproved HDN product mark.

## Delivery environment

- **Framework:** JUCE 8.0.9.
- **CLAP wrapper:** clap-juce-extensions `c1a5ad025f95d01e03267857fa8276ebeed16500`.
- **Release formats:** VST3 and CLAP.
- **Design/test harness:** Standalone; it is not packaged by the current release workflow.
- **Platforms:** macOS, Windows, and Linux.
- **Editor policy:** Fixed at 1000 x 640 logical pixels.
- **Scale factors:** 100%, 125%, 150%, 175%, and 200% where the host/platform supports them.
- **Input:** Mouse, trackpad, keyboard, and assistive technology.
- **Typography:** Bundled Barlow Condensed 1.422 Regular and SemiBold under the SIL Open Font License 1.1.

## Immutable functional contract

The UI work must preserve:

- every file under `Source/DSP/` and all audio-thread behavior;
- `PluginProcessor.*`, the APVTS state type `Parameters`, state serialization, and session compatibility;
- manufacturer/plugin codes `SpAu`/`Supr` and CLAP ID `audio.suppressor.suppressor`;
- all 20 parameter IDs, version hints, order, names, types, ranges, defaults, tapers, steps, units, text conversion, and automation behavior;
- buses, sidechain behavior, MIDI declarations, latency reporting, and wrapper identity;
- CLAP event resolution and wrapper revision.

## Parameter inventory

| ID | Purpose | Range / default / text | Dependency |
|---|---|---|---|
| `strength` | One-split crossover strength | 0–100%, 90%, linear | Inactive but editable in 4/6-Band |
| `threshold` | One-split detector threshold | -80–0 dB, -40 dB, 0.1 dB | Inactive but editable in 4/6-Band |
| `release` | Gate release | 2–30 ms, 6 ms, exponential | Active in every topology |
| `gateMode` | Reduction or Tight Gate | Reduction / Tight Gate, Reduction | Depth is ignored by Tight Gate |
| `depth` | Reduction-mode floor | 0–60 dB, 40 dB, 0.1 dB | Inactive but editable in Tight Gate |
| `hysteresis` | Open/close threshold separation | 0–24 dB, 6 dB, 0.1 dB | Active |
| `hold` | Retriggerable hold | 0–50 ms, 2 ms, 0.1 ms | Active |
| `adaptiveRelease` | Crest-dependent release | Off / on, off | Active |
| `cue` | Broadband transient cue | 0–100%, 0%, integer text | Active |
| `sidechain` | Detector source | Internal / External Key, Internal | External selection does not prove key presence |
| `lookahead` | Detector lookahead | 0 / 1 / 2 ms, 0 ms | Changes reported latency |
| `humEnable` | Enable learned hum dips | Off / on, off | Lock is also required for audible removal |
| `humBase` | Auto/50/60 Hz family | Auto / 50 Hz / 60 Hz, Auto | A change invalidates an existing lock |
| `humHarmonics` | Maximum hum harmonics | 1–16, 8, integer | Inactive but editable while Hum Removal is off |
| `humStrength` | Learned dip strength | 0–100%, 100% | Inactive but editable while Hum Removal is off |
| `humLearn` | Start/stop hum capture | Boolean, off | Works while Hum Removal is off; hum stage passes during capture |
| `bandMode` | Suppression topology | One-Split / 4-Band / 6-Band, One-Split | Determines core applicability |
| `bandsLearn` | Start/stop band threshold capture | Boolean, off | Editable/pre-armable in One-Split; capture is effective only while processing in 4/6-Band, where the suppression stage passes |
| `deltaAudition` | Output removed signal | Off / on, off | Critical output state |
| `outputGain` | Final gain | -12–12 dB, 0 dB, 0.1 dB | Active |

All parameters retain version hint 1 and their current automatable state.

## Mode and state inventory

| State | Presentation |
|---|---|
| Reduction | Depth uses the periwinkle active arc. |
| Tight Gate | Depth remains operable in place with a neutral arc and an accessible “inactive but editable” explanation. |
| One-Split | Strength, global Threshold, and detector/threshold activity are active. |
| 4/6-Band | Strength and global Threshold remain in place with neutral arcs. The stale one-split detector value is replaced by “Global level not exposed”; gain reduction remains visible. |
| Hum off | Harmonics and Hum Strength remain editable with neutral arcs. |
| Hum learning armed | Learn becomes “Stop Learning”; status says the request is armed and capture/pass-through occurs only while processing. No lock-success claim is made. |
| Multiband learning armed | Learn becomes “Stop Learning”; Activity marks gain reduction unavailable and says suppression passes while processing. No lock-success claim is made. |
| One-Split + Bands Learn | The parameter remains editable and can be armed for automation/preconfiguration; status says it is waiting for 4/6-Band. |
| Delta Audition | Activity status explicitly reads “OUTPUT: REMOVED SIGNAL” in addition to the selected Delta Audition control. |
| External Key | The selected source is shown without claiming that a sidechain is connected. |

The processor does not expose bypass, key-presence, lock validity, learn completion, or a processing heartbeat through a safe immutable UI snapshot. The editor therefore makes no claims about those states. It labels activity as a last-processing snapshot, treats enabled Learn parameters as armed requests, and does not read the existing non-atomic engine status accessors.

## Telemetry contract

| Datum | Source | Decision | Validity and presentation |
|---|---|---|---|
| Detector level | `detectorDbAtomic` | Place the One-Split threshold relative to incoming high-band energy | Numeric and bar in One-Split; not shown as current in 4/6-Band because the source is stale there; <= -120 dB is “No Signal” |
| Gain reduction | `grDbAtomic` | See how much high-band attenuation is being applied | Positive magnitude, 0–60 dB display range; `60+ dB` avoids false precision in Tight Gate |
| Threshold marker | Existing `threshold` parameter | Compare detector level with the configured threshold | White marker on the detector bar in One-Split only |
| Learn armed | APVTS-backed Learn button state | Know that capture is requested and stop it | Text plus latched shape; capture/pass-through is explicitly conditional on processing; no invented progress or lock-success claim |

The display repaints only when rounded values or semantic state changes and announces semantic transitions rather than meter samples.

## Information architecture

The dominant path is header → Suppression Core → Activity → Detection/Hum/Multiband and Output.

- **Suppression Core:** A wide primary panel contains Gate Mode, Adaptive Release, and the six controls that determine the core suppression behavior. This prioritizes the main musical decision rather than mirroring implementation classes.
- **Activity:** The first accessibility group and a dedicated top-level panel present the last processing snapshot, with stale one-split data and learning pass-through handled explicitly rather than through a decorative analyzer.
- **Detection:** Cue, source, and lookahead stay together.
- **Hum Filter:** Enable, mains family, harmonics, strength, Learn, and capture state form one stable zone.
- **Multiband / Output:** Topology and learning share a compact upper zone; final gain and the critical Delta state sit below a divider.

No mode moves a control. Spacing and surface hierarchy replace the previous six equal GroupComponent boxes.

## Family system and tokens

The implementation retains the family’s flat surfaces, bundled typography, centered rotary values, 4 px rhythm, 18 px outer margin, 12 px gaps, semantic focus, and exact-entry behavior. It intentionally replaces the Ring Modulator lime with a product-specific **spectral periwinkle** system:

| Role | Token | Contrast evidence |
|---|---:|---|
| Canvas | `#0A0E14` | — |
| Header | `#0D141E` | — |
| Panel | `#111A24` | — |
| Raised control | `#182432` | — |
| Decorative divider | `#2A3A4B` | Not an interactive boundary |
| Essential control outline | `#66798C` | 3.5:1 on raised controls |
| Primary text | `#F1F5F8` | 16.0:1 on panels |
| Secondary text | `#A6B2BE` | 8.1:1 on panels |
| Active/focus accent | `#8EA7FF` | 7.6:1 on panels |
| Inactive-editable track | `#6C7C8F` | 4.1:1 on panels |
| Warning/acquiring | `#F2B84B` | 9.8:1 on panels |
| Error | `#FF6F7D` | 6.5:1 on panels |

Color is paired with text, fill extent, marker geometry, a checkmark, or a persistent 2 px focus outline.

## Interaction matrix

| Control | Pointer | Fine / keyboard | Exact entry | Reset / wheel | Focus |
|---|---|---|---|---|---|
| Rotary | Full dial, centered value, arc, and interior drag; up/right increases | Shift-drag uses velocity fine mode; arrows retain JUCE parameter steps | Double-click anywhere or Return; Return commits, Escape cancels; invalid text preserves the current value | Alt/Option-click restores the APVTS default; wheel disabled | Explicit tab order and 2 px outer ring |
| Selector | Full 34 px control opens | Arrows navigate; Return selects; Escape dismisses | n/a | Host parameter mapping retained | 2 px outline plus arrow color/shape |
| Toggle | Full 34–38 px tile | Space toggles | n/a | Host-notifying ButtonAttachment | 2 px outline plus checkmark |
| Learn action | Full 36 px action | Space/Return operates | n/a | APVTS ButtonAttachment supplies host gesture/state sync | Unique Learn/Stop Learning name and latched fill/text; armed is not mislabeled as capturing |

`getControlParameterIndex` maps each control and centered value child to its existing processor parameter for host services.

## Accessibility

- Every interactive component has a unique name, title, formatted parameter value/unit, description, and explicit focus order.
- Decorative labels are hidden from the accessibility tree to prevent duplicate announcements.
- All interactive targets are at least 34 px high; rotary targets are substantially larger.
- Inactive-but-editable controls remain enabled and explain why they currently have no audible effect.
- Critical states use text plus geometry; no state relies on color alone.
- The Activity display comes before controls in accessibility browse order and exposes a read-only last-snapshot value. Meter samples are queryable, but only semantic transitions announce.
- `EDITOR_WANTS_KEYBOARD_FOCUS` is enabled; embedded VST3 and CLAP focus and text-entry checks pass.

## Host-format validation

| Format | Artifact / validator | Manual host evidence | Result |
|---|---|---|---|
| VST3 | Release `suppressor.vst3`; pluginval 1.0.4 strictness 10 | JUCE 8.0.9 AudioPluginHost: embedded exact commit/cancel/invalid preservation, Tab and arrow paths, Alt/Option reset, normal and Shift-fine drag, host graph state save/recall, close/reopen, fixed-size rejection, and graceful null-context fallback | Pass |
| CLAP | Release `suppressor.clap`; clap-validator 0.4.1 | Pinned-header Cocoa integration host: 1000 x 640 logical size, non-resizable contract, 24 named accessibility elements, Tab/Space, exact entry, reset, normal/fine drag, balanced begin/value/end events, host-driven `params.flush` UI update, and repeated creation | Pass with wrapper debt noted below |
| Standalone | Release `suppressor.app` | Design/accessibility harness: complete state captures, AX inspection, pointer/keyboard paths, close-during-drag coverage, and fixed geometry | Pass; not a shipped format |

VST3 and CLAP are treated as separate wrappers. Validator results supplement rather than replace the embedded-editor checks. Cocoa CLAP sizes are logical and the pinned CLAP contract does not permit `set_scale` for that native API.

## Verification evidence

### Captured states

- [Default CLAP editor](images/suppressor-ui.png)
- [Multiband learning armed](images/suppressor-learning.png)
- [Persistent keyboard focus during learning](images/suppressor-learning-focus.png)
- [Delta Audition with `OUTPUT: REMOVED SIGNAL`](images/suppressor-delta-audition.png)
- [Centered exact-value entry](images/suppressor-exact-entry.png)
- [VST3 editor embedded in JUCE AudioPluginHost](images/suppressor-vst3-host.png)
- Scale renders: [100%](images/suppressor-scale-100.png), [125%](images/suppressor-scale-125.png), [150%](images/suppressor-scale-150.png), [175%](images/suppressor-scale-175.png), and [200%](images/suppressor-scale-200.png)

### Automated and manual checks

- Nine production-linked UI unit/integration cases pass with 363 assertions. They cover the bundled fonts, contrast, centered value hit testing, strict exact parsing, default reset, all 20 APVTS controls, accessible names and parameter mapping, truthful topology/learning/Delta states, keyboard gestures including Space, close-during-drag teardown, fixed logical geometry, and complete rendering at every declared scale.
- The existing DSP suite passes unchanged.
- Release VST3, CLAP, and Standalone targets build successfully.
- pluginval 1.0.4 at strictness 10 reports `SUCCESS`.
- clap-validator 0.4.1 reports 32 passed, 0 failed, one warning, and 11 skipped.
- The generated VST3 definition contains `JucePlugin_EditorRequiresKeyboardFocus=1`.
- In both hosted formats, Strength exact entry commits `20%`; Escape and malformed suffix text preserve the prior value; Right changes `20%` to `21%`; Alt/Option resets to `90%`; a normal 10 px drag changes `50%` to `54.5%`; and a Shift 50 px drag changes it to `50.2%`.
- CLAP output records exactly balanced begin/value/end events for Space, exact entry, arrows, reset, and drag. A host-originated `params.flush` change to `42%` updates the visible control without a feedback loop.
- VST3 host graph save/load restores Strength from `72%` to the saved `33%`, and editor close/reopen preserves `20%`.
- The fixed editor rejects host-window resize attempts in VST3 and CLAP. The scale test keeps 1000 x 640 logical bounds while rendering complete transformed bounds at 100%, 125%, 150%, 175%, and 200%.
- CI runs the UI render suite and uploads all declared-scale PNGs on macOS, Windows, and Linux; Linux uses Xvfb. CI also validates both shipped formats on all three platforms.

## HDN review scorecard

| Dimension | Score | Evidence / debt |
|---|---:|---|
| Truth and state | 2/2 | Last-snapshot, armed-request, conditional pass-through, unavailable reduction, and Delta output wording match exposed state. |
| Hierarchy and grouping | 2/2 | Stable topology and a clear core/activity/supporting-zone order. |
| Restraint and family fit | 2/2 | Flat technical surfaces and product-specific periwinkle without analyzer theater or unapproved branding. |
| Control and interaction clarity | 2/2 | Full targets, units, exact/reset/fine paths, focus, and neutral inactive-but-editable styling are explicit. |
| Typography and scaling | 2/2 | Bundled fonts and complete 100–200% scale renders have no clipping, crowding, or jitter. |
| Accessibility | 2/2 | Contrast, 20 unique controls, semantic Activity-first browse order, full keyboard paths, non-color cues, and persistent focus geometry pass. |
| Host correctness and performance | 2/2 | Separate embedded VST3/CLAP passes, balanced gestures, host synchronization/state recall, fixed sizing, bounded timers, validators, and immutable processor/DSP audit pass. |
| Polish and consistency | 2/2 | Alignment, rhythm, formatting, state styling, and interaction feedback form one coherent system. |
| **Total** | **16/16** | **SHIP for the UI boundary.** Pre-existing release metadata/signing debt remains outside this patch. |

## Pre-existing release and wrapper debt

These findings reproduce without the UI patch and are not silently broadened into this editor-only change:

1. JUCE derives `com.Suppressor Audio.Suppressor`, whose space makes the macOS VST3 bundle identifier invalid for notarized release. Product/manufacturer codes and the CLAP ID remain intentionally unchanged here.
2. Baseline and final macOS CLAP/Standalone bundles report `code has no resources but signature indicates they must be present`. Standalone is not packaged; release signing remains an owner follow-up.
3. clap-validator's random-state warning and the pinned wrapper's choice/integer metadata behavior are unchanged wrapper debt; deterministic state and UI synchronization checks pass.
4. The pinned CLAP wrapper reports `gui.hide == false` for Cocoa while `show`, host-window concealment, destruction, and fresh creation operate. This is recorded as wrapper behavior rather than presented as a UI state.

## Open constraints and design-system candidates

1. Safe lock/base/notch and band-lock snapshots require a separate processor transport change, which is outside this UI-only boundary.
2. Host bypass and external-key validity are not exposed; the UI does not invent them.
3. Product-specific accent variation and the neutral “owned by another topology” treatment are candidates for the central design system only after review across more plugins.
