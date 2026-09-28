#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Custom LookAndFeel for sleek underground lo-fi aesthetic
class SubdrumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SubdrumLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFF7A00));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF262C33));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFFA23A));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFEDEDED));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
        setColour(juce::Label::textColourId, juce::Colour(0xFFB0B8C0));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height)).reduced(6.0f);

        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto centre = bounds.getCentre();

        // Background Track
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centre.x, centre.y, radius - 4.0f, radius - 4.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
        g.strokePath(backgroundArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Filled Value Arc
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, radius - 4.0f, radius - 4.0f, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(valueArc, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Dial Body
        auto dialRadius = radius - 10.0f;
        juce::ColourGradient dialGrad(juce::Colour(0xFF22272E), centre.x - dialRadius, centre.y - dialRadius,
                                     juce::Colour(0xFF161A1D), centre.x + dialRadius, centre.y + dialRadius, false);
        g.setGradientFill(dialGrad);
        g.fillEllipse(centre.x - dialRadius, centre.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

        g.setColour(juce::Colour(0xFF323842));
        g.drawEllipse(centre.x - dialRadius, centre.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f, 1.2f);

        // Pointer Needle
        juce::Path needle;
        auto needleLength = dialRadius * 0.75f;
        needle.addLineSegment(juce::Line<float>(centre, centre.getPointOnCircumference(needleLength, toAngle)), 2.5f);
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.strokePath(needle, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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

    // KeyListener callbacks (Captures all keyboard events globally)
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

    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText,
                      const juce::String& suffix = "");

    void setupPad(juce::TextButton& button, const juce::String& text, int note);

    SubdrumProcessorAudioProcessor& audioProcessor;
    SubdrumLookAndFeel customLookAndFeel;

    // Controls
    RotaryControl driveKnob;
    RotaryControl tapeMixKnob;

    RotaryControl cutoffKnob;
    RotaryControl resonanceKnob;

    RotaryControl compThreshKnob;
    RotaryControl compRatioKnob;
    RotaryControl compAttackKnob;
    RotaryControl compReleaseKnob;
    RotaryControl compMakeupKnob;
    RotaryControl compMixKnob;

    RotaryControl vinylNoiseKnob;
    RotaryControl vinylDustKnob;

    RotaryControl outputGainKnob;

    // Interactive Drum Pads
    juce::TextButton kickPad;
    juce::TextButton snarePad;
    juce::TextButton closedHatPad;
    juce::TextButton openHatPad;
    juce::TextButton subBassPad;

    // Virtual MIDI Keyboard Component
    juce::MidiKeyboardComponent keyboardComponent;

    float currentMeterGainReduction { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubdrumProcessorAudioProcessorEditor)
};
