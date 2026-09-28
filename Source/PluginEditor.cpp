#include "PluginProcessor.h"
#include "PluginEditor.h"

SubdrumProcessorAudioProcessorEditor::SubdrumProcessorAudioProcessorEditor(SubdrumProcessorAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&customLookAndFeel);

    // Setup All Controls
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

    // Virtual Keyboard Setup
    keyboardComponent.setAvailableRange(36, 72);
    keyboardComponent.setOctaveForMiddleC(3);
    keyboardComponent.setKeyWidth(26.0f);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xFF262C34));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xFF14171A));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xFFFF7A00));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x33FFA23A));
    addAndMakeVisible(keyboardComponent);

    setWantsKeyboardFocus(true);
    setSize(920, 620);
    startTimerHz(30);
}

SubdrumProcessorAudioProcessorEditor::~SubdrumProcessorAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void SubdrumProcessorAudioProcessorEditor::setupControl(RotaryControl& control, const juce::String& paramID,
                                                        const juce::String& labelText, const juce::String& suffix)
{
    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    control.slider.setTextValueSuffix(suffix);
    addAndMakeVisible(control.slider);

    control.label.setText(labelText, juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    addAndMakeVisible(control.label);

    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), paramID, control.slider);
}

bool SubdrumProcessorAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    int noteToPlay = -1;
    const auto keyChar = std::tolower(key.getTextCharacter());

    switch (keyChar)
    {
        case 'a': noteToPlay = 36; break; // Kick
        case 's': noteToPlay = 38; break; // Snare
        case 'd': noteToPlay = 42; break; // Closed Hat
        case 'f': noteToPlay = 46; break; // Open Hat
        case 'g': noteToPlay = 48; break; // Sub Bass
        default: break;
    }

    if (noteToPlay != -1)
    {
        audioProcessor.getKeyboardState().noteOn(1, noteToPlay, 0.95f);
        return true;
    }

    return false;
}

bool SubdrumProcessorAudioProcessorEditor::keyStateChanged(bool isKeyDown)
{
    if (!isKeyDown)
    {
        // Release notes on key up
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
    contentArea.removeFromBottom(105); // Space for keyboard & hints

    auto topRow = contentArea.removeFromTop(190);
    contentArea.removeFromTop(10);
    auto bottomRow = contentArea;

    auto tapeArea = topRow.removeFromLeft(440);
    topRow.removeFromLeft(10);
    auto filterArea = topRow;

    auto compArea = bottomRow.removeFromLeft(560);
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

    // Keyboard Section Hints Header
    auto kbArea = getLocalBounds().reduced(16).removeFromBottom(100);
    g.setColour(juce::Colour(0xFF181C21));
    g.fillRoundedRectangle(kbArea.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xFF262C34));
    g.drawRoundedRectangle(kbArea.toFloat(), 6.0f, 1.0f);

    g.setColour(juce::Colour(0xFFFFA23A));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("LIVE DRUM TRIGGER (TOCA CON TU TECLADO):", kbArea.getX() + 12, kbArea.getY() + 6, 320, 18, juce::Justification::left);

    g.setColour(juce::Colour(0xFF8CD0FF));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("[A] KICK    |    [S] SNARE    |    [D] CLOSED HAT    |    [F] OPEN HAT    |    [G] SUB BASS",
               kbArea.getX() + 320, kbArea.getY() + 6, kbArea.getWidth() - 330, 18, juce::Justification::right);
}

void SubdrumProcessorAudioProcessorEditor::resized()
{
    auto contentArea = getLocalBounds().reduced(16);
    contentArea.removeFromTop(50);

    auto kbArea = contentArea.removeFromBottom(100);
    contentArea.removeFromBottom(10);

    auto topRow = contentArea.removeFromTop(190);
    contentArea.removeFromTop(10);
    auto bottomRow = contentArea;

    auto placeKnob = [](RotaryControl& ctrl, juce::Rectangle<int> box)
    {
        ctrl.label.setBounds(box.removeFromTop(16));
        ctrl.slider.setBounds(box);
    };

    // 1. Tape Section
    auto tapeArea = topRow.removeFromLeft(440);
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
    auto compArea = bottomRow.removeFromLeft(560);
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

    // 5. Virtual Keyboard
    keyboardComponent.setBounds(kbArea.getX() + 10, kbArea.getY() + 28, kbArea.getWidth() - 20, 62);
}
