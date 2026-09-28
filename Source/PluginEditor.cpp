#include "PluginProcessor.h"
#include "PluginEditor.h"

SubdrumProcessorAudioProcessorEditor::SubdrumProcessorAudioProcessorEditor(SubdrumProcessorAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    setLookAndFeel(&industrialLookAndFeel);

    // Preset Selector Setup
    const auto& presets = audioProcessor.getPresets();
    for (size_t i = 0; i < presets.size(); ++i)
    {
        presetComboBox.addItem(presets[i].name, static_cast<int>(i + 1));
    }
    presetComboBox.setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetComboBox.setWantsKeyboardFocus(false);
    presetComboBox.onChange = [this]()
    {
        const int selectedIdx = presetComboBox.getSelectedId() - 1;
        if (selectedIdx >= 0)
        {
            audioProcessor.setCurrentProgram(selectedIdx);
        }
    };
    addAndMakeVisible(presetComboBox);

    // Tab Navigation Buttons Setup
    auto setupTabBtn = [this](juce::TextButton& btn, const juce::String& text, SoundTab tab)
    {
        btn.setButtonText(text);
        btn.setWantsKeyboardFocus(false);
        btn.onClick = [this, tab]() { setActiveTab(tab); };
        addAndMakeVisible(btn);
    };

    setupTabBtn(kickTabButton,   "1. BASS KICK",    SoundTab::Kick);
    setupTabBtn(snareTabButton,  "2. 2-STEP CLAP",  SoundTab::SnareClap);
    setupTabBtn(masterTabButton, "3. MASTER DSP",   SoundTab::MasterDSP);

    // 1. KICK CONTROLS (6 Knobs)
    setupControl(kickPitchKnob, "kickPitch", "Pitch (st)");
    setupControl(kickTuneKnob,  "kickTune",  "Tune (Hz)");
    setupControl(kickSweepKnob, "kickSweep", "Pitch Drop");
    setupControl(kickDecayKnob, "kickDecay", "Decay (ms)");
    setupControl(kickPunchKnob, "kickPunch", "Punch Click");
    setupControl(kickDriveKnob, "kickDrive", "Overdrive");

    // 2. SNARE / CLAP CONTROLS (6 Knobs)
    setupControl(snarePitchKnob,  "snarePitch",  "Pitch (st)");
    setupControl(snareDecayKnob,  "snareDecay",  "Decay (ms)");
    setupControl(snareNoiseKnob,  "snareNoise",  "Noise Mix");
    setupControl(snareSnapKnob,   "snareSnap",   "Crack Snap");
    setupControl(snareSizzleKnob, "snareSizzle", "Air Sizzle");
    setupControl(snareBodyKnob,   "snareBody",   "Body Punch");

    // 3. MASTER DSP, DUB ECHO & ROOM AMBIENCE CONTROLS (6 Knobs)
    setupControl(roomMixKnob,    "roomMix",    "Room Space");
    setupControl(roomSizeKnob,   "roomSize",   "Room Size");
    setupControl(echoMixKnob,    "echoMix",    "Dub Echo");
    setupControl(driveKnob,      "drive",      "Tape Drive");
    setupControl(cutoffKnob,     "cutoff",     "Master Cutoff");
    setupControl(outputGainKnob, "outputGain", "Master Gain");

    // TR-808 Sequencer Transport Setup
    playButton.setButtonText("▶ PLAY");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF14161A));
    playButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFEDEDF0));
    playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFF3B30));
    playButton.setWantsKeyboardFocus(false);
    playButton.onClick = [this]()
    {
        const bool playing = !audioProcessor.getSequencer().isPlaying();
        audioProcessor.getSequencer().setPlaying(playing);
        updateSequencerButtonColours();
    };
    addAndMakeVisible(playButton);

    bpmSlider.setSliderStyle(juce::Slider::LinearBar);
    bpmSlider.setRange(60.0, 180.0, 1.0);
    bpmSlider.setValue(audioProcessor.getSequencer().getBpm(), juce::dontSendNotification);
    bpmSlider.setTextValueSuffix(" BPM");
    bpmSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFF14161A));
    bpmSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFEDEDF0));
    bpmSlider.setWantsKeyboardFocus(false);
    bpmSlider.onValueChange = [this]()
    {
        audioProcessor.getSequencer().setBpm(bpmSlider.getValue());
    };
    addAndMakeVisible(bpmSlider);

    clearPatternButton.setButtonText("CLEAR");
    clearPatternButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF30343D));
    clearPatternButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFD4D7DE));
    clearPatternButton.setWantsKeyboardFocus(false);
    clearPatternButton.onClick = [this]()
    {
        audioProcessor.getSequencer().clear();
        updateSequencerButtonColours();
    };
    addAndMakeVisible(clearPatternButton);

    defaultPatternButton.setButtonText("2-STEP LOOP");
    defaultPatternButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF30343D));
    defaultPatternButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFD4D7DE));
    defaultPatternButton.setWantsKeyboardFocus(false);
    defaultPatternButton.onClick = [this]()
    {
        audioProcessor.getSequencer().loadDefaultPattern();
        updateSequencerButtonColours();
    };
    addAndMakeVisible(defaultPatternButton);

    // Track Labels
    kickTrackLabel.setText("1. KICK", juce::dontSendNotification);
    kickTrackLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    kickTrackLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF14161A));
    addAndMakeVisible(kickTrackLabel);

    snareTrackLabel.setText("2. CLAP", juce::dontSendNotification);
    snareTrackLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    snareTrackLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF14161A));
    addAndMakeVisible(snareTrackLabel);

    // 16 Step Buttons for Kick & Snare
    for (int s = 0; s < 16; ++s)
    {
        kickStepButtons[s].setButtonText(juce::String(s + 1));
        kickStepButtons[s].setWantsKeyboardFocus(false);
        kickStepButtons[s].onClick = [this, s]()
        {
            audioProcessor.getSequencer().toggleStep(0, s);
            updateSequencerButtonColours();
        };
        addAndMakeVisible(kickStepButtons[s]);

        snareStepButtons[s].setButtonText(juce::String(s + 1));
        snareStepButtons[s].setWantsKeyboardFocus(false);
        snareStepButtons[s].onClick = [this, s]()
        {
            audioProcessor.getSequencer().toggleStep(1, s);
            updateSequencerButtonColours();
        };
        addAndMakeVisible(snareStepButtons[s]);
    }

    // Setup Interactive Performance Pads (Strictly 2 Active Sounds)
    setupPad(kickPad,  "1. KICK\n[ A ]",  36, juce::Colour(0xFFB5A895), juce::Colour(0xFF14161A)); // Taupe
    setupPad(snarePad, "2. CLAP\n[ S ]",  38, juce::Colour(0xFF9D9BFF), juce::Colour(0xFF14161A)); // Periwinkle

    for (size_t i = 0; i < emptyPads.size(); ++i)
    {
        emptyPads[i].setButtonText(juce::String(static_cast<int>(i + 3)) + ". [ EMPTY ]");
        emptyPads[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFD6D9DF));
        emptyPads[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF989CA8));
        emptyPads[i].setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFD6D9DF));
        emptyPads[i].setEnabled(false);
        emptyPads[i].setWantsKeyboardFocus(false);
        addAndMakeVisible(emptyPads[i]);
    }

    setActiveTab(SoundTab::SnareClap); // Default focus to Snare/Clap tab per user request
    updateSequencerButtonColours();

    setWantsKeyboardFocus(true);
    addKeyListener(this);

    visualizerBarHeights.fill(0.0f);

    setSize(920, 680);
    startTimerHz(30);
}

