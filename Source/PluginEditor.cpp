#include "PluginEditor.h"

#include <cmath>

class ActivityDisplay::ValueInterface final : public juce::AccessibilityValueInterface
{
public:
    explicit ValueInterface (ActivityDisplay& displayToUse) : display (displayToUse) {}

    bool isReadOnly() const override { return true; }
    double getCurrentValue() const override { return display.reduction; }
    juce::String getCurrentValueAsString() const override
    {
        return display.accessibleValueText();
    }
    void setValue (double) override {}
    void setValueAsString (const juce::String&) override {}
    AccessibleValueRange getRange() const override { return {}; }

private:
    ActivityDisplay& display;
};

void ActivityDisplay::setState (float detectorDb, float gainReductionDb,
                                float thresholdDb, int bandMode, bool deltaAudition,
                                bool multibandLearning)
{
    const auto previousStatus = semanticStatus();
    const auto newReduction = juce::jmax (0.0f, -gainReductionDb);
    const auto changed = juce::roundToInt (detector * 10.0f)
                             != juce::roundToInt (detectorDb * 10.0f)
                      || juce::roundToInt (reduction * 10.0f)
                             != juce::roundToInt (newReduction * 10.0f)
                      || juce::roundToInt (threshold * 10.0f)
                             != juce::roundToInt (thresholdDb * 10.0f)
                      || mode != bandMode || delta != deltaAudition
                      || learning != multibandLearning;

    if (! changed)
        return;

    detector = detectorDb;
    reduction = newReduction;
    threshold = thresholdDb;
    mode = bandMode;
    delta = deltaAudition;
    learning = multibandLearning;
    setDescription (accessibleValueText());
    repaint();

    if (semanticStatus() != previousStatus)
        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
}

juce::String ActivityDisplay::semanticStatus() const
{
    if (delta)
        return "OUTPUT: REMOVED SIGNAL";
    if (learning)
        return "LEARN ARMED";
    if (mode > 0)
        return "MULTIBAND MODE";
    if (detector <= -120.0f)
        return "NO SIGNAL";
    return reduction > 0.5f ? "REDUCTION OBSERVED" : "NO REDUCTION";
}

juce::String ActivityDisplay::accessibleValueText() const
{
    juce::String detectorText;
    if (mode > 0)
        detectorText = "per-band detector; global detector unavailable";
    else if (detector <= -120.0f)
        detectorText = "no input detected";
    else
        detectorText = "detector " + juce::String (detector, 1) + " decibels";

    const auto reductionText = learning
                                   ? juce::String ("gain reduction unavailable while multiband learning is armed; suppression passes while processing")
                                   : reduction >= 60.0f
                                         ? juce::String ("at least 60 decibels gain reduction")
                                         : juce::String (reduction, 1) + " decibels gain reduction";
    return "Last processing snapshot: " + semanticStatus().toLowerCase()
         + ", " + detectorText + ", " + reductionText;
}

std::unique_ptr<juce::AccessibilityHandler> ActivityDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::staticText, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface> (*this) });
}

