#include "PluginLookAndFeel.h"
#include "SuppressorAssets.h"

#include <cmath>

namespace
{
const juce::Typeface::Ptr& regularTypeface()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor (
        SuppressorAssets::BarlowCondensedRegular_ttf,
        SuppressorAssets::BarlowCondensedRegular_ttfSize);
    return typeface;
}

const juce::Typeface::Ptr& semiboldTypeface()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor (
        SuppressorAssets::BarlowCondensedSemiBold_ttf,
        SuppressorAssets::BarlowCondensedSemiBold_ttfSize);
    return typeface;
}

juce::TextLayout makeTooltipLayout (const juce::String& text, juce::Colour colour,
                                    float maximumWidth)
{
    juce::AttributedString attributed;
    attributed.setJustification (juce::Justification::centred);
    attributed.setWordWrap (juce::AttributedString::byWord);
    attributed.append (text, SuppressorTheme::makeFont (14.0f), colour);
    juce::TextLayout layout;
    layout.createLayout (attributed, maximumWidth);
    return layout;
}
}

juce::Font SuppressorTheme::makeFont (float size, bool semibold, float tracking)
{
    auto typeface = semibold ? semiboldTypeface() : regularTypeface();
    return juce::Font (juce::FontOptions (typeface)
                           .withHeight (size)
                           .withKerningFactor (tracking));
}

void ParameterSlider::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (isEnabled() && isTextBoxEditable() && ! event.mods.isPopupMenu())
        showTextBox();
}

bool ParameterSlider::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey && isEnabled() && isTextBoxEditable())
    {
        showTextBox();
        return true;
    }

    const auto isAdjustmentKey = ! key.getModifiers().isAnyModifierKeyDown()
                              && (key.isKeyCode (juce::KeyPress::leftKey)
                                  || key.isKeyCode (juce::KeyPress::rightKey)
                                  || key.isKeyCode (juce::KeyPress::upKey)
                                  || key.isKeyCode (juce::KeyPress::downKey));
    if (! isEnabled() || ! isAdjustmentKey || getThumbBeingDragged() >= 0)
        return juce::Slider::keyPressed (key);

    if (beginKeyboardGesture != nullptr)
        beginKeyboardGesture();
    const auto handled = juce::Slider::keyPressed (key);
    if (endKeyboardGesture != nullptr)
        endKeyboardGesture();
    return handled;
}

void ParameterSlider::setKeyboardGestureCallbacks (GestureCallback begin,
                                                    GestureCallback end)
{
    beginKeyboardGesture = std::move (begin);
    endKeyboardGesture = std::move (end);
}

void ParameterSlider::endActiveGesture()
{
    if (getThumbBeingDragged() < 0)
        return;

    const auto now = juce::Time::getCurrentTime();
    const auto position = getLocalBounds().getCentre().toFloat();
    juce::Slider::mouseUp (
        { juce::Desktop::getInstance().getMainMouseSource(), position, {},
          1.0f, 0.0f, 0.0f, 0.0f, 0.0f, this, this, now, position, now, 1, true });
}

double ParameterSlider::getValueFromText (const juce::String& text)
{
    auto numericText = text.trim();
    const auto suffix = getTextValueSuffix().trim();
    if (suffix.isNotEmpty() && numericText.endsWithIgnoreCase (suffix))
        numericText = numericText.dropLastCharacters (suffix.length()).trimEnd();

    bool hasDigit = false;
    bool hasDecimalPoint = false;
    for (int index = 0; index < numericText.length(); ++index)
    {
        const auto character = numericText[index];
        if (character >= '0' && character <= '9')
        {
            hasDigit = true;
            continue;
        }
        if (character == '.' && ! hasDecimalPoint)
        {
            hasDecimalPoint = true;
            continue;
        }
        if (index == 0 && (character == '+' || character == '-'))
            continue;
        return getValue();
    }

    if (! hasDigit)
        return getValue();

    const auto parsed = numericText.getDoubleValue();
    return std::isfinite (parsed) ? juce::Slider::getValueFromText (numericText)
                                  : getValue();
}

bool ParameterToggleButton::keyPressed (const juce::KeyPress& key)
{
    if (isEnabled() && key.isKeyCode (' '))
    {
        setToggleState (! getToggleState(), juce::sendNotificationSync);
        return true;
    }
    return juce::ToggleButton::keyPressed (key);
}