SubdrumProcessorAudioProcessorEditor::~SubdrumProcessorAudioProcessorEditor()
{
    removeKeyListener(this);
    setLookAndFeel(nullptr);
}

void SubdrumProcessorAudioProcessorEditor::setActiveTab(SoundTab tab)
{
    activeTab = tab;

    // Tab Button Styling
    auto updateTabBtnStyle = [](juce::TextButton& btn, bool active)
    {
        if (active)
        {
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFF3B30));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFFFFFF));
        }
        else
        {
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFD2D5DC));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF4A4E58));
        }
    };

    updateTabBtnStyle(kickTabButton,   activeTab == SoundTab::Kick);
    updateTabBtnStyle(snareTabButton,  activeTab == SoundTab::SnareClap);
    updateTabBtnStyle(masterTabButton, activeTab == SoundTab::MasterDSP);

    // Toggle Visibility of Knob Groups
    const bool isKick = (activeTab == SoundTab::Kick);
    kickPitchKnob.slider.setVisible(isKick);   kickPitchKnob.label.setVisible(isKick);
    kickTuneKnob.slider.setVisible(isKick);    kickTuneKnob.label.setVisible(isKick);
    kickSweepKnob.slider.setVisible(isKick);   kickSweepKnob.label.setVisible(isKick);
    kickDecayKnob.slider.setVisible(isKick);   kickDecayKnob.label.setVisible(isKick);
    kickPunchKnob.slider.setVisible(isKick);   kickPunchKnob.label.setVisible(isKick);
    kickDriveKnob.slider.setVisible(isKick);   kickDriveKnob.label.setVisible(isKick);

    const bool isSnare = (activeTab == SoundTab::SnareClap);
    snarePitchKnob.slider.setVisible(isSnare);   snarePitchKnob.label.setVisible(isSnare);
    snareDecayKnob.slider.setVisible(isSnare);   snareDecayKnob.label.setVisible(isSnare);
    snareNoiseKnob.slider.setVisible(isSnare);   snareNoiseKnob.label.setVisible(isSnare);
    snareSnapKnob.slider.setVisible(isSnare);    snareSnapKnob.label.setVisible(isSnare);
    snareSizzleKnob.slider.setVisible(isSnare);  snareSizzleKnob.label.setVisible(isSnare);
    snareBodyKnob.slider.setVisible(isSnare);    snareBodyKnob.label.setVisible(isSnare);

    const bool isMaster = (activeTab == SoundTab::MasterDSP);
    roomMixKnob.slider.setVisible(isMaster);     roomMixKnob.label.setVisible(isMaster);
    roomSizeKnob.slider.setVisible(isMaster);    roomSizeKnob.label.setVisible(isMaster);
    echoMixKnob.slider.setVisible(isMaster);     echoMixKnob.label.setVisible(isMaster);
    driveKnob.slider.setVisible(isMaster);       driveKnob.label.setVisible(isMaster);
    cutoffKnob.slider.setVisible(isMaster);      cutoffKnob.label.setVisible(isMaster);
    outputGainKnob.slider.setVisible(isMaster);  outputGainKnob.label.setVisible(isMaster);

    resized();
    repaint();
}

