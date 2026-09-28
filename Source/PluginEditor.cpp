#include "PluginProcessor.h"
#include "PluginEditor.h"

SubdrumProcessorAudioProcessorEditor::SubdrumProcessorAudioProcessorEditor(SubdrumProcessorAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&minimalistLookAndFeel);

    // Row 1 Controls
    setupControl(driveKnob,       "drive",         "Drive");
    setupControl(tapeMixKnob,     "tapeMix",       "Mix");
    setupControl(cutoffKnob,      "cutoff",        "Filter");
    setupControl(resonanceKnob,   "resonance",     "Resonance");

    // Row 2 Controls
    setupControl(compThreshKnob,  "compThreshold", "Thresh");
    setupControl(compAttackKnob,  "compAttack",    "Attack");
    setupControl(compReleaseKnob, "compRelease",   "Decay");
    setupControl(compMakeupKnob,  "compMakeup",    "Volume");
    setupControl(vinylNoiseKnob,  "vinylNoise",    "Dust");
    setupControl(outputGainKnob,  "outputGain",    "Master");

    // Setup Interactive Drum Pads
    setupPad(kickPad,      "KICK [ A ]",       36);
    setupPad(snarePad,     "SNARE [ S ]",      38);
    setupPad(closedHatPad, "CLOSED HAT [ D ]", 42);
    setupPad(openHatPad,   "OPEN HAT [ F ]",   46);
    setupPad(subBassPad,   "SUB 808 [ G ]",    48);

    // Virtual Keyboard Setup
    keyboardComponent.setAvailableRange(36, 72);
    keyboardComponent.setOctaveForMiddleC(3);
    keyboardComponent.setKeyWidth(26.0f);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xFF1E2026));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xFF121316));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xFFE5838B));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x33E5838B));
    keyboardComponent.setWantsKeyboardFocus(false);
    addAndMakeVisible(keyboardComponent);

    setWantsKeyboardFocus(true);
    addKeyListener(this);

    // Initialize visualizer bar heights
    visualizerBarHeights.fill(0.0f);

    setSize(940, 650);
    startTimerHz(30);
}

SubdrumProcessorAudioProcessorEditor::~SubdrumProcessorAudioProcessorEditor()
{
    removeKeyListener(this);
    setLookAndFeel(nullptr);
}

void SubdrumProcessorAudioProcessorEditor::setupControl(RotaryControl& control, const juce::String& paramID,
                                                        const juce::String& labelText)
{
    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    control.slider.setWantsKeyboardFocus(false);
    addAndMakeVisible(control.slider);

    control.label.setText(labelText, juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    control.label.setColour(juce::Label::textColourId, juce::Colour(0xFFA6ABB6));
    control.label.setWantsKeyboardFocus(false);
    addAndMakeVisible(control.label);

    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), paramID, control.slider);
}

void SubdrumProcessorAudioProcessorEditor::setupPad(juce::TextButton& button, const juce::String& text, int note)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF181A20));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFE5838B));
    button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFC8B6CD));
    button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF121316));
    button.setClickingTogglesState(false);
    button.setWantsKeyboardFocus(false);

    button.onStateChange = [this, &button, note]()
    {
        if (button.isDown())
            triggerDrumVoice(note);
        else
            releaseDrumVoice(note);
    };

    addAndMakeVisible(button);
}

void SubdrumProcessorAudioProcessorEditor::triggerDrumVoice(int noteNumber)
{
    audioProcessor.getKeyboardState().noteOn(1, noteNumber, 1.0f);
    liveVisualizerPeak = 1.0f; // Instantly trigger visualizer transient
}

void SubdrumProcessorAudioProcessorEditor::releaseDrumVoice(int noteNumber)
{
    audioProcessor.getKeyboardState().noteOff(1, noteNumber, 0.0f);
}

bool SubdrumProcessorAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    const auto keyChar = std::tolower(key.getTextCharacter());
    const int keyCode = key.getKeyCode();

    int noteToPlay = -1;

    if (keyChar == 'a' || keyCode == 'A' || keyCode == 65)
    {
        noteToPlay = 36; // Kick
        kickPad.setState(juce::Button::buttonDown);
    }
    else if (keyChar == 's' || keyCode == 'S' || keyCode == 83)
    {
        noteToPlay = 38; // Snare
        snarePad.setState(juce::Button::buttonDown);
    }
    else if (keyChar == 'd' || keyCode == 'D' || keyCode == 68)
    {
        noteToPlay = 42; // Closed Hat
        closedHatPad.setState(juce::Button::buttonDown);
    }
    else if (keyChar == 'f' || keyCode == 'F' || keyCode == 70)
    {
        noteToPlay = 46; // Open Hat
        openHatPad.setState(juce::Button::buttonDown);
    }
    else if (keyChar == 'g' || keyCode == 'G' || keyCode == 71)
    {
        noteToPlay = 48; // Sub 808
        subBassPad.setState(juce::Button::buttonDown);
    }

    if (noteToPlay != -1)
    {
        triggerDrumVoice(noteToPlay);
        return true;
    }

    return false;
}

