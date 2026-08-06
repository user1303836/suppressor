#include "PluginEditor.h"

namespace
{
const juce::Colour kBg      { 0xff17181c };
const juce::Colour kPanel   { 0xff22242a };
const juce::Colour kAccent  { 0xff4fc3f7 };
const juce::Colour kText    { 0xffe0e0e0 };
constexpr int kKnobW = 78, kKnobH = 84, kLabelH = 16;
} // namespace

SuppressorEditor::SuppressorEditor (SuppressorProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    auto& look = getLookAndFeel();
    look.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    look.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3d45));
    look.setColour (juce::Label::textColourId, kText);
    look.setColour (juce::ComboBox::backgroundColourId, kPanel);
    look.setColour (juce::ComboBox::textColourId, kText);
    look.setColour (juce::ToggleButton::textColourId, kText);
    look.setColour (juce::GroupComponent::outlineColourId, juce::Colour (0xff3a3d45));
    look.setColour (juce::GroupComponent::textColourId, kAccent);

    auto& core  = addGroup ("Core",        { 10,  10, 260, 200 });
    addKnob ("strength",  "Strength",  core);
    addKnob ("threshold", "Threshold", core);
    addKnob ("release",   "Release",   core);

    auto& gate = addGroup ("Gate",        { 280, 10, 300, 200 });
    addCombo ("gateMode",   "Mode",           gate);
    addKnob  ("depth",      "Depth",          gate);
    addKnob  ("hysteresis", "Hysteresis",     gate);
    addKnob  ("hold",       "Hold",           gate);
    addToggle ("adaptiveRelease", "Adaptive Release", gate);

    auto& det = addGroup ("Detection",    { 590, 10, 260, 200 });
    addKnob  ("cue",       "Transient Cue", det);
    addCombo ("sidechain", "Source",        det);
    addCombo ("lookahead", "Lookahead",     det);

    auto& hum = addGroup ("Hum Removal",  { 10, 220, 260, 220 });
    addToggle ("humEnable",    "Enable",    hum);
    addCombo  ("humBase",      "Base",      hum);
    addKnob   ("humHarmonics", "Harmonics", hum);
    addKnob   ("humStrength",  "Strength",  hum);
    hum.addAndMakeVisible (humLearnButton);
    hum.addAndMakeVisible (humStatus);
    humLearnButton.onClick = [this]
    {
        proc.apvts.getParameter ("humLearn")->setValueNotifyingHost (
            proc.humLearning() ? 0.0f : 1.0f);
    };

    auto& bands = addGroup ("Multiband", { 280, 220, 300, 220 });
    addCombo ("bandMode", "Mode", bands);
    bands.addAndMakeVisible (bandsLearnButton);
    bands.addAndMakeVisible (bandsStatus);
    bandsLearnButton.onClick = [this]
    {
        proc.apvts.getParameter ("bandsLearn")->setValueNotifyingHost (
            proc.bandsLearning() ? 0.0f : 1.0f);
    };

    auto& out = addGroup ("Output",       { 590, 220, 260, 220 });
    addToggle ("deltaAudition", "Delta Audition", out);
    addKnob   ("outputGain",    "Gain",           out);
    out.addAndMakeVisible (detLabel);
    out.addAndMakeVisible (grLabel);

    setSize (860, 520);
    startTimerHz (15);
}

void SuppressorEditor::addKnob (const juce::String& paramId, const juce::String& text,
                                juce::Component& parent)
{
    sliders.push_back (std::make_unique<juce::Slider>());
    auto* s = sliders.back().get();
    s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                            juce::MathConstants<float>::pi * 2.75f, true);
    parent.addAndMakeVisible (*s);

    auto e = std::make_unique<Entry>();
    e->id = paramId;
    e->slider = s;
    e->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramId, *s);
    e->label = std::make_unique<juce::Label>();
    e->label->setText (text, juce::dontSendNotification);
    e->label->setJustificationType (juce::Justification::centred);
    parent.addAndMakeVisible (*e->label);
    entries.push_back (std::move (e));
}

void SuppressorEditor::addToggle (const juce::String& paramId, const juce::String& text,
                                  juce::Component& parent)
{
    toggleButtons.push_back (std::make_unique<juce::ToggleButton> (text));
    auto* b = toggleButtons.back().get();
    parent.addAndMakeVisible (*b);

    auto e = std::make_unique<Entry>();
    e->id = paramId;
    e->button = b;
    e->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, paramId, *b);
    entries.push_back (std::move (e));
}

