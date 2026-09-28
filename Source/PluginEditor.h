#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Custom LookAndFeel implementing the Industrial Dot-Matrix / Nothing OS / Teenage Engineering aesthetic
class IndustrialDotMatrixLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IndustrialDotMatrixLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFF3B30)); // Red accent
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFFB8BBC2));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFF3B30));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFF14161A));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
        setColour(juce::Label::textColourId, juce::Colour(0xFF14161A));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height)).reduced(5.0f);

        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto centre = bounds.getCentre();

        // 1. Off-white Outer Dial Disc
        auto outerRadius = radius - 3.0f;
        g.setColour(juce::Colour(0xFFEDEDF0));
        g.fillEllipse(centre.x - outerRadius, centre.y - outerRadius, outerRadius * 2.0f, outerRadius * 2.0f);

        // Thin outer perimeter border
        g.setColour(juce::Colour(0xFFD0D3D9));
        g.drawEllipse(centre.x - outerRadius, centre.y - outerRadius, outerRadius * 2.0f, outerRadius * 2.0f, 1.0f);

        // 2. Radial Tick Marks around the perimeter
        const int numTicks = 24;
        for (int i = 0; i < numTicks; ++i)
        {
            const float tickAngle = rotaryStartAngle + (static_cast<float>(i) / static_cast<float>(numTicks - 1)) * (rotaryEndAngle - rotaryStartAngle);
            const auto pOuter = centre.getPointOnCircumference(outerRadius - 2.0f, tickAngle);
            const auto pInner = centre.getPointOnCircumference(outerRadius - (i % 6 == 0 ? 7.0f : 4.5f), tickAngle);

            g.setColour(i % 6 == 0 ? juce::Colour(0xFF1A1C20) : juce::Colour(0xFFA0A4AC));
            g.drawLine(pInner.x, pInner.y, pOuter.x, pOuter.y, i % 6 == 0 ? 1.4f : 0.9f);
        }

        // 3. Red Accent Indicator Dot at current angle
        const auto redDotPos = centre.getPointOnCircumference(outerRadius - 5.0f, toAngle);
        g.setColour(juce::Colour(0xFFFF3B30));
        g.fillEllipse(redDotPos.x - 2.5f, redDotPos.y - 2.5f, 5.0f, 5.0f);

        // 4. Center Rotor Hub (Industrial 3-Spoke Black Aperture)
        auto innerRadius = outerRadius * 0.46f;
        g.setColour(juce::Colour(0xFF14161A));
        g.fillEllipse(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

        const float spokeRadius = innerRadius * 0.58f;
        g.setColour(juce::Colour(0xFFEDEDF0));
        for (int i = 0; i < 3; ++i)
        {
            const float spokeAngle = toAngle + i * (juce::MathConstants<float>::twoPi / 3.0f);
            const auto spokePt = centre.getPointOnCircumference(spokeRadius, spokeAngle);
            g.fillEllipse(spokePt.x - 2.5f, spokePt.y - 2.5f, 5.0f, 5.0f);
        }

        g.fillEllipse(centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
    }
};

class SubdrumProcessorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            public juce::Timer,
                                            public juce::KeyListener
{
public:
    explicit SubdrumProcessorAudioProcessorEditor(SubdrumProcessorAudioProcessor&);
    ~SubdrumProcessorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent) override;

    void triggerDrumVoice(int noteNumber);
    void releaseDrumVoice(int noteNumber);

private:
    struct RotaryControl
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText);
    void setupPad(juce::TextButton& button, const juce::String& text, int note, juce::Colour baseColor, juce::Colour textCol);

    SubdrumProcessorAudioProcessor& audioProcessor;
    IndustrialDotMatrixLookAndFeel industrialLookAndFeel;

    // Visualizer Bars Animation State
    static constexpr int numVisualizerCols = 32;
    std::array<float, numVisualizerCols> visualizerBarHeights {};
    float liveVisualizerPeak { 0.0f };

    // DSP Controls (Row 1)
    RotaryControl driveKnob;
    RotaryControl tapeMixKnob;
    RotaryControl cutoffKnob;
    RotaryControl resonanceKnob;

    // DSP Controls (Row 2)
    RotaryControl compThreshKnob;
    RotaryControl compAttackKnob;
    RotaryControl compReleaseKnob;
    RotaryControl compMakeupKnob;
    RotaryControl vinylNoiseKnob;
    RotaryControl outputGainKnob;

    // Ableton Drum Rack Styled Pads (8 Pads)
    juce::TextButton kickPad;
    juce::TextButton rimPad;
    juce::TextButton snarePad;
    juce::TextButton clapPad;
    juce::TextButton closedHatPad;
    juce::TextButton openHatPad;
    juce::TextButton lowTomPad;
    juce::TextButton subBassPad;

    // Virtual MIDI Keyboard Component
    juce::MidiKeyboardComponent keyboardComponent;

    // Current active octave offset for Ableton-style Z/X octave transpose
    int octaveOffset { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubdrumProcessorAudioProcessorEditor)
};
