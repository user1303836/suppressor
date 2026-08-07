#include <doctest/doctest.h>

#include "PluginEditor.h"
#include "PluginLookAndFeel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace
{
float relativeLuminance (juce::Colour colour)
{
    auto linearise = [] (float value)
    {
        return value <= 0.04045f ? value / 12.92f
                                 : std::pow ((value + 0.055f) / 1.055f, 2.4f);
    };

    return 0.2126f * linearise (colour.getFloatRed())
         + 0.7152f * linearise (colour.getFloatGreen())
         + 0.0722f * linearise (colour.getFloatBlue());
}

float contrastRatio (juce::Colour first, juce::Colour second)
{
    const auto lighter = std::max (relativeLuminance (first), relativeLuminance (second));
    const auto darker = std::min (relativeLuminance (first), relativeLuminance (second));
    return (lighter + 0.05f) / (darker + 0.05f);
}

int countPixelsNear (juce::Image& image, juce::Colour target)
{
    int count = 0;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto pixel = image.getPixelAt (x, y);
            const auto distance = std::abs (pixel.getFloatRed() - target.getFloatRed())
                                + std::abs (pixel.getFloatGreen() - target.getFloatGreen())
                                + std::abs (pixel.getFloatBlue() - target.getFloatBlue());
            if (distance < 0.08f)
                ++count;
        }
    return count;
}

juce::MouseEvent makeMouseEvent (juce::Component& target, juce::ModifierKeys modifiers,
                                 int clicks)
{
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), { 60.0f, 70.0f },
             modifiers, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &target, &target, now,
             { 60.0f, 70.0f }, now, clicks, false };
}

struct ParameterGestureCounter final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int, bool starting) override
    {
        starting ? ++starts : ++ends;
    }

    void reset()
    {
        starts = 0;
        ends = 0;
    }

    int starts = 0;
    int ends = 0;
};

}

TEST_CASE ("theme colours meet text and essential-control contrast targets")
{
    CHECK (contrastRatio (SuppressorTheme::primaryText, SuppressorTheme::canvas) >= 4.5f);
    CHECK (contrastRatio (SuppressorTheme::secondaryText, SuppressorTheme::surface) >= 4.5f);
    CHECK (contrastRatio (SuppressorTheme::accent, SuppressorTheme::canvas) >= 4.5f);
    CHECK (contrastRatio (SuppressorTheme::warning, SuppressorTheme::surface) >= 4.5f);
    CHECK (contrastRatio (SuppressorTheme::error, SuppressorTheme::surface) >= 4.5f);
    CHECK (contrastRatio (SuppressorTheme::controlOutline,
                          SuppressorTheme::surfaceRaised) >= 3.0f);
}

TEST_CASE ("rotary value is represented by a proportional accent arc")
{
    SuppressorLookAndFeel lookAndFeel;
    juce::Slider slider;
    slider.setLookAndFeel (&lookAndFeel);

    auto render = [&] (float position)
    {
        juce::Image image (juce::Image::RGB, 120, 120, true);
        juce::Graphics graphics (image);
        lookAndFeel.drawRotarySlider (graphics, 10, 10, 100, 100, position,
                                      juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f,
                                      slider);
        return image;
    };

    auto minimum = render (0.0f);
    auto maximum = render (1.0f);
    CHECK (countPixelsNear (maximum, SuppressorTheme::accent)
           > countPixelsNear (minimum, SuppressorTheme::accent) + 100);
}

TEST_CASE ("rotary value field is centred and leaves the complete dial draggable")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SuppressorLookAndFeel lookAndFeel;
    juce::Component parent;
    ParameterSlider slider;
    parent.setLookAndFeel (&lookAndFeel);
    parent.setBounds (0, 0, 120, 140);
    parent.addAndMakeVisible (slider);
    slider.setBounds (parent.getLocalBounds());
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 24);

    const auto layout = lookAndFeel.getSliderLayout (slider);
    CHECK (layout.sliderBounds == slider.getLocalBounds());
    CHECK (layout.textBoxBounds.getCentre() == slider.getLocalBounds().getCentre());

    auto* valueLabel = dynamic_cast<juce::Label*> (slider.getChildComponent (0));
    REQUIRE (valueLabel != nullptr);
    bool labelInterceptsClicks = true;
    bool editorInterceptsClicks = false;
    valueLabel->getInterceptsMouseClicks (labelInterceptsClicks, editorInterceptsClicks);
    CHECK_FALSE (labelInterceptsClicks);
    CHECK (editorInterceptsClicks);
    CHECK (slider.getComponentAt (layout.textBoxBounds.getCentre()) == &slider);
}