void ActivityDisplay::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds();
    auto statusBounds = bounds.removeFromTop (54);
    const auto status = semanticStatus();
    const auto statusColour = status == "NO SIGNAL" || status == "LEARN ARMED"
                                   || status == "OUTPUT: REMOVED SIGNAL"
                                  ? SuppressorTheme::warning
                                  : SuppressorTheme::accent;

    graphics.setColour (statusColour);
    graphics.fillRoundedRectangle (static_cast<float> (statusBounds.getX()),
                                   static_cast<float> (statusBounds.getY() + 8),
                                   4.0f, 31.0f, 2.0f);
    graphics.setColour (SuppressorTheme::primaryText);
    graphics.setFont (SuppressorTheme::makeFont (27.0f, true, 0.02f));
    graphics.drawFittedText (status, statusBounds.withTrimmedLeft (14),
                             juce::Justification::centredLeft, 1);

    auto drawCaption = [&graphics] (juce::Rectangle<int> row, const juce::String& title,
                                    const juce::String& value)
    {
        graphics.setColour (SuppressorTheme::secondaryText);
        graphics.setFont (SuppressorTheme::makeFont (11.5f, true, 0.09f));
        graphics.drawText (title, row.removeFromLeft (row.getWidth() * 2 / 3),
                           juce::Justification::centredLeft);
        graphics.setColour (SuppressorTheme::primaryText);
        graphics.setFont (SuppressorTheme::makeFont (14.0f, true));
        graphics.drawText (value, row, juce::Justification::centredRight);
    };

    auto drawMeter = [&graphics] (juce::Rectangle<int> meterBounds, float proportion)
    {
        auto track = meterBounds.toFloat();
        graphics.setColour (SuppressorTheme::divider);
        graphics.fillRoundedRectangle (track, 2.5f);
        track.setWidth (track.getWidth() * juce::jlimit (0.0f, 1.0f, proportion));
        graphics.setColour (SuppressorTheme::accent);
        graphics.fillRoundedRectangle (track, 2.5f);
    };

    bounds.removeFromTop (4);
    auto detectorLabel = bounds.removeFromTop (22);
    if (mode > 0)
    {
        drawCaption (detectorLabel, "DETECTOR", "PER-BAND");
        auto message = bounds.removeFromTop (25);
        graphics.setColour (SuppressorTheme::secondaryText);
        graphics.setFont (SuppressorTheme::makeFont (13.0f));
        graphics.drawFittedText ("GLOBAL LEVEL NOT EXPOSED", message,
                                 juce::Justification::centredLeft, 1);
        bounds.removeFromTop (13);
    }
    else
    {
        const auto detectorText = detector <= -120.0f
                                      ? juce::String ("NO SIGNAL")
                                      : juce::String (detector, 1) + " dB";
        drawCaption (detectorLabel, "DETECTOR", detectorText);
        auto meter = bounds.removeFromTop (7).reduced (0, 1);
        drawMeter (meter, juce::jmap (juce::jlimit (-80.0f, 0.0f, detector),
                                     -80.0f, 0.0f, 0.0f, 1.0f));

        const auto thresholdPosition = juce::jmap (juce::jlimit (-80.0f, 0.0f, threshold),
                                                   -80.0f, 0.0f, 0.0f, 1.0f);
        const auto thresholdX = static_cast<float> (meter.getX())
                              + static_cast<float> (meter.getWidth()) * thresholdPosition;
        graphics.setColour (SuppressorTheme::primaryText);
        graphics.drawVerticalLine (juce::roundToInt (thresholdX),
                                   static_cast<float> (meter.getY() - 2),
                                   static_cast<float> (meter.getBottom() + 2));
        bounds.removeFromTop (25);
    }

    auto reductionLabel = bounds.removeFromTop (22);
    if (learning)
    {
        drawCaption (reductionLabel, "GAIN REDUCTION", "UNAVAILABLE");
        graphics.setColour (SuppressorTheme::secondaryText);
        graphics.setFont (SuppressorTheme::makeFont (13.0f));
        graphics.drawFittedText ("SUPPRESSION PASSES WHILE PROCESSING",
                                 bounds.removeFromTop (25),
                                 juce::Justification::centredLeft, 1);
    }
    else
    {
        const auto reductionText = reduction >= 60.0f
                                       ? juce::String ("60+ dB")
                                       : juce::String (reduction, 1) + " dB";
        drawCaption (reductionLabel, "GAIN REDUCTION", reductionText);
        auto reductionMeter = bounds.removeFromTop (7).reduced (0, 1);
        drawMeter (reductionMeter, juce::jlimit (0.0f, 1.0f, reduction / 60.0f));
    }

    bounds.removeFromTop (14);
    graphics.setColour (SuppressorTheme::secondaryText);
    graphics.setFont (SuppressorTheme::makeFont (12.5f));
    graphics.drawFittedText ("UPDATES WHILE PROCESSING", bounds.removeFromTop (18),
                             juce::Justification::centredLeft, 1);
}