void SubdrumProcessorAudioProcessorEditor::updateSequencerButtonColours()
{
    const int curStep = audioProcessor.getSequencer().getCurrentStep();
    const bool isPlaying = audioProcessor.getSequencer().isPlaying();

    // TR-808 Authentic 4-Step Color Palette
    // Steps 1-4: Red (#E63946)
    // Steps 5-8: Orange (#F77F00)
    // Steps 9-12: Yellow (#FCBF49)
    // Steps 13-16: Off-White (#D8DBE2)
    const juce::Colour tr808ActiveColors[4] = {
        juce::Colour(0xFFE63946), // Red
        juce::Colour(0xFFF77F00), // Orange
        juce::Colour(0xFFFCBF49), // Yellow
        juce::Colour(0xFFE2E4E8)  // Off-White
    };

    for (int s = 0; s < 16; ++s)
    {
        const int group = s / 4;
        const bool isCursor = (isPlaying && curStep == s);
        const juce::Colour blockCol = tr808ActiveColors[group];

        // Track 0: KICK
        const bool kickActive = audioProcessor.getSequencer().getStep(0, s);
        if (kickActive)
        {
            kickStepButtons[s].setColour(juce::TextButton::buttonColourId, isCursor ? juce::Colour(0xFFFFFFFF) : blockCol);
            kickStepButtons[s].setColour(juce::TextButton::textColourOffId, group == 2 ? juce::Colour(0xFF14161A) : (group == 3 ? juce::Colour(0xFF14161A) : juce::Colour(0xFFFFFFFF)));
        }
        else
        {
            kickStepButtons[s].setColour(juce::TextButton::buttonColourId, isCursor ? juce::Colour(0xFF555964) : juce::Colour(0xFF262930));
            kickStepButtons[s].setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF7A7E88));
        }

        // Track 1: SNARE / CLAP
        const bool snareActive = audioProcessor.getSequencer().getStep(1, s);
        if (snareActive)
        {
            snareStepButtons[s].setColour(juce::TextButton::buttonColourId, isCursor ? juce::Colour(0xFFFFFFFF) : blockCol);
            snareStepButtons[s].setColour(juce::TextButton::textColourOffId, group == 2 ? juce::Colour(0xFF14161A) : (group == 3 ? juce::Colour(0xFF14161A) : juce::Colour(0xFFFFFFFF)));
        }
        else
        {
            snareStepButtons[s].setColour(juce::TextButton::buttonColourId, isCursor ? juce::Colour(0xFF555964) : juce::Colour(0xFF262930));
            snareStepButtons[s].setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF7A7E88));
        }
    }
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
    control.label.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    control.label.setColour(juce::Label::textColourId, juce::Colour(0xFF14161A));
    control.label.setWantsKeyboardFocus(false);
    addAndMakeVisible(control.label);

    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), paramID, control.slider);
}