TEST_CASE ("bundled display font is available in both weights")
{
    const auto regular = SuppressorTheme::makeFont (18.0f);
    const auto semibold = SuppressorTheme::makeFont (18.0f, true);
    CHECK (regular.getTypefaceName() == "Barlow Condensed");
    CHECK (semibold.getTypefaceName() == "Barlow Condensed");
    CHECK (regular.getTypefaceStyle() != semibold.getTypefaceStyle());
}

TEST_CASE ("double-click opens exact value entry instead of resetting")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SuppressorLookAndFeel lookAndFeel;
    ParameterSlider slider;
    slider.setLookAndFeel (&lookAndFeel);
    slider.setBounds (0, 0, 120, 140);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 24);
    slider.setDoubleClickReturnValue (true, 0.5);
    slider.setValue (0.8);

    auto event = makeMouseEvent (slider,
                                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                 2);
    slider.mouseDoubleClick (event);

    auto* valueLabel = dynamic_cast<juce::Label*> (slider.getChildComponent (0));
    REQUIRE (valueLabel != nullptr);
    CHECK (valueLabel->isBeingEdited());
    CHECK (slider.getValue() == doctest::Approx (0.8));
    slider.hideTextBox (true);
}

TEST_CASE ("invalid exact-entry text preserves the current value")
{
    ParameterSlider slider;
    slider.setRange (-80.0, 0.0, 0.1);
    slider.setTextValueSuffix (" dB");
    slider.setValue (-40.0);

    CHECK (slider.getValueFromText ("not a number") == doctest::Approx (-40.0));
    CHECK (slider.getValueFromText ("--12") == doctest::Approx (-40.0));
    CHECK (slider.getValueFromText ("1e2") == doctest::Approx (-40.0));
    CHECK (slider.getValueFromText ("0x10") == doctest::Approx (-40.0));
    CHECK (slider.getValueFromText ("-12.5 garbage") == doctest::Approx (-40.0));
    CHECK (slider.getValueFromText ("-12.5 dB") == doctest::Approx (-12.5));
}

TEST_CASE ("Alt/Option-click remains the non-conflicting default reset gesture")
{
    struct DragCounter final : juce::Slider::Listener
    {
        void sliderValueChanged (juce::Slider*) override {}
        void sliderDragStarted (juce::Slider*) override { ++starts; }
        void sliderDragEnded (juce::Slider*) override { ++ends; }

        int starts = 0;
        int ends = 0;
    } counter;

    juce::ScopedJuceInitialiser_GUI gui;
    ParameterSlider slider;
    slider.addListener (&counter);
    slider.setBounds (0, 0, 120, 140);
    slider.setRange (0.0, 1.0, 0.01);
    slider.setDoubleClickReturnValue (true, 0.25, juce::ModifierKeys::altModifier);
    slider.setValue (0.8);

    auto event = makeMouseEvent (
        slider,
        juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                            | juce::ModifierKeys::altModifier),
        1);
    slider.mouseDown (event);
    CHECK (slider.getValue() == doctest::Approx (0.25));
    CHECK (counter.starts == 1);
    CHECK (counter.ends == 1);

    auto dragEvent = makeMouseEvent (
        slider, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1);
    slider.mouseDown (dragEvent);
    CHECK (counter.starts == 2);
    CHECK (counter.ends == 1);
    slider.endActiveGesture();
    CHECK (counter.ends == 2);
    CHECK (slider.getThumbBeingDragged() == -1);
}


