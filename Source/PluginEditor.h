#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Custom LookAndFeel replicating the minimalist aesthetic from the reference
class MinimalistSubdrumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MinimalistSubdrumLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFE5838B)); // Coral / Rose Pink
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF26282F)); // Dark Charcoal Track
        setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE5838B));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFD4D8E2));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
        setColour(juce::Label::textColourId, juce::Colour(0xFFA6ABB6));
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

        // 1. Dark Base Circle
        auto baseRadius = radius - 3.0f;
        g.setColour(juce::Colour(0xFF191B20));
        g.fillEllipse(centre.x - baseRadius, centre.y - baseRadius, baseRadius * 2.0f, baseRadius * 2.0f);

        // 2. Background Track Arc (thin dark circle)
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
        g.strokePath(backgroundArc, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Active Rose / Coral Value Arc
        if (sliderPosProportional > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
            g.strokePath(valueArc, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 4. Concentric Inner Circle / Ring (as seen in reference design)
        auto innerRadius = radius * 0.52f;
        g.setColour(juce::Colour(0xFFD4D8E2).withAlpha(0.85f));
        g.drawEllipse(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.4f);

        // 5. Subtle Center Dot
        g.setColour(juce::Colour(0xFFD4D8E2).withAlpha(0.9f));
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
    void setupPad(juce::TextButton& button, const juce::String& text, int note);

    SubdrumProcessorAudioProcessor& audioProcessor;
    MinimalistSubdrumLookAndFeel minimalistLookAndFeel;

    // Visualizer Bars Animation State
    static constexpr int numVisualizerBars = 36;
    std::array<float, numVisualizerBars> visualizerBarHeights {};
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

    // Interactive Drum Pads
    juce::TextButton kickPad;
    juce::TextButton snarePad;
    juce::TextButton closedHatPad;
    juce::TextButton openHatPad;
    juce::TextButton subBassPad;

    // Virtual MIDI Keyboard Component
    juce::MidiKeyboardComponent keyboardComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubdrumProcessorAudioProcessorEditor)
};
