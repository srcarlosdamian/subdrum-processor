#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Custom LookAndFeel implementing the Industrial Dot-Matrix aesthetic
class IndustrialDotMatrixLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IndustrialDotMatrixLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFF3B30));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFFB8BBC2));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFF3B30));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFF14161A));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
        setColour(juce::Label::textColourId, juce::Colour(0xFF14161A));

        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF14161A));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xFFEDEDF0));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF323640));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFFF3B30));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF14161A));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xFFEDEDF0));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFFFF3B30));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xFFFFFFFF));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height)).reduced(4.0f);

        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto centre = bounds.getCentre();

        // 1. Off-white Outer Dial Disc
        auto outerRadius = radius - 2.5f;
        g.setColour(juce::Colour(0xFFEDEDF0));
        g.fillEllipse(centre.x - outerRadius, centre.y - outerRadius, outerRadius * 2.0f, outerRadius * 2.0f);

        // Thin outer perimeter border
        g.setColour(juce::Colour(0xFFD0D3D9));
        g.drawEllipse(centre.x - outerRadius, centre.y - outerRadius, outerRadius * 2.0f, outerRadius * 2.0f, 1.0f);

        // 2. Radial Tick Marks around the perimeter
        const int numTicks = 20;
        for (int i = 0; i < numTicks; ++i)
        {
            const float tickAngle = rotaryStartAngle + (static_cast<float>(i) / static_cast<float>(numTicks - 1)) * (rotaryEndAngle - rotaryStartAngle);
            const auto pOuter = centre.getPointOnCircumference(outerRadius - 2.0f, tickAngle);
            const auto pInner = centre.getPointOnCircumference(outerRadius - (i % 5 == 0 ? 6.0f : 4.0f), tickAngle);

            g.setColour(i % 5 == 0 ? juce::Colour(0xFF1A1C20) : juce::Colour(0xFFA0A4AC));
            g.drawLine(pInner.x, pInner.y, pOuter.x, pOuter.y, i % 5 == 0 ? 1.4f : 0.85f);
        }

        // 3. Red Accent Indicator Dot at current angle
        const auto redDotPos = centre.getPointOnCircumference(outerRadius - 4.5f, toAngle);
        g.setColour(juce::Colour(0xFFFF3B30));
        g.fillEllipse(redDotPos.x - 2.5f, redDotPos.y - 2.5f, 5.0f, 5.0f);

        // 4. Center Rotor Hub
        auto innerRadius = outerRadius * 0.46f;
        g.setColour(juce::Colour(0xFF14161A));
        g.fillEllipse(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

        const float spokeRadius = innerRadius * 0.58f;
        g.setColour(juce::Colour(0xFFEDEDF0));
        for (int i = 0; i < 3; ++i)
        {
            const float spokeAngle = toAngle + i * (juce::MathConstants<float>::twoPi / 3.0f);
            const auto spokePt = centre.getPointOnCircumference(spokeRadius, spokeAngle);
            g.fillEllipse(spokePt.x - 2.0f, spokePt.y - 2.0f, 4.0f, 4.0f);
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

    enum class SoundTab
    {
        Kick = 0,
        Snare,
        Clap,
        ClosedHat,
        OpenHat,
        Rimshot,
        Sub808,
        Shaker,
        MasterDSP
    };

    void setActiveTab(SoundTab tab);

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

    // Header Controls: Presets, Rhythms, and Import/Export
    juce::ComboBox presetComboBox;
    juce::ComboBox rhythmComboBox;
    juce::TextButton saveButton;
    juce::TextButton exportButton;
    juce::TextButton importButton;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Visualizer Bars Animation State
    static constexpr int numVisualizerCols = 32;
    std::array<float, numVisualizerCols> visualizerBarHeights {};
    float liveVisualizerPeak { 0.0f };

    // 9 Sound Selection Tab Buttons (8 Sounds + Master DSP)
    std::array<juce::TextButton, 9> tabButtons;
    SoundTab activeTab { SoundTab::Snare };

    // 1. KICK (6 Knobs)
    std::array<RotaryControl, 6> kickControls;
    // 2. SNARE (6 Knobs)
    std::array<RotaryControl, 6> snareControls;
    // 3. CLAP (6 Knobs)
    std::array<RotaryControl, 6> clapControls;
    // 4. CLOSED HAT (6 Knobs)
    std::array<RotaryControl, 6> chatControls;
    // 5. OPEN HAT (6 Knobs)
    std::array<RotaryControl, 6> ohatControls;
    // 6. RIMSHOT (6 Knobs)
    std::array<RotaryControl, 6> rimControls;
    // 7. SUB 808 (6 Knobs)
    std::array<RotaryControl, 6> subControls;
    // 8. SHAKER (6 Knobs)
    std::array<RotaryControl, 6> shakerControls;
    // 9. MASTER DSP (11 Knobs)
    std::array<RotaryControl, 11> masterControls;

    // TR-808 Style Step Sequencer UI Components
    juce::TextButton playButton;
    juce::TextButton hostSyncButton;
    juce::Slider bpmSlider;
    juce::Slider swingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAttachment;
    juce::TextButton clearPatternButton;

    // 4 Tracks x 16 Steps
    std::array<juce::Label, 4> trackLabels;
    std::array<std::array<juce::TextButton, 16>, 4> stepButtons;

    void updateSequencerButtonColours();

    // 8 Interactive Performance Drum Pads
    std::array<juce::TextButton, 8> drumPads;

    int octaveOffset { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubdrumProcessorAudioProcessorEditor)
};
