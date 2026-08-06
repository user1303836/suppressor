#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/DenoiserEngine.h"

class SuppressorProcessor : public juce::AudioProcessor,
                            private juce::AsyncUpdater
{
public:
    SuppressorProcessor();
    ~SuppressorProcessor() override = default;

    // ------------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // ------------------------------------------------------------------
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

    // Metering / status for the editor (read on the message thread)
    float detectorDb() const noexcept   { return detectorDbAtomic.load(); }
    float gainReductionDb() const noexcept { return grDbAtomic.load(); }
    bool humLearning() const noexcept;
    bool humLocked() const noexcept;
    bool bandsLearning() const noexcept;
    bool bandsLocked() const noexcept;
    int  humActiveDips() const noexcept;
    double humDetectedBaseHz() const noexcept;
    void requestHumLearnStop()    { humLearnRequest = false; }
    void requestBandsLearnStop()  { bandsLearnRequest = false; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void handleAsyncUpdate() override;
    suppressor::EngineParams readTargets (double sampleRate) const;

    suppressor::DenoiserEngine engine;

    std::atomic<float> detectorDbAtomic { -120.0f };
    std::atomic<float> grDbAtomic { 0.0f };
    std::atomic<bool>  latencyDirty { false };
    std::atomic<bool>  humAutoFinished { false };

    bool humLearnRequest = false, humLearningNow = false;
    bool bandsLearnRequest = false, bandsLearningNow = false;
    int currentLatency = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuppressorProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
