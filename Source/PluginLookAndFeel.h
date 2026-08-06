#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <utility>

namespace SuppressorTheme
{
inline const juce::Colour canvas { 0xff0a0e14 };
inline const juce::Colour header { 0xff0d141e };
inline const juce::Colour surface { 0xff111a24 };
inline const juce::Colour surfaceRaised { 0xff182432 };
inline const juce::Colour divider { 0xff2a3a4b };
inline const juce::Colour controlOutline { 0xff66798c };
inline const juce::Colour primaryText { 0xfff1f5f8 };
inline const juce::Colour secondaryText { 0xffa6b2be };
inline const juce::Colour accent { 0xff8ea7ff };
inline const juce::Colour warning { 0xfff2b84b };
inline const juce::Colour error { 0xffff6f7d };
inline const juce::Colour inactiveControl { 0xff6c7c8f };
inline constexpr float cornerRadius = 9.0f;

juce::Font makeFont (float size, bool semibold = false, float tracking = 0.0f);
}

class ParameterSlider final : public juce::Slider
{
public:
    using GestureCallback = std::function<void()>;

    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    double getValueFromText (const juce::String&) override;
    void setKeyboardGestureCallbacks (GestureCallback begin, GestureCallback end);
    void endActiveGesture();

private:
    GestureCallback beginKeyboardGesture;
    GestureCallback endKeyboardGesture;
};

class ParameterToggleButton final : public juce::ToggleButton
{
public:
    using juce::ToggleButton::ToggleButton;
    bool keyPressed (const juce::KeyPress&) override;
};

class ParameterTextButton final : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    bool keyPressed (const juce::KeyPress&) override;
};

class SuppressorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SuppressorLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosition, float startAngle, float endAngle,
                           juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonWidth, int buttonHeight,
                       juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>,
                                            juce::Rectangle<int>) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isMouseOver,
                           bool isButtonDown) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOver, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOver,
                         bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
