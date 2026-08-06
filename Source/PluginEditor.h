#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class SuppressorEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit SuppressorEditor (SuppressorProcessor&);
    ~SuppressorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Entry
    {
        juce::String id;
        juce::Slider* slider = nullptr;              // when a knob
        juce::ComboBox* combo = nullptr;             // when a combo
        juce::Button* button = nullptr;              // when a toggle
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };

    void addKnob   (const juce::String& paramId, const juce::String& text, juce::Component& parent);
    void addToggle (const juce::String& paramId, const juce::String& text, juce::Component& parent);
    void addCombo  (const juce::String& paramId, const juce::String& text, juce::Component& parent);
    juce::GroupComponent& addGroup (const juce::String& title, juce::Rectangle<int> bounds);
    void place (const juce::String& paramId, juce::Rectangle<int> knobBounds,
                bool labelBelow = true);

    SuppressorProcessor& proc;
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::ComboBox>> comboBoxes;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggleButtons;
    std::vector<std::unique_ptr<Entry>> entries;
    std::vector<std::unique_ptr<juce::GroupComponent>> groups;

    juce::TextButton humLearnButton { "Learn" };
    juce::TextButton bandsLearnButton { "Learn" };
    juce::Label humStatus, bandsStatus;
    juce::Label detLabel, grLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuppressorEditor)
};