bool ParameterTextButton::keyPressed (const juce::KeyPress& key)
{
    if (isEnabled() && key.isKeyCode (' '))
    {
        setToggleState (! getToggleState(), juce::sendNotificationSync);
        return true;
    }
    return juce::TextButton::keyPressed (key);
}

SuppressorLookAndFeel::SuppressorLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, SuppressorTheme::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, SuppressorTheme::controlOutline);
    setColour (juce::Slider::textBoxTextColourId, SuppressorTheme::primaryText);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, SuppressorTheme::accent.withAlpha (0.28f));
    setColour (juce::ComboBox::textColourId, SuppressorTheme::primaryText);
    setColour (juce::ComboBox::backgroundColourId, SuppressorTheme::surfaceRaised);
    setColour (juce::ComboBox::outlineColourId, SuppressorTheme::controlOutline);
    setColour (juce::ComboBox::arrowColourId, SuppressorTheme::secondaryText);
    setColour (juce::ComboBox::focusedOutlineColourId, SuppressorTheme::accent);
    setColour (juce::PopupMenu::backgroundColourId, SuppressorTheme::surfaceRaised);
    setColour (juce::PopupMenu::textColourId, SuppressorTheme::primaryText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, SuppressorTheme::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, SuppressorTheme::canvas);
    setColour (juce::Label::textColourId, SuppressorTheme::primaryText);
    setColour (juce::Label::textWhenEditingColourId, SuppressorTheme::primaryText);
    setColour (juce::Label::backgroundWhenEditingColourId, SuppressorTheme::surfaceRaised);
    setColour (juce::Label::outlineWhenEditingColourId, SuppressorTheme::accent);
    setColour (juce::TextEditor::textColourId, SuppressorTheme::primaryText);
    setColour (juce::TextEditor::backgroundColourId, SuppressorTheme::surfaceRaised);
    setColour (juce::TextEditor::highlightColourId, SuppressorTheme::accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, SuppressorTheme::primaryText);
    setColour (juce::CaretComponent::caretColourId, SuppressorTheme::accent);
    setColour (juce::TooltipWindow::backgroundColourId, SuppressorTheme::surfaceRaised);
    setColour (juce::TooltipWindow::textColourId, SuppressorTheme::primaryText);
    setColour (juce::TooltipWindow::outlineColourId, SuppressorTheme::controlOutline);
}

void SuppressorLookAndFeel::drawRotarySlider (juce::Graphics& graphics, int x, int y,
                                               int width, int height, float sliderPosition,
                                               float startAngle, float endAngle,
                                               juce::Slider& slider)
{
    const auto size = static_cast<float> (juce::jmin (width, height));
    const auto radius = size * 0.5f - 8.0f;
    const auto centre = juce::Point<float> (
        static_cast<float> (x) + static_cast<float> (width) * 0.5f,
        static_cast<float> (y) + static_cast<float> (height) * 0.5f);
    const auto angle = juce::jmap (sliderPosition, 0.0f, 1.0f, startAngle, endAngle);
    const auto alpha = slider.isEnabled() ? 1.0f : 0.38f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         startAngle, endAngle, true);
    graphics.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId)
                            .withMultipliedAlpha (alpha));
    graphics.strokePath (track, juce::PathStrokeType (4.0f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

    if (sliderPosition > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                             startAngle, angle, true);
        graphics.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId)
                                .withMultipliedAlpha (alpha));
        graphics.strokePath (value, juce::PathStrokeType (4.0f,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
    }

    const auto innerRadius = radius - 10.0f;
    graphics.setColour (SuppressorTheme::surfaceRaised.withMultipliedAlpha (alpha));
    graphics.fillEllipse (centre.x - innerRadius, centre.y - innerRadius,
                          innerRadius * 2.0f, innerRadius * 2.0f);

    auto innerOutline = slider.isMouseOverOrDragging()
                            ? SuppressorTheme::secondaryText
                            : SuppressorTheme::divider;
    graphics.setColour (innerOutline.withMultipliedAlpha (alpha));
    graphics.drawEllipse (centre.x - innerRadius, centre.y - innerRadius,
                          innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

    const auto markerStart = centre.getPointOnCircumference (innerRadius - 9.0f, angle);
    const auto markerEnd = centre.getPointOnCircumference (innerRadius - 3.0f, angle);
    graphics.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId)
                            .withMultipliedAlpha (alpha));
    graphics.drawLine ({ markerStart, markerEnd }, 2.5f);

    if (slider.hasKeyboardFocus (true))
    {
        graphics.setColour (SuppressorTheme::accent);
        graphics.drawEllipse (centre.x - radius - 4.0f, centre.y - radius - 4.0f,
                              (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f, 2.0f);
    }
}

