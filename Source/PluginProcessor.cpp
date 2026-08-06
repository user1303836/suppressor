#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> exponentialReleaseRange()
{
    // R(r) = 2 * 15^r  ->  2 ms .. 30 ms, 6 ms at r ~= 0.4057
    return juce::NormalisableRange<float> (
        2.0f, 30.0f,
        [] (float, float, float x) { return 2.0f * std::pow (15.0f, x); },
        [] (float, float, float v) { return std::log (v / 2.0f) / std::log (15.0f); });
}

// 0..1 parameter displayed as a percentage; the inverse parser must map
// the displayed number back to the 0..1 domain for exact round-trips.
juce::AudioParameterFloatAttributes percentAttrs (int decimals)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("%")
        .withStringFromValueFunction ([decimals] (float v, int) {
            return juce::String (v * 100.0f, decimals); })
        .withValueFromStringFunction ([] (const juce::String& s) {
            return s.retainCharacters ("0123456789.+-").getFloatValue() / 100.0f; });
}

// Release: exponential mapping with a stable 0.1 ms text round-trip.
// (JUCE applies the range mapping OUTSIDE these functions: stringFromValue
// receives milliseconds, valueFromString must return milliseconds.)
juce::AudioParameterFloatAttributes releaseAttrs()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("ms")
        .withStringFromValueFunction ([] (float ms, int) { return juce::String (ms, 1); })
        .withValueFromStringFunction ([] (const juce::String& s) {
            return s.retainCharacters ("0123456789.+-").getFloatValue(); });
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout SuppressorProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using API = juce::AudioParameterInt;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---------------- Core ----------------
    layout.add (std::make_unique<APF> (juce::ParameterID { "strength", 1 }, "Strength",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.9f, percentAttrs (1)));
    layout.add (std::make_unique<APF> (juce::ParameterID { "threshold", 1 }, "Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -40.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<APF> (juce::ParameterID { "release", 1 }, "Release",
        exponentialReleaseRange(), 6.0f, releaseAttrs()));

    // ---------------- Gate refinement ----------------
    layout.add (std::make_unique<APC> (juce::ParameterID { "gateMode", 1 }, "Gate Mode",
        juce::StringArray { "Reduction", "Tight Gate" }, 0));
    layout.add (std::make_unique<APF> (juce::ParameterID { "depth", 1 }, "Depth",
        juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 40.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<APF> (juce::ParameterID { "hysteresis", 1 }, "Hysteresis",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<APF> (juce::ParameterID { "hold", 1 }, "Hold",
        juce::NormalisableRange<float> (0.0f, 50.0f, 0.1f), 2.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<APB> (juce::ParameterID { "adaptiveRelease", 1 },
        "Adaptive Release", false));

    // ---------------- Detection ----------------
    layout.add (std::make_unique<APF> (juce::ParameterID { "cue", 1 }, "Transient Cue",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percentAttrs (0)));
    layout.add (std::make_unique<APC> (juce::ParameterID { "sidechain", 1 }, "Detector Source",
        juce::StringArray { "Internal", "External Key" }, 0));
    layout.add (std::make_unique<APC> (juce::ParameterID { "lookahead", 1 }, "Lookahead",
        juce::StringArray { "0 ms", "1 ms", "2 ms" }, 0));

    // ---------------- Hum ----------------
    layout.add (std::make_unique<APB> (juce::ParameterID { "humEnable", 1 }, "Hum Removal", false));
    layout.add (std::make_unique<APC> (juce::ParameterID { "humBase", 1 }, "Hum Base",
        juce::StringArray { "Auto", "50 Hz", "60 Hz" }, 0));
    layout.add (std::make_unique<API> (juce::ParameterID { "humHarmonics", 1 }, "Hum Harmonics",
        1, 16, 8));
    layout.add (std::make_unique<APF> (juce::ParameterID { "humStrength", 1 }, "Hum Strength",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percentAttrs (0)));
    layout.add (std::make_unique<APB> (juce::ParameterID { "humLearn", 1 }, "Hum Learn", false));

    // ---------------- Bands ----------------
    layout.add (std::make_unique<APC> (juce::ParameterID { "bandMode", 1 }, "Band Mode",
        juce::StringArray { "One-Split", "4-Band", "6-Band" }, 0));
    layout.add (std::make_unique<APB> (juce::ParameterID { "bandsLearn", 1 }, "Bands Learn", false));

    // ---------------- Output ----------------
    layout.add (std::make_unique<APB> (juce::ParameterID { "deltaAudition", 1 },
        "Delta (Removed) Audition", false));
    layout.add (std::make_unique<APF> (juce::ParameterID { "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

SuppressorProcessor::SuppressorProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // keep the engine valid from birth so editor/status queries are safe
    // even before (or without) prepareToPlay
    engine.prepare (44100.0, 2, 256);
}

void SuppressorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    if (sampleRate <= 0.0)
        return; // stress hosts may probe 0/0: keep the previous state
    engine.prepare (sampleRate, 2, (int) std::ceil (0.0025 * sampleRate));
    currentLatency = (int) std::round (apvts.getRawParameterValue ("lookahead")->load()
                                       * 0.001 * sampleRate);
    setLatencySamples (currentLatency);
    engine.setTargets (readTargets (sampleRate));
    engine.reset();
}

void SuppressorProcessor::releaseResources() {}

bool SuppressorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainInputChannelSet();
    if (main != layouts.getMainOutputChannelSet())
        return false;
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo())
        return false;
    const auto& sc = layouts.getChannelSet (true, 1);
    if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono()
                             && sc != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

suppressor::EngineParams SuppressorProcessor::readTargets (double sampleRate) const
{
    suppressor::EngineParams p;
    p.strength01    = apvts.getRawParameterValue ("strength")->load();
    p.thresholdDb   = apvts.getRawParameterValue ("threshold")->load();
    p.releaseMs     = apvts.getRawParameterValue ("release")->load();
    p.hysteresisDb  = apvts.getRawParameterValue ("hysteresis")->load();
    p.holdMs        = apvts.getRawParameterValue ("hold")->load();
    p.depthDb       = apvts.getRawParameterValue ("depth")->load();
    p.tightGate     = apvts.getRawParameterValue ("gateMode")->load() > 0.5f;
    p.adaptiveRelease = apvts.getRawParameterValue ("adaptiveRelease")->load() > 0.5f;
    p.cueAmount     = apvts.getRawParameterValue ("cue")->load();
    p.externalKey   = apvts.getRawParameterValue ("sidechain")->load() > 0.5f;
    p.bandMode      = (int) apvts.getRawParameterValue ("bandMode")->load();
    p.deltaAudition = apvts.getRawParameterValue ("deltaAudition")->load() > 0.5f;
    p.outputGainDb  = apvts.getRawParameterValue ("outputGain")->load();
    p.lookaheadSamples = (int) std::round (apvts.getRawParameterValue ("lookahead")->load()
                                           * 0.001 * sampleRate);
    return p;
}

void SuppressorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const double fs = getSampleRate();
    auto targets = readTargets (fs);

    if (targets.lookaheadSamples != currentLatency)
    {
        currentLatency = targets.lookaheadSamples;
        latencyDirty.store (true);
        triggerAsyncUpdate(); // setLatencySamples on the message thread
    }

    engine.setTargets (targets);

    // ---- learn triggers ------------------------------------------------
    const bool humReq = apvts.getRawParameterValue ("humLearn")->load() > 0.5f;
    if (humReq && ! humLearningNow)
    {
        for (int ch = 0; ch < engine.numChannels(); ++ch)
            engine.hum (ch).startLearn();
        humLearningNow = true;
    }
    else if (! humReq && humLearningNow)
    {
        for (int ch = 0; ch < engine.numChannels(); ++ch)
            engine.hum (ch).finishLearn();
        humLearningNow = false;
    }
    else if (humLearningNow && ! engine.hum (0).learning())
    {
        humLearningNow = false; // capture completed by the engine
        humAutoFinished.store (true);
        triggerAsyncUpdate();   // reset the parameter on the message thread
    }

    const bool bandsReq = apvts.getRawParameterValue ("bandsLearn")->load() > 0.5f;
    if (bandsReq && ! bandsLearningNow)
    {
        for (int ch = 0; ch < engine.numChannels(); ++ch)
            engine.multi (ch).startLearn();
        bandsLearningNow = true;
    }
    else if (! bandsReq && bandsLearningNow)
    {
        for (int ch = 0; ch < engine.numChannels(); ++ch)
            engine.multi (ch).finishLearn();
        bandsLearningNow = false;
    }

    // ---- hum structural params -----------------------------------------
    for (int ch = 0; ch < engine.numChannels(); ++ch)
    {
        auto& h = engine.hum (ch);
        h.setEnabled (apvts.getRawParameterValue ("humEnable")->load() > 0.5f);
        const int baseSel = (int) apvts.getRawParameterValue ("humBase")->load();
        h.setBaseHz (baseSel == 1 ? 50.0 : (baseSel == 2 ? 60.0 : 0.0));
        h.setNumHarmonics ((int) apvts.getRawParameterValue ("humHarmonics")->load());
        h.setStrength (apvts.getRawParameterValue ("humStrength")->load());
    }

    // ---- channel buffers ------------------------------------------------
    auto mainBus = getBusBuffer (buffer, true, 0);
    const int numMain = mainBus.getNumChannels();
    float* io[2] = { numMain > 0 ? mainBus.getWritePointer (0) : nullptr,
                     numMain > 1 ? mainBus.getWritePointer (1) : nullptr };

    const float* sc[2] = { nullptr, nullptr };
    int numSc = 0;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto scBus = getBusBuffer (buffer, true, 1);
        numSc = scBus.getNumChannels();
        if (numSc > 0) sc[0] = scBus.getReadPointer (0);
        if (numSc > 1) sc[1] = scBus.getReadPointer (1);
    }

    engine.processBlock (io, sc, numSc, buffer.getNumSamples(), numMain);

    detectorDbAtomic.store (engine.detectorLevelDb());
    grDbAtomic.store (engine.gainReductionDb());
}

void SuppressorProcessor::processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    midi.clear();
}