SuppressorEditor::SuppressorEditor (SuppressorProcessor& processorToUse)
    : juce::AudioProcessorEditor (&processorToUse), proc (processorToUse)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    activityDisplay.setName ("Suppression activity");
    activityDisplay.setTitle ("Suppression activity");
    activityDisplay.setExplicitFocusOrder (nextFocusOrder++);
    addAndMakeVisible (activityDisplay);

    addCombo ("gateMode", "Gate Mode");
    addToggle ("adaptiveRelease", "Adaptive Release");
    auto& suppressionStrength = addKnob ("strength", "Strength");
    suppressionStrength.setName ("Suppression Strength");
    suppressionStrength.setTitle ("Suppression Strength");
    addKnob ("threshold", "Threshold");
    addKnob ("release", "Release");
    addKnob ("depth", "Depth");
    addKnob ("hysteresis", "Hysteresis");
    addKnob ("hold", "Hold");

    addKnob ("cue", "Transient Cue");
    addCombo ("sidechain", "Detector Source");
    addCombo ("lookahead", "Lookahead");

    addToggle ("humEnable", "Hum Removal");
    addCombo ("humBase", "Mains Family");
    addKnob ("humHarmonics", "Harmonics");
    addKnob ("humStrength", "Hum Strength");
    addAction ("humLearn", humLearnButton, "Learn hum profile");

    addCombo ("bandMode", "Band Mode");
    addAction ("bandsLearn", bandsLearnButton, "Learn multiband thresholds");
    addKnob ("outputGain", "Output Gain");
    addToggle ("deltaAudition", "Delta Audition");

    auto setupStatus = [this] (juce::Label& label, const juce::String& name)
    {
        label.setName (name);
        label.setTitle (name);
        label.setFont (SuppressorTheme::makeFont (13.0f, true, 0.03f));
        label.setColour (juce::Label::textColourId, SuppressorTheme::secondaryText);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setMinimumHorizontalScale (0.72f);
        addAndMakeVisible (label);
    };
    setupStatus (humStatus, "Hum learning status");
    setupStatus (bandsStatus, "Multiband learning status");

    setSize (1000, 640);
    updateModePresentation();
    timerCallback();
    updateTimerState();
}

SuppressorEditor::~SuppressorEditor()
{
    stopTimer();
    for (const auto& slider : sliders)
        slider->endActiveGesture();
    entries.clear();
    setLookAndFeel (nullptr);
}

ParameterSlider& SuppressorEditor::addKnob (const juce::String& parameterId,
                                             const juce::String& labelText)
{
    sliders.push_back (std::make_unique<ParameterSlider>());
    auto* slider = sliders.back().get();
    addAndMakeVisible (*slider);
    slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                 juce::MathConstants<float>::pi * 2.75f, true);
    slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 88, 24);
    slider->setMouseDragSensitivity (220);
    slider->setVelocityBasedMode (false);
    slider->setVelocityModeParameters (0.24, 1, 0.0, true,
                                       juce::ModifierKeys::shiftModifier);
    slider->setScrollWheelEnabled (false);
    slider->setWantsKeyboardFocus (true);
    slider->setExplicitFocusOrder (nextFocusOrder++);
    slider->setComponentID (parameterId);
    slider->setName (labelText);
    slider->setTitle (labelText);
    slider->setDescription ("Drag to adjust. Shift-drag for fine control. Double-click or press Return for exact entry. Alt/Option-click resets to default.");
    slider->setTooltip ("Return/double-click: exact. Shift-drag: fine. Alt/Option-click: reset.");

    auto entry = std::make_unique<Entry>();
    entry->id = parameterId;
    entry->slider = slider;
    entry->label = std::make_unique<juce::Label>();
    entry->label->setText (labelText.toUpperCase(), juce::dontSendNotification);
    entry->label->setFont (SuppressorTheme::makeFont (11.5f, true, 0.08f));
    entry->label->setColour (juce::Label::textColourId, SuppressorTheme::secondaryText);
    entry->label->setJustificationType (juce::Justification::centred);
    entry->label->setInterceptsMouseClicks (false, false);
    entry->label->setAccessible (false);
    addAndMakeVisible (*entry->label);
    entry->sliderAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, parameterId, *slider);
    if (auto* parameter = proc.apvts.getParameter (parameterId))
    {
        slider->setDoubleClickReturnValue (
            true, parameter->convertFrom0to1 (parameter->getDefaultValue()),
            juce::ModifierKeys::altModifier);
        slider->setKeyboardGestureCallbacks (
            [parameter] { parameter->beginChangeGesture(); },
            [parameter] { parameter->endChangeGesture(); });
        if (parameter->getLabel().isNotEmpty())
            slider->setTextValueSuffix (" " + parameter->getLabel());
    }
    entries.push_back (std::move (entry));
    return *slider;
}