void SuppressorEditor::addCombo (const juce::String& paramId, const juce::String& text,
                                 juce::Component& parent)
{
    comboBoxes.push_back (std::make_unique<juce::ComboBox>());
    auto* c = comboBoxes.back().get();
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        c->addItemList (p->choices, 1);
    parent.addAndMakeVisible (*c);

    auto e = std::make_unique<Entry>();
    e->id = paramId;
    e->combo = c;
    e->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, paramId, *c);
    e->label = std::make_unique<juce::Label>();
    e->label->setText (text, juce::dontSendNotification);
    e->label->setJustificationType (juce::Justification::centred);
    parent.addAndMakeVisible (*e->label);
    entries.push_back (std::move (e));
}

juce::GroupComponent& SuppressorEditor::addGroup (const juce::String& title,
                                                  juce::Rectangle<int> bounds)
{
    auto g = std::make_unique<juce::GroupComponent> (juce::String(), title);
    g->setBounds (bounds);
    addAndMakeVisible (*g);
    groups.push_back (std::move (g));
    return *groups.back();
}

void SuppressorEditor::place (const juce::String& paramId, juce::Rectangle<int> b,
                              bool labelBelow)
{
    for (auto& e : entries)
        if (e->id == paramId)
        {
            if (e->slider != nullptr)
            {
                e->slider->setBounds (b.getX(), b.getY(), b.getWidth(), b.getHeight());
                if (e->label != nullptr)
                    e->label->setBounds (b.getX() - 5, b.getY() + b.getHeight(),
                                         b.getWidth() + 10, kLabelH);
            }
            else if (e->combo != nullptr)
            {
                if (e->label != nullptr && labelBelow)
                    e->label->setBounds (b.getX(), b.getY() - kLabelH - 2,
                                         b.getWidth(), kLabelH);
                e->combo->setBounds (b);
            }
            else if (e->button != nullptr)
            {
                e->button->setBounds (b);
            }
        }
}

void SuppressorEditor::resized()
{
    const int kw = kKnobW, kh = kKnobH, dx = kw + 6;

    // Core group {10,10,260,200}
    place ("strength",  { 12,           30, kw, kh });
    place ("threshold", { 12 + dx,      30, kw, kh });
    place ("release",   { 12 + 2 * dx,  30, kw, kh });

    // Gate group {280,10,300,200}
    place ("gateMode",        { 12,  60, 130, 24 });
    place ("depth",           { 12,           90, kw, kh });
    place ("hysteresis",      { 12 + dx,      90, kw, kh });
    place ("hold",            { 12 + 2 * dx,  90, kw, kh });
    place ("adaptiveRelease", { 170, 30, 150, 24 });

    // Detection group {590,10,260,200}
    place ("cue",       { 12, 90, kw, kh });
    place ("sidechain", { 140, 60, 110, 24 });
    place ("lookahead", { 140, 150, 110, 24 });

    // Hum group {10,220,260,220}
    place ("humEnable",    { 12,  30, 90, 24 });
    place ("humBase",      { 130, 30, 110, 24 });
    place ("humHarmonics", { 12,     90, kw, kh });
    place ("humStrength",  { 12 + dx, 90, kw, kh });
    humLearnButton.setBounds (12, 190, 80, 24);
    humStatus.setBounds (100, 190, 150, 24);

    // Multiband group {280,220,300,220}
    place ("bandMode", { 12, 60, 130, 24 });
    bandsLearnButton.setBounds (12, 190, 80, 24);
    bandsStatus.setBounds (100, 190, 180, 24);

    // Output group {590,220,260,220}
    place ("deltaAudition", { 12,  30, 140, 24 });
    place ("outputGain",    { 12,  90, kw, kh });
    detLabel.setBounds (120, 100, 130, 20);
    grLabel.setBounds (120, 124, 130, 20);
}

void SuppressorEditor::timerCallback()
{
    detLabel.setText ("Det: " + juce::String (proc.detectorDb(), 1) + " dB",
                      juce::dontSendNotification);
    grLabel.setText ("GR: " + juce::String (proc.gainReductionDb(), 1) + " dB",
                     juce::dontSendNotification);

    if (proc.humLearning())
        humStatus.setText ("learning...", juce::dontSendNotification);
    else if (proc.humLocked())
        humStatus.setText (juce::String (proc.humDetectedBaseHz(), 0) + " Hz, "
                           + juce::String (proc.humActiveDips()) + " dips",
                           juce::dontSendNotification);
    else
        humStatus.setText ("not learned", juce::dontSendNotification);

    if (proc.bandsLearning())
        bandsStatus.setText ("learning...", juce::dontSendNotification);
    else if (proc.bandsLocked())
        bandsStatus.setText ("thresholds locked", juce::dontSendNotification);
    else
        bandsStatus.setText ("not learned", juce::dontSendNotification);
}

void SuppressorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kText);
    g.setFont (16.0f);
    g.drawText ("suppressor", 12, 452, 200, 24, juce::Justification::left);
    g.setColour (juce::Colour (0xff6a6d75));
    g.setFont (11.0f);
    g.drawText ("zero-latency frequency-split noise suppressor", 12, 474, 400, 20,
                juce::Justification::left);
}