void SubdrumProcessorAudioProcessorEditor::setupPad(juce::TextButton& button, const juce::String& text, int note,
                                                    juce::Colour baseColor, juce::Colour textCol)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, baseColor);
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFF3B30));
    button.setColour(juce::TextButton::textColourOffId, textCol);
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
    liveVisualizerPeak = 1.0f;
}

void SubdrumProcessorAudioProcessorEditor::releaseDrumVoice(int noteNumber)
{
    audioProcessor.getKeyboardState().noteOff(1, noteNumber, 0.0f);
}

bool SubdrumProcessorAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    const auto keyChar = std::tolower(key.getTextCharacter());
    const int keyCode = key.getKeyCode();

    // Octave Transpose (Z / X)
    if (keyChar == 'z' || keyCode == 'Z')
    {
        octaveOffset = juce::jmax(-24, octaveOffset - 12);
        return true;
    }
    if (keyChar == 'x' || keyCode == 'X')
    {
        octaveOffset = juce::jmin(24, octaveOffset + 12);
        return true;
    }

    int noteToPlay = -1;

    // Sound 1: KICK (A)
    if (keyChar == 'a' || keyCode == 'A')
    {
        noteToPlay = 36 + octaveOffset;
        kickPad.setState(juce::Button::buttonDown);
    }
    // Sound 2: SNARE (S)
    else if (keyChar == 's' || keyCode == 'S')
    {
        noteToPlay = 38 + octaveOffset;
        snarePad.setState(juce::Button::buttonDown);
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
        audioProcessor.getKeyboardState().allNotesOff(1);
    }
    return true;
}

void SubdrumProcessorAudioProcessorEditor::timerCallback()
{
    float audioPeak = 0.0f;
    std::array<float, SubdrumProcessorAudioProcessor::visualizerBufferSize> bufferData;
    audioProcessor.getVisualizerData(bufferData.data());

    for (auto val : bufferData)
        audioPeak = std::max(audioPeak, val);

    liveVisualizerPeak = std::max(audioPeak, liveVisualizerPeak * 0.88f);

    for (int i = 0; i < numVisualizerCols; ++i)
    {
        const float decayFactor = std::exp(-static_cast<float>(i) * 0.26f);
        const float targetHeight = liveVisualizerPeak * decayFactor;
        visualizerBarHeights[i] = visualizerBarHeights[i] * 0.72f + targetHeight * 0.28f;
    }

    if (presetComboBox.getSelectedId() != audioProcessor.getCurrentProgram() + 1)
        presetComboBox.setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);

    // Sequencer Play Button State & Dynamic Steps
    if (audioProcessor.getSequencer().isPlaying())
    {
        playButton.setButtonText("■ STOP");
        playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFF3B30));
        playButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFFFFFF));
    }
    else
    {
        playButton.setButtonText("▶ PLAY");
        playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF14161A));
        playButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFEDEDF0));
    }

    updateSequencerButtonColours();
    repaint();
}