juce::ComboBox& SuppressorEditor::addCombo (const juce::String& parameterId,
                                             const juce::String& labelText)
{
    comboBoxes.push_back (std::make_unique<juce::ComboBox>());
    auto* combo = comboBoxes.back().get();
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (
            proc.apvts.getParameter (parameterId)))
        combo->addItemList (parameter->choices, 1);
    combo->setJustificationType (juce::Justification::centredLeft);
    combo->setWantsKeyboardFocus (true);
    combo->setExplicitFocusOrder (nextFocusOrder++);
    combo->setComponentID (parameterId);
    combo->setName (labelText);
    combo->setTitle (labelText);
    combo->setDescription ("Choose " + labelText.toLowerCase());
    combo->setTooltip (combo->getDescription());
    addAndMakeVisible (*combo);

    auto entry = std::make_unique<Entry>();
    entry->id = parameterId;
    entry->combo = combo;
    entry->label = std::make_unique<juce::Label>();
    entry->label->setText (labelText.toUpperCase(), juce::dontSendNotification);
    entry->label->setFont (SuppressorTheme::makeFont (11.5f, true, 0.08f));
    entry->label->setColour (juce::Label::textColourId, SuppressorTheme::secondaryText);
    entry->label->setJustificationType (juce::Justification::centredLeft);
    entry->label->setInterceptsMouseClicks (false, false);
    entry->label->setAccessible (false);
    addAndMakeVisible (*entry->label);
    entry->comboAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, parameterId, *combo);
    entries.push_back (std::move (entry));

    if (parameterId == "gateMode" || parameterId == "bandMode")
        combo->onChange = [this]
        {
            updateModePresentation();
            timerCallback();
        };

    return *combo;
}

juce::ToggleButton& SuppressorEditor::addToggle (const juce::String& parameterId,
                                                  const juce::String& labelText)
{
    toggleButtons.push_back (std::make_unique<ParameterToggleButton> (labelText.toUpperCase()));
    auto* button = toggleButtons.back().get();
    button->setWantsKeyboardFocus (true);
    button->setExplicitFocusOrder (nextFocusOrder++);
    button->setComponentID (parameterId);
    button->setName (labelText);
    button->setTitle (labelText);
    button->setTooltip ("Toggle " + labelText.toLowerCase());
    addAndMakeVisible (*button);

    auto entry = std::make_unique<Entry>();
    entry->id = parameterId;
    entry->button = button;
    entry->buttonAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, parameterId, *button);
    entries.push_back (std::move (entry));

    if (parameterId == "humEnable")
        button->onClick = [this] { updateModePresentation(); };

    return *button;
}

void SuppressorEditor::addAction (const juce::String& parameterId,
                                  juce::TextButton& button,
                                  const juce::String& accessibleName)
{
    button.setClickingTogglesState (true);
    button.setWantsKeyboardFocus (true);
    button.setExplicitFocusOrder (nextFocusOrder++);
    button.setComponentID (parameterId);
    button.setName (accessibleName);
    button.setTitle (accessibleName);
    button.setTooltip (accessibleName);
    addAndMakeVisible (button);

    auto entry = std::make_unique<Entry>();
    entry->id = parameterId;
    entry->button = &button;
    entry->buttonAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, parameterId, button);
    entries.push_back (std::move (entry));
}

SuppressorEditor::Entry* SuppressorEditor::findEntry (const juce::String& parameterId) const
{
    for (const auto& entry : entries)
        if (entry->id == parameterId)
            return entry.get();
    return nullptr;
}

