#pragma once

#include "PluginLookAndFeel.h"
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class ActivityDisplay final : public juce::Component
{
public:
    void setState (float detectorDb, float gainReductionDb, float thresholdDb,
                   int bandMode, bool deltaAudition, bool multibandLearning);
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    class ValueInterface;
    juce::String accessibleValueText() const;
    juce::String semanticStatus() const;

    float detector = -180.0f;
    float reduction = 0.0f;
    float threshold = -40.0f;
    int mode = 0;
    bool delta = false;
    bool learning = false;
};

class SuppressorEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit SuppressorEditor (SuppressorProcessor&);
    ~SuppressorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    int getControlParameterIndex (juce::Component&) override;

private:
    struct Entry
    {
        juce::String id;
        ParameterSlider* slider = nullptr;
        juce::ComboBox* combo = nullptr;
        juce::Button* button = nullptr;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };

    void timerCallback() override;
    ParameterSlider& addKnob (const juce::String& parameterId, const juce::String& label);
    juce::ComboBox& addCombo (const juce::String& parameterId, const juce::String& label);
    juce::ToggleButton& addToggle (const juce::String& parameterId, const juce::String& label);
    void addAction (const juce::String& parameterId, juce::TextButton& button,
                    const juce::String& accessibleName);
    Entry* findEntry (const juce::String& parameterId) const;
    void placeKnob (const juce::String& parameterId, juce::Rectangle<int> bounds);
    void placeCombo (const juce::String& parameterId, juce::Rectangle<int> bounds);
    void placeButton (const juce::String& parameterId, juce::Rectangle<int> bounds);
    void updateModePresentation();
    void updateTimerState();
    void paintPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String&) const;

    SuppressorProcessor& proc;
    SuppressorLookAndFeel lookAndFeel;
    ActivityDisplay activityDisplay;
    juce::TooltipWindow tooltipWindow { this, 1200 };

    std::vector<std::unique_ptr<ParameterSlider>> sliders;
    std::vector<std::unique_ptr<juce::ComboBox>> comboBoxes;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggleButtons;
    std::vector<std::unique_ptr<Entry>> entries;

    ParameterTextButton humLearnButton { "LEARN" };
    ParameterTextButton bandsLearnButton { "LEARN" };
    juce::Label humStatus;
    juce::Label bandsStatus;

    juce::Rectangle<int> coreBounds;
    juce::Rectangle<int> activityBounds;
    juce::Rectangle<int> detectionBounds;
    juce::Rectangle<int> humBounds;
    juce::Rectangle<int> outputBounds;

    int nextFocusOrder = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuppressorEditor)
};
