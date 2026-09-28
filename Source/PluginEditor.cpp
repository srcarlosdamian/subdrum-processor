#include "PluginProcessor.h"
#include "PluginEditor.h"

SubdrumProcessorAudioProcessorEditor::SubdrumProcessorAudioProcessorEditor(SubdrumProcessorAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&customLookAndFeel);

    // Setup DSP Rotary Controls
    setupControl(driveKnob,       "drive",         "DRIVE",       " dB");
    setupControl(tapeMixKnob,     "tapeMix",       "MIX",         " %");

    setupControl(cutoffKnob,      "cutoff",        "CUTOFF",      " Hz");
    setupControl(resonanceKnob,   "resonance",     "RESO",        " Q");

    setupControl(compThreshKnob,  "compThreshold", "THRESH",      " dB");
    setupControl(compRatioKnob,   "compRatio",     "RATIO",       ":1");
    setupControl(compAttackKnob,  "compAttack",    "ATTACK",      " ms");
    setupControl(compReleaseKnob, "compRelease",   "RELEASE",     " ms");
    setupControl(compMakeupKnob,  "compMakeup",    "MAKEUP",      " dB");
    setupControl(compMixKnob,     "compMix",       "COMP MIX",    " %");

    setupControl(vinylNoiseKnob,  "vinylNoise",    "VINYL NOISE", " %");
    setupControl(vinylDustKnob,   "vinylDust",     "DUST POPS",   " %");

    setupControl(outputGainKnob,  "outputGain",    "OUTPUT TRIM", " dB");

    // Setup Interactive Drum Pads
    setupPad(kickPad,      "KICK\n[ A ]",       36);
    setupPad(snarePad,     "SNARE\n[ S ]",      38);
    setupPad(closedHatPad, "CLOSED HAT\n[ D ]", 42);
    setupPad(openHatPad,   "OPEN HAT\n[ F ]",   46);
    setupPad(subBassPad,   "SUB 808\n[ G ]",    48);

    // Virtual Keyboard Setup
    keyboardComponent.setAvailableRange(36, 72);
    keyboardComponent.setOctaveForMiddleC(3);
    keyboardComponent.setKeyWidth(26.0f);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xFF262C34));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xFF14171A));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xFFFF7A00));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x33FFA23A));
    keyboardComponent.setWantsKeyboardFocus(false);
    addAndMakeVisible(keyboardComponent);

    // Add KeyListener to catch keyboard strokes
    addKeyListener(this);
    setWantsKeyboardFocus(true);

    setSize(940, 650);
    startTimerHz(30);
}

SubdrumProcessorAudioProcessorEditor::~SubdrumProcessorAudioProcessorEditor()
{
    removeKeyListener(this);
    setLookAndFeel(nullptr);
}

void SubdrumProcessorAudioProcessorEditor::setupControl(RotaryControl& control, const juce::String& paramID,
                                                        const juce::String& labelText, const juce::String& suffix)
{
    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    control.slider.setTextValueSuffix(suffix);
    control.slider.setWantsKeyboardFocus(false);
    addAndMakeVisible(control.slider);

    control.label.setText(labelText, juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    control.label.setWantsKeyboardFocus(false);
    addAndMakeVisible(control.label);

    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), paramID, control.slider);
}

void SubdrumProcessorAudioProcessorEditor::setupPad(juce::TextButton& button, const juce::String& text, int note)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF20252C));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFF7A00));
    button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFFA23A));
    button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFFFFFFFF));
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
    const float targetGr = audioProcessor.getGainReduction();
    currentMeterGainReduction = currentMeterGainReduction * 0.7f + targetGr * 0.3f;
    repaint();
}

void SubdrumProcessorAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Deep Charcoal Background
    g.fillAll(juce::Colour(0xFF121417));

    // Header Area
    g.setColour(juce::Colour(0xFF191D22));
    g.fillRect(0, 0, getWidth(), 56);

    g.setColour(juce::Colour(0xFFFF7A00));
    g.fillRect(0, 54, getWidth(), 2);

    // Title & Subtitle
    g.setColour(juce::Colour(0xFFFFFFFF));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("SUBDRUM PROCESSOR", 24, 8, 300, 22, juce::Justification::left);

    g.setColour(juce::Colour(0xFF8C95A0));
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText("UNDERGROUND 2-STEP / LO-FI DRUM ENGINE", 24, 30, 300, 16, juce::Justification::left);

    // Section Panels Helper
    auto drawPanel = [&](juce::Rectangle<int> bounds, const juce::String& title)
    {
        g.setColour(juce::Colour(0xFF181C21));
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

        g.setColour(juce::Colour(0xFF262C34));
        g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 1.2f);

        g.setColour(juce::Colour(0xFFFFA23A));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, bounds.getX() + 14, bounds.getY() + 8, bounds.getWidth() - 28, 18, juce::Justification::left);

        g.setColour(juce::Colour(0xFF2E3540));
        g.fillRect(bounds.getX() + 14, bounds.getY() + 28, bounds.getWidth() - 28, 1);
    };

    // Layout Panels
    auto contentArea = getLocalBounds().reduced(16);
    contentArea.removeFromTop(50);
    contentArea.removeFromBottom(150); // Space for drum pads & keyboard

    auto topRow = contentArea.removeFromTop(185);
    contentArea.removeFromTop(10);
    auto bottomRow = contentArea;

    auto tapeArea = topRow.removeFromLeft(450);
    topRow.removeFromLeft(10);
    auto filterArea = topRow;

    auto compArea = bottomRow.removeFromLeft(570);
    bottomRow.removeFromLeft(10);
    auto vinylArea = bottomRow;

    drawPanel(tapeArea, "1. TAPE DRIVE  [4X OVERSAMPLED]");
    drawPanel(filterArea, "2. SAMPLER FILTER  [LO-FI ROLLOFF]");
    drawPanel(compArea, "3. VCA TRANSIENT COMPRESSOR");
    drawPanel(vinylArea, "4. TEXTURE & MASTER");

    // Draw Gain Reduction Meter in Compressor panel
    auto meterBounds = juce::Rectangle<int>(compArea.getX() + compArea.getWidth() - 140, compArea.getY() + 8, 120, 14);
    g.setColour(juce::Colour(0xFF121417));
    g.fillRoundedRectangle(meterBounds.toFloat(), 3.0f);
    g.setColour(juce::Colour(0xFF323842));
    g.drawRoundedRectangle(meterBounds.toFloat(), 3.0f, 1.0f);

    float grProportion = juce::jlimit(0.0f, 1.0f, currentMeterGainReduction / 24.0f);
    auto fillMeter = meterBounds.reduced(2);
    fillMeter.setWidth(static_cast<int>(fillMeter.getWidth() * grProportion));
    g.setColour(juce::Colour(0xFFFF5533));
    g.fillRoundedRectangle(fillMeter.toFloat(), 2.0f);

    g.setColour(juce::Colour(0xFF909AA4));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(juce::String::formatted("GR: -%.1f dB", currentMeterGainReduction),
               meterBounds.getX() - 75, meterBounds.getY(), 70, 14, juce::Justification::right);

    // Drum Pads & Keyboard Section Header
    auto padSectionArea = getLocalBounds().reduced(16).removeFromBottom(145);
    g.setColour(juce::Colour(0xFF181C21));
    g.fillRoundedRectangle(padSectionArea.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xFF262C34));
    g.drawRoundedRectangle(padSectionArea.toFloat(), 6.0f, 1.0f);

    g.setColour(juce::Colour(0xFFFFA23A));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("LIVE DRUM PADS & MAC KEYBOARD TRIGGER:", padSectionArea.getX() + 12, padSectionArea.getY() + 6, 350, 18, juce::Justification::left);

    g.setColour(juce::Colour(0xFF6EC6FF));
    g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
    g.drawText("Presiona las teclas [A, S, D, F, G] en tu Mac o haz clic en los pads para escuchar",
               padSectionArea.getX() + 360, padSectionArea.getY() + 6, padSectionArea.getWidth() - 375, 18, juce::Justification::right);
}