void SuppressorProcessor::handleAsyncUpdate()
{
    if (latencyDirty.exchange (false))
        setLatencySamples (currentLatency);
    if (humAutoFinished.exchange (false))
        if (auto* prm = apvts.getParameter ("humLearn"))
            prm->setValueNotifyingHost (0.0f);
}

// ------------------------------------------------------------------------
bool SuppressorProcessor::humLearning() const noexcept { return humLearningNow; }
bool SuppressorProcessor::humLocked() const noexcept
{
    if (engine.numChannels() == 0) return false;
    return const_cast<suppressor::DenoiserEngine&> (engine).hum (0).locked();
}
bool SuppressorProcessor::bandsLearning() const noexcept { return bandsLearningNow; }
bool SuppressorProcessor::bandsLocked() const noexcept
{
    if (engine.numChannels() == 0) return false;
    return const_cast<suppressor::DenoiserEngine&> (engine).multi (0).locked();
}
int SuppressorProcessor::humActiveDips() const noexcept
{
    if (engine.numChannels() == 0) return 0;
    return const_cast<suppressor::DenoiserEngine&> (engine).hum (0).activeHarmonicCount();
}
double SuppressorProcessor::humDetectedBaseHz() const noexcept
{
    if (engine.numChannels() == 0) return 60.0;
    return const_cast<suppressor::DenoiserEngine&> (engine).hum (0).baseHz();
}

// ------------------------------------------------------------------------
juce::AudioProcessorEditor* SuppressorProcessor::createEditor()
{
    return new SuppressorEditor (*this);
}

void SuppressorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SuppressorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

        // APVTS::replaceState swaps the ValueTree without firing parameter
        // notifications; let wrappers know every value must be re-queried.
        updateHostDisplay (juce::AudioProcessorListener::ChangeDetails()
                               .withParameterInfoChanged (true));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuppressorProcessor();
}