void SuppressorEditor::placeKnob (const juce::String& parameterId,
                                  juce::Rectangle<int> bounds)
{
    if (auto* entry = findEntry (parameterId))
    {
        entry->label->setBounds (bounds.removeFromTop (18));
        entry->slider->setBounds (bounds);
    }
}

void SuppressorEditor::placeCombo (const juce::String& parameterId,
                                   juce::Rectangle<int> bounds)
{
    if (auto* entry = findEntry (parameterId))
    {
        entry->label->setBounds (bounds.getX(), bounds.getY() - 17,
                                 bounds.getWidth(), 15);
        entry->combo->setBounds (bounds);
    }
}

void SuppressorEditor::placeButton (const juce::String& parameterId,
                                    juce::Rectangle<int> bounds)
{
    if (auto* entry = findEntry (parameterId))
        entry->button->setBounds (bounds);
}

void SuppressorEditor::resized()
{
    coreBounds = { 18, 82, 650, 250 };
    activityBounds = { 680, 82, 302, 250 };
    detectionBounds = { 18, 344, 262, 278 };
    humBounds = { 292, 344, 392, 278 };
    outputBounds = { 696, 344, 286, 278 };

    placeCombo ("gateMode", { coreBounds.getX() + 14, coreBounds.getY() + 51, 168, 34 });
    placeButton ("adaptiveRelease",
                 { coreBounds.getX() + 196, coreBounds.getY() + 51, 178, 34 });

    const auto knobWidth = (coreBounds.getWidth() - 28) / 6;
    const juce::StringArray coreKnobs { "strength", "threshold", "release",
                                        "depth", "hysteresis", "hold" };
    for (int index = 0; index < coreKnobs.size(); ++index)
        placeKnob (coreKnobs[index],
                   { coreBounds.getX() + 14 + index * knobWidth,
                     coreBounds.getY() + 94, knobWidth, 142 });

    auto activityContent = activityBounds.reduced (14);
    activityContent.removeFromTop (28);
    activityDisplay.setBounds (activityContent);

    placeKnob ("cue", { detectionBounds.getX() + 14, detectionBounds.getY() + 41,
                         110, 218 });
    placeCombo ("sidechain",
                { detectionBounds.getX() + 138, detectionBounds.getY() + 69, 110, 34 });
    placeCombo ("lookahead",
                { detectionBounds.getX() + 138, detectionBounds.getY() + 145, 110, 34 });

    placeButton ("humEnable",
                 { humBounds.getX() + 14, humBounds.getY() + 48, 132, 34 });
    placeCombo ("humBase",
                { humBounds.getX() + 250, humBounds.getY() + 48, 128, 34 });
    placeKnob ("humHarmonics",
               { humBounds.getX() + 14, humBounds.getY() + 96, 108, 164 });
    placeKnob ("humStrength",
               { humBounds.getX() + 126, humBounds.getY() + 96, 108, 164 });
    placeButton ("humLearn",
                 { humBounds.getX() + 250, humBounds.getY() + 111, 128, 36 });
    humStatus.setBounds (humBounds.getX() + 250, humBounds.getY() + 154, 128, 62);

    placeCombo ("bandMode",
                { outputBounds.getX() + 14, outputBounds.getY() + 50, 122, 34 });
    placeButton ("bandsLearn",
                 { outputBounds.getX() + 150, outputBounds.getY() + 50, 122, 34 });
    bandsStatus.setBounds (outputBounds.getX() + 14, outputBounds.getY() + 91, 258, 25);
    placeKnob ("outputGain",
               { outputBounds.getX() + 14, outputBounds.getY() + 139, 110, 124 });
    placeButton ("deltaAudition",
                 { outputBounds.getX() + 140, outputBounds.getY() + 175, 132, 38 });
}