void SubdrumProcessorAudioProcessorEditor::resized()
{
    auto contentArea = getLocalBounds().reduced(16);
    contentArea.removeFromTop(50);

    auto padSectionArea = contentArea.removeFromBottom(145);
    contentArea.removeFromBottom(10);

    auto topRow = contentArea.removeFromTop(185);
    contentArea.removeFromTop(10);
    auto bottomRow = contentArea;

    auto placeKnob = [](RotaryControl& ctrl, juce::Rectangle<int> box)
    {
        ctrl.label.setBounds(box.removeFromTop(16));
        ctrl.slider.setBounds(box);
    };

    // 1. Tape Section
    auto tapeArea = topRow.removeFromLeft(450);
    topRow.removeFromLeft(10);
    auto tapeContent = tapeArea.reduced(14).withTrimmedTop(22);
    int tapeColW = tapeContent.getWidth() / 2;

    placeKnob(driveKnob, tapeContent.removeFromLeft(tapeColW).reduced(10, 0));
    placeKnob(tapeMixKnob, tapeContent.reduced(10, 0));

    // 2. Filter Section
    auto filterArea = topRow;
    auto filterContent = filterArea.reduced(14).withTrimmedTop(22);
    int filterColW = filterContent.getWidth() / 2;

    placeKnob(cutoffKnob, filterContent.removeFromLeft(filterColW).reduced(10, 0));
    placeKnob(resonanceKnob, filterContent.reduced(10, 0));

    // 3. Compressor Section
    auto compArea = bottomRow.removeFromLeft(570);
    bottomRow.removeFromLeft(10);
    auto compContent = compArea.reduced(12).withTrimmedTop(22);
    int compColW = compContent.getWidth() / 6;

    placeKnob(compThreshKnob,  compContent.removeFromLeft(compColW).reduced(4, 0));
    placeKnob(compRatioKnob,   compContent.removeFromLeft(compColW).reduced(4, 0));
    placeKnob(compAttackKnob,  compContent.removeFromLeft(compColW).reduced(4, 0));
    placeKnob(compReleaseKnob, compContent.removeFromLeft(compColW).reduced(4, 0));
    placeKnob(compMakeupKnob,  compContent.removeFromLeft(compColW).reduced(4, 0));
    placeKnob(compMixKnob,     compContent.reduced(4, 0));

    // 4. Vinyl & Master Section
    auto vinylArea = bottomRow;
    auto vinylContent = vinylArea.reduced(12).withTrimmedTop(22);
    int vinylColW = vinylContent.getWidth() / 3;

    placeKnob(vinylNoiseKnob,  vinylContent.removeFromLeft(vinylColW).reduced(4, 0));
    placeKnob(vinylDustKnob,   vinylContent.removeFromLeft(vinylColW).reduced(4, 0));
    placeKnob(outputGainKnob,  vinylContent.reduced(4, 0));

    // 5. Drum Pads Row
    auto padsRow = padSectionArea.reduced(12).withTrimmedTop(20);
    auto padsOnlyRow = padsRow.removeFromTop(44);
    int padW = (padsOnlyRow.getWidth() - 32) / 5;

    kickPad.setBounds(padsOnlyRow.removeFromLeft(padW));
    padsOnlyRow.removeFromLeft(8);
    snarePad.setBounds(padsOnlyRow.removeFromLeft(padW));
    padsOnlyRow.removeFromLeft(8);
    closedHatPad.setBounds(padsOnlyRow.removeFromLeft(padW));
    padsOnlyRow.removeFromLeft(8);
    openHatPad.setBounds(padsOnlyRow.removeFromLeft(padW));
    padsOnlyRow.removeFromLeft(8);
    subBassPad.setBounds(padsOnlyRow);

    // 6. Virtual MIDI Keyboard
    padsRow.removeFromTop(6);
    keyboardComponent.setBounds(padsRow);
}