bool SubdrumProcessorAudioProcessorEditor::keyStateChanged(bool isKeyDown, juce::Component*)
{
    if (!isKeyDown)
    {
        kickPad.setState(juce::Button::buttonNormal);
        snarePad.setState(juce::Button::buttonNormal);
        closedHatPad.setState(juce::Button::buttonNormal);
        openHatPad.setState(juce::Button::buttonNormal);
        subBassPad.setState(juce::Button::buttonNormal);

        audioProcessor.getKeyboardState().allNotesOff(1);
    }
    return true;
}

void SubdrumProcessorAudioProcessorEditor::timerCallback()
{
    // Read audio buffer peaks
    float audioPeak = 0.0f;
    std::array<float, SubdrumProcessorAudioProcessor::visualizerBufferSize> bufferData;
    audioProcessor.getVisualizerData(bufferData.data());

    for (auto val : bufferData)
        audioPeak = std::max(audioPeak, val);

    liveVisualizerPeak = std::max(audioPeak, liveVisualizerPeak * 0.88f);

    // Update decaying vertical bars
    for (int i = 0; i < numVisualizerBars; ++i)
    {
        // Exponential decay envelope across bars matching the reference image
        const float decayFactor = std::exp(-static_cast<float>(i) * 0.28f);
        const float targetHeight = liveVisualizerPeak * decayFactor;
        visualizerBarHeights[i] = visualizerBarHeights[i] * 0.72f + targetHeight * 0.28f;
    }

    repaint();
}

void SubdrumProcessorAudioProcessorEditor::paint(juce::Graphics& g)
{
    // 1. Deep Matte Dark Background (#121316)
    g.fillAll(juce::Colour(0xFF121316));

    // 2. Top Window Bar
    g.setColour(juce::Colour(0xFF16171B));
    g.fillRect(0, 0, getWidth(), 38);

    // Minimalist macOS Traffic Light Dots
    g.setColour(juce::Colour(0xFF2C2F36));
    g.fillEllipse(20.0f, 14.0f, 10.0f, 10.0f);
    g.fillEllipse(36.0f, 14.0f, 10.0f, 10.0f);
    g.fillEllipse(52.0f, 14.0f, 10.0f, 10.0f);

    // Minimalist Top Icons (Crescent icon on left & small dot-in-square on right)
    g.setColour(juce::Colour(0xFFC8B6CD));
    juce::Path crescent;
    crescent.addEllipse(78.0f, 12.0f, 14.0f, 14.0f);
    g.strokePath(crescent, juce::PathStrokeType(1.2f));
    g.fillEllipse(82.0f, 12.0f, 10.0f, 14.0f);

    g.setColour(juce::Colour(0xFF626670));
    g.drawRoundedRectangle(static_cast<float>(getWidth() - 36), 12.0f, 14.0f, 14.0f, 3.0f, 1.2f);
    g.fillEllipse(static_cast<float>(getWidth() - 30), 18.0f, 3.0f, 3.0f);

    // 3. Upper Visualizer Screen
    auto screenBounds = juce::Rectangle<float>(20.0f, 48.0f, static_cast<float>(getWidth() - 40), 160.0f);
    g.setColour(juce::Colour(0xFF15161A));
    g.fillRoundedRectangle(screenBounds, 6.0f);

    g.setColour(juce::Colour(0xFF22242B));
    g.drawRoundedRectangle(screenBounds, 6.0f, 1.0f);

    // Dotted Grid Lines
    g.setColour(juce::Colour(0xFF262932));
    const float gridSpacingX = screenBounds.getWidth() / 14.0f;
    for (float x = screenBounds.getX() + gridSpacingX; x < screenBounds.getRight(); x += gridSpacingX)
    {
        for (float y = screenBounds.getY() + 10.0f; y < screenBounds.getBottom(); y += 8.0f)
        {
            g.fillEllipse(x - 0.75f, y - 0.75f, 1.5f, 1.5f);
        }
    }

    const float gridSpacingY = screenBounds.getHeight() / 5.0f;
    for (float y = screenBounds.getY() + gridSpacingY; y < screenBounds.getBottom(); y += gridSpacingY)
    {
        for (float x = screenBounds.getX() + 10.0f; x < screenBounds.getRight(); x += 8.0f)
        {
            g.fillEllipse(x - 0.75f, y - 0.75f, 1.5f, 1.5f);
        }
    }

    // Dashed Horizontal Zero-Crossing Center Line
    const float centerY = screenBounds.getCentreY();
    g.setColour(juce::Colour(0xFF4B4E58));
    for (float x = screenBounds.getX() + 15.0f; x < screenBounds.getRight() - 15.0f; x += 10.0f)
    {
        g.fillRect(x, centerY - 0.6f, 6.0f, 1.2f);
    }

    // Render Lavender Exponential Decay Bars (#C8B6CD)
    g.setColour(juce::Colour(0xFFC8B6CD));
    const float startX = screenBounds.getX() + 30.0f;
    const float barWidth = 4.5f;
    const float barGap = 4.0f;
    const float maxBarHeight = 110.0f;

    for (int i = 0; i < numVisualizerBars; ++i)
    {
        // Minimal idle height or live amplitude height
        const float idleH = std::exp(-static_cast<float>(i) * 0.22f) * 60.0f + 2.0f;
        const float barH = juce::jlimit(2.0f, maxBarHeight, idleH * 0.3f + visualizerBarHeights[i] * maxBarHeight);

        const float barX = startX + i * (barWidth + barGap);
        if (barX + barWidth > screenBounds.getRight() - 20.0f)
            break;

        const float barY = centerY - (barH * 0.5f);
        g.fillRoundedRectangle(barX, barY, barWidth, barH, 2.0f);
    }

    // Drum Pads & Keyboard Section Outline
    auto bottomSection = juce::Rectangle<float>(20.0f, static_cast<float>(getHeight() - 130),
                                                static_cast<float>(getWidth() - 40), 115.0f);
    g.setColour(juce::Colour(0xFF16171B));
    g.fillRoundedRectangle(bottomSection, 6.0f);
    g.setColour(juce::Colour(0xFF22242B));
    g.drawRoundedRectangle(bottomSection, 6.0f, 1.0f);
}