void SubdrumProcessorAudioProcessorEditor::paint(juce::Graphics& g)
{
    // 1. Light Industrial Canvas Background (#E2E4E8)
    g.fillAll(juce::Colour(0xFFE2E4E8));

    // 2. Full Dot-Matrix Grid Background
    g.setColour(juce::Colour(0xFFB8BBC2));
    const float dotSpacing = 12.0f;
    for (float x = 6.0f; x < static_cast<float>(getWidth()); x += dotSpacing)
    {
        for (float y = 6.0f; y < static_cast<float>(getHeight()); y += dotSpacing)
        {
            g.fillEllipse(x - 0.75f, y - 0.75f, 1.5f, 1.5f);
        }
    }

    // 3. Header Branding
    g.setColour(juce::Colour(0xFF101216));
    g.setFont(juce::FontOptions(25.0f, juce::Font::bold));
    g.drawText("Subdrum", 24, 12, 160, 26, juce::Justification::left);

    g.setColour(juce::Colour(0xFF7E828C));
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("Underground DSP", 24, 38, 200, 20, juce::Justification::left);

    // Label for Presets
    g.setColour(juce::Colour(0xFF101216));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("PRESET:", getWidth() - 310, 22, 60, 24, juce::Justification::right);

    // 4. Upper Scope / Dot-Matrix Visualizer Screen Card
    auto screenBounds = juce::Rectangle<float>(20.0f, 62.0f, static_cast<float>(getWidth() - 40), 96.0f);
    g.setColour(juce::Colour(0xFFEBEDF1));
    g.fillRoundedRectangle(screenBounds, 10.0f);

    g.setColour(juce::Colour(0xFFD2D5DC));
    g.drawRoundedRectangle(screenBounds, 10.0f, 1.2f);

    // Active Sound Status Banner
    g.setColour(juce::Colour(0xFF5A5E68));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("SYNTHESIS ENGINES: [ 1. KICK (Active) | 2. SNARE/CLAP (Active) | 3-8. EMPTY ]",
               screenBounds.getX() + 18, screenBounds.getY() + 6, screenBounds.getWidth() - 36, 16, juce::Justification::left);

    // Dashed Centerline
    const float centerY = screenBounds.getCentreY() + 6.0f;
    g.setColour(juce::Colour(0xFF787C86));
    for (float x = screenBounds.getX() + 15.0f; x < screenBounds.getRight() - 15.0f; x += 10.0f)
    {
        g.fillRect(x, centerY - 0.75f, 5.0f, 1.5f);
    }

    // Render Waveform / Envelope as Bold Dot-Matrix Columns
    const float startX = screenBounds.getX() + 32.0f;
    const float colGap = 13.0f;
    const float dotSize = 4.0f;
    const float dotStepY = 5.5f;

    for (int col = 0; col < numVisualizerCols; ++col)
    {
        const float colX = startX + col * colGap;
        if (colX + dotSize > screenBounds.getRight() - 25.0f)
            break;

        const float idleFactor = std::exp(-static_cast<float>(col) * 0.22f);
        const float liveHeightNorm = juce::jlimit(0.05f, 1.0f, idleFactor * 0.35f + visualizerBarHeights[col] * 0.85f);
        const int numDotsHalf = static_cast<int>(liveHeightNorm * 6.0f) + 1;

        // Active Upper/Lower Bold Black Dots
        g.setColour(juce::Colour(0xFF121417));
        for (int d = 0; d < numDotsHalf; ++d)
        {
            const float yUp = centerY - (d * dotStepY) - 3.0f;
            const float yDown = centerY + (d * dotStepY) + 3.0f;

            if (yUp >= screenBounds.getY() + 16.0f)
                g.fillEllipse(colX, yUp - dotSize * 0.5f, dotSize, dotSize);

            if (yDown <= screenBounds.getBottom() - 6.0f)
                g.fillEllipse(colX, yDown - dotSize * 0.5f, dotSize, dotSize);
        }

        // Fading Ghost Dots beneath the decay line
        g.setColour(juce::Colour(0xFFB0B4BD));
        for (int d = numDotsHalf; d < numDotsHalf + 2; ++d)
        {
            const float yGhost = centerY + (d * dotStepY) + 3.0f;
            if (yGhost <= screenBounds.getBottom() - 6.0f)
                g.fillEllipse(colX, yGhost - (dotSize - 1.0f) * 0.5f, dotSize - 1.0f, dotSize - 1.0f);
        }
    }

    // 5. Center Sound Voice & Parameter Bank Card
    auto voiceCard = juce::Rectangle<float>(20.0f, 168.0f, static_cast<float>(getWidth() - 40), 185.0f);
    g.setColour(juce::Colour(0xFFEBEDF1));
    g.fillRoundedRectangle(voiceCard, 10.0f);
    g.setColour(juce::Colour(0xFFD2D5DC));
    g.drawRoundedRectangle(voiceCard, 10.0f, 1.2f);

    // 6. TR-808 Style Hardware Sequencer Container Card
    auto seqCard = juce::Rectangle<float>(20.0f, 362.0f, static_cast<float>(getWidth() - 40), 195.0f);
    g.setColour(juce::Colour(0xFF1E2128)); // Dark hardware chassis styling matching TR-808 step bay
    g.fillRoundedRectangle(seqCard, 10.0f);
    g.setColour(juce::Colour(0xFF343842));
    g.drawRoundedRectangle(seqCard, 10.0f, 1.5f);

    g.setColour(juce::Colour(0xFFE2E4E8));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("RHYTHM PROGRAMMER // 16-STEP TR-808 GRID", seqCard.getX() + 16, seqCard.getY() + 10, 360, 16, juce::Justification::left);

    // 7. Bottom Performance Trigger Pads Card
    auto bottomSection = juce::Rectangle<float>(20.0f, static_cast<float>(getHeight() - 110),
                                                static_cast<float>(getWidth() - 40), 95.0f);
    g.setColour(juce::Colour(0xFFEBEDF1));
    g.fillRoundedRectangle(bottomSection, 10.0f);
    g.setColour(juce::Colour(0xFFD2D5DC));
    g.drawRoundedRectangle(bottomSection, 10.0f, 1.2f);

    g.setColour(juce::Colour(0xFF5A5E68));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("VOICE TRIGGER PADS (QWERTY [A] / [S])", bottomSection.getX() + 14, bottomSection.getY() + 6, 280, 14, juce::Justification::left);
}