juce::Label* SuppressorLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (SuppressorTheme::makeFont (15.0f, true));
    label->setJustificationType (juce::Justification::centred);
    label->onEditorShow = [label]
    {
        if (auto* editor = label->getCurrentTextEditor())
        {
            editor->setFont (label->getFont());
            editor->setJustification (juce::Justification::centred);
        }
    };
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineWhenEditingColourId, juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    label->setInterceptsMouseClicks (false, true);
    return label;
}

juce::Slider::SliderLayout SuppressorLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;
    const auto bounds = slider.getLocalBounds();
    layout.sliderBounds = bounds;

    if (slider.getTextBoxPosition() != juce::Slider::NoTextBox)
    {
        const auto width = juce::jmin (slider.getTextBoxWidth(), bounds.getWidth() - 4);
        const auto height = juce::jmin (slider.getTextBoxHeight(), bounds.getHeight() - 4);
        layout.textBoxBounds = juce::Rectangle<int> (width, height).withCentre (bounds.getCentre());
    }

    return layout;
}

void SuppressorLookAndFeel::drawComboBox (juce::Graphics& graphics, int width, int height,
                                           bool isButtonDown, int, int, int, int,
                                           juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.5f, 0.5f,
                                           static_cast<float> (width) - 1.0f,
                                           static_cast<float> (height) - 1.0f);
    graphics.setColour (SuppressorTheme::surfaceRaised);
    graphics.fillRoundedRectangle (bounds, 6.0f);

    const auto focused = box.hasKeyboardFocus (false);
    const auto outline = focused || isButtonDown
                             ? SuppressorTheme::accent
                             : (box.isMouseOver() ? SuppressorTheme::secondaryText
                                                  : SuppressorTheme::controlOutline);
    graphics.setColour (outline);
    graphics.drawRoundedRectangle (bounds, 6.0f, focused ? 2.0f : 1.0f);

    const auto arrowCentreX = static_cast<float> (width - 17);
    const auto arrowCentreY = static_cast<float> (height) * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (arrowCentreX - 4.0f, arrowCentreY - 2.0f);
    arrow.lineTo (arrowCentreX, arrowCentreY + 2.0f);
    arrow.lineTo (arrowCentreX + 4.0f, arrowCentreY - 2.0f);
    graphics.setColour (focused ? SuppressorTheme::accent : SuppressorTheme::secondaryText);
    graphics.strokePath (arrow, juce::PathStrokeType (1.7f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
}

juce::Font SuppressorLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return SuppressorTheme::makeFont (15.0f);
}

juce::Font SuppressorLookAndFeel::getPopupMenuFont()
{
    return SuppressorTheme::makeFont (15.0f);
}

juce::Rectangle<int> SuppressorLookAndFeel::getTooltipBounds (
    const juce::String& text, juce::Point<int> screenPosition,
    juce::Rectangle<int> parentArea)
{
    const auto layout = makeTooltipLayout (text, SuppressorTheme::primaryText, 300.0f);
    const auto width = juce::roundToInt (std::ceil (layout.getWidth())) + 16;
    const auto height = juce::roundToInt (std::ceil (layout.getHeight())) + 10;
    return juce::Rectangle<int> (
               screenPosition.x > parentArea.getCentreX() ? screenPosition.x - width - 12
                                                          : screenPosition.x + 24,
               screenPosition.y > parentArea.getCentreY() ? screenPosition.y - height - 6
                                                          : screenPosition.y + 6,
               width, height)
        .constrainedWithin (parentArea);
}

void SuppressorLookAndFeel::drawTooltip (juce::Graphics& graphics,
                                         const juce::String& text,
                                         int width, int height)
{
    const auto bounds = juce::Rectangle<int> (width, height).toFloat();
    graphics.setColour (SuppressorTheme::surfaceRaised);
    graphics.fillRoundedRectangle (bounds, 5.0f);
    graphics.setColour (SuppressorTheme::controlOutline);
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    const auto textBounds = bounds.reduced (8.0f, 5.0f);
    auto layout = makeTooltipLayout (text, SuppressorTheme::primaryText,
                                     textBounds.getWidth());
    layout.draw (graphics, textBounds);
}

void SuppressorLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 1, box.getWidth() - 38, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void SuppressorLookAndFeel::drawToggleButton (juce::Graphics& graphics,
                                               juce::ToggleButton& button,
                                               bool isMouseOver, bool isButtonDown)
{
    const auto alpha = button.isEnabled() ? 1.0f : 0.42f;
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = SuppressorTheme::surfaceRaised;
    if (isButtonDown)
        fill = fill.brighter (0.08f);
    graphics.setColour (fill.withMultipliedAlpha (alpha));
    graphics.fillRoundedRectangle (bounds, 6.0f);

    const auto focused = button.hasKeyboardFocus (false);
    const auto outline = focused ? SuppressorTheme::accent
                                 : (isMouseOver ? SuppressorTheme::secondaryText
                                                : SuppressorTheme::controlOutline);
    graphics.setColour (outline.withMultipliedAlpha (alpha));
    graphics.drawRoundedRectangle (bounds, 6.0f, focused ? 2.0f : 1.0f);

    auto indicator = juce::Rectangle<float> (12.0f,
                                              static_cast<float> (button.getHeight()) * 0.5f - 8.0f,
                                              16.0f, 16.0f);
    graphics.setColour ((button.getToggleState() ? SuppressorTheme::accent
                                                  : SuppressorTheme::surface)
                            .withMultipliedAlpha (alpha));
    graphics.fillRoundedRectangle (indicator, 3.0f);
    graphics.setColour ((button.getToggleState() ? SuppressorTheme::accent
                                                  : SuppressorTheme::controlOutline)
                            .withMultipliedAlpha (alpha));
    graphics.drawRoundedRectangle (indicator, 3.0f, 1.0f);

    if (button.getToggleState())
    {
        juce::Path check;
        check.startNewSubPath (indicator.getX() + 3.5f, indicator.getCentreY());
        check.lineTo (indicator.getX() + 7.0f, indicator.getBottom() - 4.0f);
        check.lineTo (indicator.getRight() - 3.0f, indicator.getY() + 4.0f);
        graphics.setColour (SuppressorTheme::canvas.withMultipliedAlpha (alpha));
        graphics.strokePath (check, juce::PathStrokeType (2.0f,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
    }

    graphics.setColour (SuppressorTheme::primaryText.withMultipliedAlpha (alpha));
    graphics.setFont (SuppressorTheme::makeFont (14.5f, true, 0.02f));
    graphics.drawFittedText (button.getButtonText(), 38, 0, button.getWidth() - 48,
                             button.getHeight(), juce::Justification::centredLeft, 1);
}

void SuppressorLookAndFeel::drawButtonBackground (juce::Graphics& graphics,
                                                   juce::Button& button,
                                                   const juce::Colour&,
                                                   bool isMouseOver, bool isButtonDown)
{
    const auto alpha = button.isEnabled() ? 1.0f : 0.42f;
    const auto active = button.getToggleState();
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = active ? SuppressorTheme::accent : SuppressorTheme::surfaceRaised;
    if (isButtonDown)
        fill = fill.brighter (active ? 0.04f : 0.08f);
    graphics.setColour (fill.withMultipliedAlpha (alpha));
    graphics.fillRoundedRectangle (bounds, 6.0f);

    const auto focused = button.hasKeyboardFocus (false);
    const auto outline = active ? SuppressorTheme::accent
                                : (isMouseOver ? SuppressorTheme::secondaryText
                                               : SuppressorTheme::controlOutline);
    graphics.setColour ((focused ? (active ? SuppressorTheme::primaryText
                                           : SuppressorTheme::accent)
                                 : outline)
                            .withMultipliedAlpha (alpha));
    graphics.drawRoundedRectangle (bounds, 6.0f, focused ? 2.0f : 1.0f);
    if (focused && active)
    {
        graphics.setColour (SuppressorTheme::canvas.withMultipliedAlpha (alpha));
        graphics.drawRoundedRectangle (bounds.reduced (3.0f), 4.0f, 1.5f);
    }
}

void SuppressorLookAndFeel::drawButtonText (juce::Graphics& graphics,
                                             juce::TextButton& button,
                                             bool, bool)
{
    const auto alpha = button.isEnabled() ? 1.0f : 0.42f;
    graphics.setColour ((button.getToggleState() ? SuppressorTheme::canvas
                                                  : SuppressorTheme::primaryText)
                            .withMultipliedAlpha (alpha));
    graphics.setFont (getTextButtonFont (button, button.getHeight()));
    graphics.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (8, 2),
                             juce::Justification::centred, 1);
}

juce::Font SuppressorLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return SuppressorTheme::makeFont (juce::jmin (15.0f, static_cast<float> (buttonHeight) * 0.5f),
                                      true, 0.04f);
}