void SuppressorEditor::updateModePresentation()
{
    const auto bandMode = findEntry ("bandMode")->combo->getSelectedItemIndex();
    const auto gateMode = findEntry ("gateMode")->combo->getSelectedItemIndex();
    const auto humEnabled = findEntry ("humEnable")->button->getToggleState();

    auto setEmphasis = [this] (const juce::String& parameterId, bool active)
    {
        if (auto* entry = findEntry (parameterId))
            if (entry->slider != nullptr)
            {
                entry->slider->setColour (juce::Slider::rotarySliderFillColourId,
                                          active ? SuppressorTheme::accent
                                                 : SuppressorTheme::inactiveControl);
                entry->slider->repaint();
            }
    };

    setEmphasis ("strength", bandMode <= 0);
    setEmphasis ("threshold", bandMode <= 0);
    setEmphasis ("depth", gateMode <= 0);
    setEmphasis ("humHarmonics", humEnabled);
    setEmphasis ("humStrength", humEnabled);

    auto setApplicability = [this] (const juce::String& parameterId, bool active,
                                    const juce::String& inactiveReason)
    {
        if (auto* entry = findEntry (parameterId))
            if (entry->slider != nullptr)
            {
                const auto interaction = " Drag to adjust; Shift-drag is fine control; double-click or Return opens exact entry; Alt/Option-click resets.";
                const auto description = active
                                             ? entry->slider->getName() + "." + interaction
                                             : "Inactive but editable. " + inactiveReason + interaction;
                entry->slider->setDescription (description);
                entry->slider->setTooltip (active
                                               ? "Return/double-click: exact. Shift-drag: fine. Alt/Option-click: reset."
                                               : inactiveReason + " Still editable.");
            }
    };

    setApplicability ("strength", bandMode <= 0, "Applies only in One-Split mode.");
    setApplicability ("threshold", bandMode <= 0, "Global Threshold applies only in One-Split mode.");
    setApplicability ("depth", gateMode <= 0, "Tight Gate uses a hard-zero floor; this Reduction depth is retained for automation and preconfiguration.");
    setApplicability ("humHarmonics", humEnabled, "Hum Removal is off; this setting is retained.");
    setApplicability ("humStrength", humEnabled, "Hum Removal is off; this setting is retained.");
}

void SuppressorEditor::timerCallback()
{
    const auto desiredIntervalMs = isShowing() ? 50 : 500;
    if (getTimerInterval() != desiredIntervalMs)
        startTimer (desiredIntervalMs);

    const auto bandMode = juce::jmax (0, findEntry ("bandMode")->combo->getSelectedItemIndex());
    const auto threshold = static_cast<float> (findEntry ("threshold")->slider->getValue());
    const auto deltaAudition = findEntry ("deltaAudition")->button->getToggleState();
    const auto bandsLearning = bandsLearnButton.getToggleState();
    activityDisplay.setState (proc.detectorDb(), proc.gainReductionDb(), threshold,
                              bandMode, deltaAudition, bandsLearning && bandMode > 0);

    const auto humLearning = humLearnButton.getToggleState();
    humLearnButton.setButtonText (humLearning ? "STOP LEARNING" : "LEARN");
    humLearnButton.setName (humLearning ? "Stop hum learning" : "Learn hum profile");
    humLearnButton.setTitle (humLearnButton.getName());
    humLearnButton.setTooltip (humLearning
                                    ? "Stop the armed request. Capture and hum pass-through occur while processing."
                                    : "Arm hum-profile learning; capture begins while processing.");
    humStatus.setText (humLearning ? "ARMED / PASSES IN PROCESS"
                                   : "READY TO LEARN",
                       juce::dontSendNotification);

    const auto multibandMode = bandMode > 0;
    bandsLearnButton.setButtonText (bandsLearning ? "STOP LEARNING"
                                                   : multibandMode ? "LEARN"
                                                                   : "ARM LEARNING");
    bandsLearnButton.setName (bandsLearning ? "Stop multiband learning"
                                            : multibandMode ? "Learn multiband thresholds"
                                                            : "Arm multiband learning");
    bandsLearnButton.setTitle (bandsLearnButton.getName());
    bandsLearnButton.setTooltip (
        bandsLearning
            ? multibandMode
                  ? "Stop the armed request. Suppression passes while learning during processing."
                  : "Stop the armed request; it is waiting for 4/6-Band."
            : multibandMode
                  ? "Arm multiband learning; suppression passes while processing."
                  : "Arm multiband learning now; capture waits for 4/6-Band processing.");
    if (bandsLearning)
        bandsStatus.setText (multibandMode ? "ARMED / PASSES WHILE PROCESSING"
                                           : "ARMED / WAITING FOR 4/6-BAND",
                             juce::dontSendNotification);
    else
        bandsStatus.setText (multibandMode ? "READY TO LEARN"
                                           : "CAN ARM FOR 4/6-BAND",
                             juce::dontSendNotification);

    updateModePresentation();
}