TEST_CASE ("editor integrates every parameter with truthful state and balanced gestures")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SuppressorProcessor processor;
    auto editor = std::make_unique<SuppressorEditor> (processor);

    const juce::StringArray parameterIds {
        "strength", "threshold", "release", "gateMode", "depth", "hysteresis", "hold",
        "adaptiveRelease", "cue", "sidechain", "lookahead", "humEnable", "humBase",
        "humHarmonics", "humStrength", "humLearn", "bandMode", "bandsLearn",
        "deltaAudition", "outputGain"
    };

    CHECK (editor->getWidth() == 1000);
    CHECK (editor->getHeight() == 640);
    CHECK_FALSE (editor->isResizable());
    REQUIRE (editor->getNumChildComponents() > 0);
    CHECK (editor->getChildComponent (0)->getName() == "Suppression activity");
    CHECK (editor->getChildComponent (0)->getExplicitFocusOrder() == 1);
    CHECK (processor.getParameters().size() == parameterIds.size());

    std::set<int> focusOrders;
    std::set<std::string> accessibleNames;
    for (const auto& id : parameterIds)
    {
        CAPTURE (id);
        auto* component = editor->findChildWithID (id);
        auto* parameter = processor.apvts.getParameter (id);
        REQUIRE (component != nullptr);
        REQUIRE (parameter != nullptr);

        const auto parameterIndex = processor.getParameters().indexOf (parameter);
        CHECK (editor->getControlParameterIndex (*component) == parameterIndex);
        CHECK (component->isVisible());
        CHECK (component->getWidth() >= 32);
        CHECK (component->getHeight() >= 32);
        CHECK (component->getWantsKeyboardFocus());
        CHECK (component->getExplicitFocusOrder() > 0);
        CHECK (focusOrders.insert (component->getExplicitFocusOrder()).second);
        CHECK (component->getName().isNotEmpty());
        CHECK (accessibleNames.insert (component->getName().toStdString()).second);

        if (auto* slider = dynamic_cast<ParameterSlider*> (component))
        {
            CHECK (slider->getTextFromValue (slider->getValue()).isNotEmpty());
            REQUIRE (slider->getNumChildComponents() > 0);
            CHECK (editor->getControlParameterIndex (*slider->getChildComponent (0))
                   == parameterIndex);
        }
    }
    CHECK (focusOrders.size() == 20);
    CHECK (accessibleNames.size() == 20);

    auto* strength = dynamic_cast<ParameterSlider*> (editor->findChildWithID ("strength"));
    auto* strengthParameter = processor.apvts.getParameter ("strength");
    REQUIRE (strength != nullptr);
    REQUIRE (strengthParameter != nullptr);
    CHECK (strength->getValueFromText ("50 %") == doctest::Approx (0.5));
    CHECK (strength->getValueFromText ("50 garbage")
           == doctest::Approx (strength->getValue()));
    CHECK (strength->getValueFromText ("1e2")
           == doctest::Approx (strength->getValue()));

    ParameterGestureCounter gestures;
    strengthParameter->addListener (&gestures);
    strength->setValue (0.2, juce::sendNotificationSync);
    gestures.reset();
    CHECK (strength->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK (gestures.starts == 1);
    CHECK (gestures.ends == 1);

    strength->setValue (0.2, juce::sendNotificationSync);
    gestures.reset();
    auto resetEvent = makeMouseEvent (
        *strength,
        juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                            | juce::ModifierKeys::altModifier),
        1);
    strength->mouseDown (resetEvent);
    CHECK (strength->getValue() == doctest::Approx (0.9));
    CHECK (gestures.starts == 1);
    CHECK (gestures.ends == 1);

    auto* adaptive = dynamic_cast<ParameterToggleButton*> (
        editor->findChildWithID ("adaptiveRelease"));
    auto* adaptiveParameter = processor.apvts.getParameter ("adaptiveRelease");
    REQUIRE (adaptive != nullptr);
    REQUIRE (adaptiveParameter != nullptr);
    ParameterGestureCounter buttonGestures;
    adaptiveParameter->addListener (&buttonGestures);
    CHECK (adaptive->keyPressed (juce::KeyPress (' ')));
    CHECK (adaptive->getToggleState());
    CHECK (buttonGestures.starts == 1);
    CHECK (buttonGestures.ends == 1);
    adaptiveParameter->removeListener (&buttonGestures);

    auto* bandMode = dynamic_cast<juce::ComboBox*> (editor->findChildWithID ("bandMode"));
    auto* bandsLearn = dynamic_cast<ParameterTextButton*> (
        editor->findChildWithID ("bandsLearn"));
    auto* bandsLearnParameter = processor.apvts.getParameter ("bandsLearn");
    REQUIRE (bandMode != nullptr);
    REQUIRE (bandsLearn != nullptr);
    REQUIRE (bandsLearnParameter != nullptr);
    ParameterGestureCounter learnGestures;
    bandsLearnParameter->addListener (&learnGestures);
    bandMode->setSelectedItemIndex (1, juce::sendNotificationSync);
    CHECK (bandsLearn->keyPressed (juce::KeyPress (' ')));
    CHECK (bandsLearn->getToggleState());
    CHECK (bandsLearnParameter->getValue() == doctest::Approx (1.0f));
    CHECK (learnGestures.starts == 1);
    CHECK (learnGestures.ends == 1);
    bandsLearnParameter->removeListener (&learnGestures);
    bandMode->setSelectedItemIndex (2, juce::sendNotificationSync);

    juce::Component* activity = nullptr;
    juce::Label* bandsStatus = nullptr;
    for (auto* child : editor->getChildren())
    {
        if (child->getName() == "Suppression activity")
            activity = child;
        if (child->getName() == "Multiband learning status")
            bandsStatus = dynamic_cast<juce::Label*> (child);
    }
    REQUIRE (activity != nullptr);
    REQUIRE (bandsStatus != nullptr);
    CHECK (activity->getDescription().containsIgnoreCase ("learn armed"));
    CHECK (activity->getDescription().containsIgnoreCase ("gain reduction unavailable"));
    CHECK (activity->getDescription().containsIgnoreCase ("passes while processing"));
    CHECK (bandsStatus->getText().containsIgnoreCase ("armed"));
    CHECK_FALSE (bandsStatus->getText().containsIgnoreCase ("capturing"));

    auto* delta = dynamic_cast<juce::Button*> (editor->findChildWithID ("deltaAudition"));
    REQUIRE (delta != nullptr);
    delta->setToggleState (true, juce::sendNotificationSync);
    bandMode->setSelectedItemIndex (1, juce::sendNotificationSync);
    CHECK (activity->getDescription().containsIgnoreCase ("output: removed signal"));

    gestures.reset();
    {
        auto dragEvent = makeMouseEvent (
            *strength, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1);
        strength->mouseDown (dragEvent);
    }
    CHECK (gestures.starts == 1);
    CHECK (gestures.ends == 0);
    editor.reset();
    CHECK (gestures.ends == 1);
    strengthParameter->removeListener (&gestures);
}