void SubdrumProcessorAudioProcessorEditor::resized()
{
    // Layout area for knobs
    const int knobsAreaTop = 220;
    const int rowHeight = 135;

    // Row 1: 4 Knobs (Drive, Mix, Filter, Resonance)
    auto placeKnob = [](RotaryControl& ctrl, juce::Rectangle<int> box)
    {
        auto labelBox = box.removeFromBottom(22);
        ctrl.label.setBounds(labelBox);
        ctrl.slider.setBounds(box.reduced(8));
    };

    const int row1Y = knobsAreaTop;
    const int numRow1 = 4;
    const int knobWidth1 = (getWidth() - 40) / numRow1;

    placeKnob(driveKnob,     juce::Rectangle<int>(20 + 0 * knobWidth1, row1Y, knobWidth1, rowHeight));
    placeKnob(tapeMixKnob,   juce::Rectangle<int>(20 + 1 * knobWidth1, row1Y, knobWidth1, rowHeight));
    placeKnob(cutoffKnob,    juce::Rectangle<int>(20 + 2 * knobWidth1, row1Y, knobWidth1, rowHeight));
    placeKnob(resonanceKnob, juce::Rectangle<int>(20 + 3 * knobWidth1, row1Y, knobWidth1, rowHeight));

    // Row 2: 6 Knobs (Thresh, Attack, Decay, Volume, Dust, Master)
    const int row2Y = knobsAreaTop + rowHeight + 10;
    const int numRow2 = 6;
    const int knobWidth2 = (getWidth() - 40) / numRow2;

    placeKnob(compThreshKnob,  juce::Rectangle<int>(20 + 0 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(compAttackKnob,  juce::Rectangle<int>(20 + 1 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(compReleaseKnob, juce::Rectangle<int>(20 + 2 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(compMakeupKnob,  juce::Rectangle<int>(20 + 3 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(vinylNoiseKnob,  juce::Rectangle<int>(20 + 4 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(outputGainKnob,  juce::Rectangle<int>(20 + 5 * knobWidth2, row2Y, knobWidth2, rowHeight));

    // Bottom Pads & Keyboard
    auto bottomArea = juce::Rectangle<int>(20, getHeight() - 130, getWidth() - 40, 115).reduced(10);
    auto padsRow = bottomArea.removeFromTop(38);
    const int padW = (padsRow.getWidth() - 32) / 5;

    kickPad.setBounds(padsRow.removeFromLeft(padW));
    padsRow.removeFromLeft(8);
    snarePad.setBounds(padsRow.removeFromLeft(padW));
    padsRow.removeFromLeft(8);
    closedHatPad.setBounds(padsRow.removeFromLeft(padW));
    padsRow.removeFromLeft(8);
    openHatPad.setBounds(padsRow.removeFromLeft(padW));
    padsRow.removeFromLeft(8);
    subBassPad.setBounds(padsRow);

    bottomArea.removeFromTop(6);
    keyboardComponent.setBounds(bottomArea);
}