void SuppressorEditor::visibilityChanged()
{
    updateTimerState();
}

void SuppressorEditor::updateTimerState()
{
    startTimer (isShowing() ? 50 : 500);
}

int SuppressorEditor::getControlParameterIndex (juce::Component& component)
{
    for (const auto& entry : entries)
    {
        auto* control = entry->slider != nullptr
                            ? static_cast<juce::Component*> (entry->slider)
                            : (entry->combo != nullptr
                                   ? static_cast<juce::Component*> (entry->combo)
                                   : static_cast<juce::Component*> (entry->button));
        if (control == &component || control->isParentOf (&component))
            return proc.getParameters().indexOf (proc.apvts.getParameter (entry->id));
    }

    return -1;
}

void SuppressorEditor::paintPanel (juce::Graphics& graphics,
                                   juce::Rectangle<int> bounds,
                                   const juce::String& title) const
{
    const auto panel = bounds.toFloat();
    graphics.setColour (SuppressorTheme::surface);
    graphics.fillRoundedRectangle (panel, SuppressorTheme::cornerRadius);
    graphics.setColour (SuppressorTheme::divider);
    graphics.drawRoundedRectangle (panel.reduced (0.5f), SuppressorTheme::cornerRadius, 1.0f);

    graphics.setColour (SuppressorTheme::secondaryText);
    graphics.setFont (SuppressorTheme::makeFont (11.5f, true, 0.10f));
    graphics.drawText (title, bounds.getX() + 14, bounds.getY() + 8,
                       bounds.getWidth() - 28, 15, juce::Justification::centredLeft);
    graphics.setColour (SuppressorTheme::divider);
    graphics.drawHorizontalLine (bounds.getY() + 29,
                                 static_cast<float> (bounds.getX() + 14),
                                 static_cast<float> (bounds.getRight() - 14));
}

void SuppressorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (SuppressorTheme::canvas);
    graphics.setColour (SuppressorTheme::header);
    graphics.fillRect (0, 0, getWidth(), 64);
    graphics.setColour (SuppressorTheme::divider);
    graphics.drawHorizontalLine (63, 0.0f, static_cast<float> (getWidth()));

    graphics.setColour (SuppressorTheme::accent);
    graphics.fillRoundedRectangle (18.0f, 16.0f, 5.0f, 32.0f, 2.5f);
    graphics.setColour (SuppressorTheme::primaryText);
    graphics.setFont (SuppressorTheme::makeFont (24.0f, true, 0.03f));
    graphics.drawText ("SUPPRESSOR", 36, 8, 220, 29, juce::Justification::centredLeft);
    graphics.setColour (SuppressorTheme::secondaryText);
    graphics.setFont (SuppressorTheme::makeFont (11.0f, true, 0.11f));
    graphics.drawText ("FREQUENCY-SPLIT NOISE CONTROL", 36, 35, 300, 15,
                       juce::Justification::centredLeft);
    graphics.drawText ("DYNAMIC HIGH-BAND PROCESSOR", getWidth() - 300, 24, 282, 16,
                       juce::Justification::centredRight);

    paintPanel (graphics, coreBounds, "SUPPRESSION CORE");
    paintPanel (graphics, activityBounds, "ACTIVITY / LAST PROCESSING SNAPSHOT");
    paintPanel (graphics, detectionBounds, "DETECTION");
    paintPanel (graphics, humBounds, "HUM FILTER");
    paintPanel (graphics, outputBounds, "MULTIBAND / OUTPUT");

    graphics.setColour (SuppressorTheme::divider);
    graphics.drawHorizontalLine (outputBounds.getY() + 128,
                                 static_cast<float> (outputBounds.getX() + 14),
                                 static_cast<float> (outputBounds.getRight() - 14));
}