TEST_CASE ("declared host scale factors preserve logical layout and render cleanly")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const std::array scales { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

    for (const auto scale : scales)
    {
        CAPTURE (scale);
        SuppressorProcessor processor;
        auto editor = std::make_unique<SuppressorEditor> (processor);
        const auto width = juce::roundToInt (1000.0f * scale);
        const auto height = juce::roundToInt (640.0f * scale);
        editor->setVisible (true);
        editor->setScaleFactor (scale);

        CHECK (editor->getWidth() == 1000);
        CHECK (editor->getHeight() == 640);
        const auto transformedBounds = editor->getLocalBounds().toFloat().transformedBy (
            editor->getTransform());
        CHECK (transformedBounds.getWidth() == doctest::Approx (width));
        CHECK (transformedBounds.getHeight() == doctest::Approx (height));

        juce::Image image (juce::Image::ARGB, width, height, true);
        {
            juce::Graphics graphics (image);
            graphics.addTransform (juce::AffineTransform::scale (scale));
            editor->paintEntireComponent (graphics, true);
        }
        CHECK (image.getPixelAt (width - 2, height - 2).getAlpha() == 0xff);
        CHECK (countPixelsNear (image, SuppressorTheme::accent)
               > juce::roundToInt (100.0f * scale * scale));

        const auto captureDirectory = juce::SystemStats::getEnvironmentVariable (
            "SUPPRESSOR_UI_CAPTURE_DIR", {});
        auto captureSucceeded = true;
        if (captureDirectory.isNotEmpty())
        {
            const auto directory = juce::File (captureDirectory);
            captureSucceeded = directory.createDirectory().wasOk();
            auto output = directory.getChildFile (
                "suppressor-ui-" + juce::String (juce::roundToInt (scale * 100.0f))
                + ".png");
            juce::PNGImageFormat format;
            auto stream = output.createOutputStream();
            captureSucceeded = captureSucceeded && stream != nullptr;
            if (stream != nullptr)
                captureSucceeded = captureSucceeded
                                   && format.writeImageToStream (image, *stream);
        }
        CHECK (captureSucceeded);
    }
}