void SubdrumProcessorAudioProcessorEditor::resized()
{
    // Preset Selector in header
    presetComboBox.setBounds(getWidth() - 245, 18, 225, 28);

    // Sound Tabs Navigation Row
    const int tabX = 32;
    const int tabY = 178;
    const int tabW = 140;
    const int tabH = 30;

    kickTabButton.setBounds(tabX, tabY, tabW, tabH);
    snareTabButton.setBounds(tabX + tabW + 8, tabY, tabW + 20, tabH);
    masterTabButton.setBounds(tabX + (tabW * 2) + 36, tabY, tabW + 10, tabH);

    // Helper to position rotary dials
    auto placeKnob = [](RotaryControl& ctrl, juce::Rectangle<int> box)
    {
        auto labelBox = box.removeFromBottom(22);
        ctrl.label.setBounds(labelBox);
        ctrl.slider.setBounds(box.reduced(6));
    };

    const int knobsAreaTop = 216;
    const int knobsAreaH = 130;
    const int contentW = getWidth() - 60;

    // KICK TAB (6 Knobs: Pitch Drop, Tune, Sweep, Decay, Punch Click, Drive)
    if (activeTab == SoundTab::Kick)
    {
        const int numKnobs = 6;
        const int knobW = contentW / numKnobs;
        placeKnob(kickPitchKnob, juce::Rectangle<int>(30 + 0 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(kickTuneKnob,  juce::Rectangle<int>(30 + 1 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(kickSweepKnob, juce::Rectangle<int>(30 + 2 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(kickDecayKnob, juce::Rectangle<int>(30 + 3 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(kickPunchKnob, juce::Rectangle<int>(30 + 4 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(kickDriveKnob, juce::Rectangle<int>(30 + 5 * knobW, knobsAreaTop, knobW, knobsAreaH));
    }
    // SNARE / CLAP TAB (6 Knobs: Pitch Drop, Decay, Noise Mix, Crack Snap, Air Sizzle, Body Punch)
    else if (activeTab == SoundTab::SnareClap)
    {
        const int numKnobs = 6;
        const int knobW = contentW / numKnobs;
        placeKnob(snarePitchKnob,  juce::Rectangle<int>(30 + 0 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareDecayKnob,  juce::Rectangle<int>(30 + 1 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareNoiseKnob,  juce::Rectangle<int>(30 + 2 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareSnapKnob,   juce::Rectangle<int>(30 + 3 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareSizzleKnob, juce::Rectangle<int>(30 + 4 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareBodyKnob,   juce::Rectangle<int>(30 + 5 * knobW, knobsAreaTop, knobW, knobsAreaH));
    }
    // MASTER DSP, DUB ECHO & ROOM AMBIENCE TAB (6 Knobs: Room Space, Room Size, Dub Echo, Tape Drive, Master Cutoff, Master Gain)
    else if (activeTab == SoundTab::MasterDSP)
    {
        const int numKnobs = 6;
        const int knobW = contentW / numKnobs;
        placeKnob(roomMixKnob,     juce::Rectangle<int>(30 + 0 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(roomSizeKnob,    juce::Rectangle<int>(30 + 1 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(echoMixKnob,     juce::Rectangle<int>(30 + 2 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(driveKnob,       juce::Rectangle<int>(30 + 3 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(cutoffKnob,      juce::Rectangle<int>(30 + 4 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(outputGainKnob,  juce::Rectangle<int>(30 + 5 * knobW, knobsAreaTop, knobW, knobsAreaH));
    }

    // TR-808 Sequencer Section Layout
    const int seqX = 36;
    const int seqY = 396;

    // Transport Row
    playButton.setBounds(seqX, seqY, 100, 30);
    bpmSlider.setBounds(seqX + 112, seqY, 120, 30);
    defaultPatternButton.setBounds(seqX + 242, seqY, 130, 30);
    clearPatternButton.setBounds(seqX + 382, seqY, 75, 30);

    // TR-808 16 Step Buttons Layout
    const int gridStartX = seqX + 85;
    const int gridW = getWidth() - gridStartX - 40;
    const int stepButtonW = (gridW - (15 * 5)) / 16;
    const int stepButtonH = 38;

    kickTrackLabel.setBounds(seqX, seqY + 44, 75, stepButtonH);
    snareTrackLabel.setBounds(seqX, seqY + 90, 75, stepButtonH);

    kickTrackLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFEDEDF0));
    snareTrackLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFEDEDF0));

    for (int s = 0; s < 16; ++s)
    {
        const int bx = gridStartX + s * (stepButtonW + 5);
        kickStepButtons[s].setBounds(bx, seqY + 44, stepButtonW, stepButtonH);
        snareStepButtons[s].setBounds(bx, seqY + 90, stepButtonW, stepButtonH);
    }

    // Bottom Trigger Pads (8 Pads, strictly 2 active)
    auto bottomArea = juce::Rectangle<int>(32, getHeight() - 84, getWidth() - 64, 60);
    const int numPads = 8;
    const int padW = (bottomArea.getWidth() - ((numPads - 1) * 8)) / numPads;

    kickPad.setBounds(bottomArea.removeFromLeft(padW));
    bottomArea.removeFromLeft(8);
    snarePad.setBounds(bottomArea.removeFromLeft(padW));
    bottomArea.removeFromLeft(8);

    for (size_t i = 0; i < emptyPads.size(); ++i)
    {
        emptyPads[i].setBounds(bottomArea.removeFromLeft(padW));
        if (i < emptyPads.size() - 1)
            bottomArea.removeFromLeft(8);
    }
}
