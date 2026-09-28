#include "PluginProcessor.h"
#include "PluginEditor.h"

SubdrumProcessorAudioProcessorEditor::SubdrumProcessorAudioProcessorEditor(SubdrumProcessorAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
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

    // Load Sample Button Setup
    loadSampleButton.setButtonText("+ LOAD WAV");
    loadSampleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF14161A));
    loadSampleButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFEDEDF0));
    loadSampleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFF3B30));
    loadSampleButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFFFFFFFF));
    loadSampleButton.setWantsKeyboardFocus(false);
    loadSampleButton.onClick = [this]() { openSampleFileDialog(); };
    addAndMakeVisible(loadSampleButton);

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

    defaultPatternButton.setButtonText("DEFAULT 2-STEP");
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
    kickTrackLabel.setText("KICK", juce::dontSendNotification);
    kickTrackLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    kickTrackLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF14161A));
    addAndMakeVisible(kickTrackLabel);

    snareTrackLabel.setText("SNARE", juce::dontSendNotification);
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

    updateSequencerButtonColours();

    // Row 1 Controls
    setupControl(driveKnob,       "drive",         "Drive");
    setupControl(tapeMixKnob,     "tapeMix",       "Mix");
    setupControl(cutoffKnob,      "cutoff",        "Filter");
    setupControl(resonanceKnob,   "resonance",     "Reso");

    // Row 2 Controls
    setupControl(compThreshKnob,  "compThreshold", "Thresh");
    setupControl(compAttackKnob,  "compAttack",    "Attack");
    setupControl(compReleaseKnob, "compRelease",   "Decay");
    setupControl(compMakeupKnob,  "compMakeup",    "Volume");
    setupControl(vinylNoiseKnob,  "vinylNoise",    "Dust");
    setupControl(outputGainKnob,  "outputGain",    "Master");

    // Setup Interactive Performance Pads (Strictly 2 Active Sounds)
    setupPad(kickPad,  "1. KICK\n[ A ]",  36, juce::Colour(0xFFB5A895), juce::Colour(0xFF14161A)); // Taupe
    setupPad(snarePad, "2. SNARE\n[ S ]", 38, juce::Colour(0xFF9D9BFF), juce::Colour(0xFF14161A)); // Periwinkle

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

    // Virtual Keyboard Setup
    keyboardComponent.setAvailableRange(36, 72);
    keyboardComponent.setOctaveForMiddleC(3);
    keyboardComponent.setKeyWidth(26.0f);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xFFF0F2F5));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xFF181A1E));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xFFFF3B30));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x33FF3B30));
    keyboardComponent.setWantsKeyboardFocus(false);
    addAndMakeVisible(keyboardComponent);

    setWantsKeyboardFocus(true);
    addKeyListener(this);

    visualizerBarHeights.fill(0.0f);

    setSize(960, 780);
    startTimerHz(30);
}

SubdrumProcessorAudioProcessorEditor::~SubdrumProcessorAudioProcessorEditor()
{
    removeKeyListener(this);
    setLookAndFeel(nullptr);
}

void SubdrumProcessorAudioProcessorEditor::updateSequencerButtonColours()
{
    const int curStep = audioProcessor.getSequencer().getCurrentStep();
    const bool isPlaying = audioProcessor.getSequencer().isPlaying();

    for (int s = 0; s < 16; ++s)
    {
        const int beatGroup = s / 4;
        const bool isCursor = (isPlaying && curStep == s);

        // Track 0: KICK
        const bool kickActive = audioProcessor.getSequencer().getStep(0, s);
        juce::Colour kickOnCol = (beatGroup % 2 == 0) ? juce::Colour(0xFFFF3B30) : juce::Colour(0xFFFF9F0A);
        juce::Colour kickOffCol = isCursor ? juce::Colour(0xFF4A4E58) : juce::Colour(0xFF262930);

        kickStepButtons[s].setColour(juce::TextButton::buttonColourId, kickActive ? (isCursor ? juce::Colour(0xFFFFFFFF) : kickOnCol) : kickOffCol);
        kickStepButtons[s].setColour(juce::TextButton::textColourOffId, kickActive ? (isCursor ? juce::Colour(0xFF14161A) : juce::Colour(0xFFFFFFFF)) : juce::Colour(0xFF8A8E98));

        // Track 1: SNARE
        const bool snareActive = audioProcessor.getSequencer().getStep(1, s);
        juce::Colour snareOnCol = (beatGroup % 2 == 0) ? juce::Colour(0xFF9D9BFF) : juce::Colour(0xFF5AC8FA);
        juce::Colour snareOffCol = isCursor ? juce::Colour(0xFF4A4E58) : juce::Colour(0xFF262930);

        snareStepButtons[s].setColour(juce::TextButton::buttonColourId, snareActive ? (isCursor ? juce::Colour(0xFFFFFFFF) : snareOnCol) : snareOffCol);
        snareStepButtons[s].setColour(juce::TextButton::textColourOffId, snareActive ? (isCursor ? juce::Colour(0xFF14161A) : juce::Colour(0xFF14161A)) : juce::Colour(0xFF8A8E98));
    }
}

void SubdrumProcessorAudioProcessorEditor::openSampleFileDialog()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select Drum Sample (.wav, .aif, .flac)",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        auto result = chooser.getResult();
        if (result.existsAsFile())
        {
            if (audioProcessor.loadSampleFile(result))
            {
                triggerDrumVoice(36);
                repaint();
            }
        }
    });
}

bool SubdrumProcessorAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        juce::String ext = juce::File(file).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".mp3" || ext == ".ogg")
            return true;
    }
    return false;
}

void SubdrumProcessorAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    isDraggingFile = true;
    repaint();
}

void SubdrumProcessorAudioProcessorEditor::fileDragExit(const juce::StringArray&)
{
    isDraggingFile = false;
    repaint();
}

void SubdrumProcessorAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    isDraggingFile = false;
    for (const auto& file : files)
    {
        juce::File f(file);
        if (f.existsAsFile() && isInterestedInFileDrag({ file }))
        {
            if (audioProcessor.loadSampleFile(f))
            {
                triggerDrumVoice(36);
                repaint();
                break;
            }
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
    control.label.setFont(juce::FontOptions(13.0f, juce::Font::bold));
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
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText("Subdrum", 24, 12, 160, 26, juce::Justification::left);

    g.setColour(juce::Colour(0xFF7E828C));
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("Underground DSP", 24, 38, 200, 20, juce::Justification::left);

    // Label for Presets
    g.setColour(juce::Colour(0xFF101216));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("PRESET:", getWidth() - 440, 22, 60, 24, juce::Justification::right);

    // 4. Upper Scope / Dot-Matrix Visualizer Screen Card
    auto screenBounds = juce::Rectangle<float>(20.0f, 62.0f, static_cast<float>(getWidth() - 40), 120.0f);
    g.setColour(juce::Colour(0xFFEBEDF1));
    g.fillRoundedRectangle(screenBounds, 12.0f);

    g.setColour(juce::Colour(0xFFD2D5DC));
    g.drawRoundedRectangle(screenBounds, 12.0f, 1.2f);

    // Sample Info Banner on top of the screen
    g.setColour(juce::Colour(0xFF5A5E68));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    juce::String sampleText = "AUDIO SOURCE:  " + audioProcessor.getLoadedSampleFileName();
    g.drawText(sampleText, screenBounds.getX() + 18, screenBounds.getY() + 6, screenBounds.getWidth() - 36, 16, juce::Justification::left);

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
    const float dotSize = 4.5f;
    const float dotStepY = 6.5f;

    for (int col = 0; col < numVisualizerCols; ++col)
    {
        const float colX = startX + col * colGap;
        if (colX + dotSize > screenBounds.getRight() - 25.0f)
            break;

        const float idleFactor = std::exp(-static_cast<float>(col) * 0.22f);
        const float liveHeightNorm = juce::jlimit(0.05f, 1.0f, idleFactor * 0.4f + visualizerBarHeights[col] * 0.85f);
        const int numDotsHalf = static_cast<int>(liveHeightNorm * 7.0f) + 1;

        // Active Upper/Lower Bold Black Dots
        g.setColour(juce::Colour(0xFF121417));
        for (int d = 0; d < numDotsHalf; ++d)
        {
            const float yUp = centerY - (d * dotStepY) - 3.5f;
            const float yDown = centerY + (d * dotStepY) + 3.5f;

            if (yUp >= screenBounds.getY() + 20.0f)
                g.fillEllipse(colX, yUp - dotSize * 0.5f, dotSize, dotSize);

            if (yDown <= screenBounds.getBottom() - 8.0f)
                g.fillEllipse(colX, yDown - dotSize * 0.5f, dotSize, dotSize);
        }

        // Fading Ghost Dots beneath the decay line
        g.setColour(juce::Colour(0xFFB0B4BD));
        for (int d = numDotsHalf; d < numDotsHalf + 2; ++d)
        {
            const float yGhost = centerY + (d * dotStepY) + 3.5f;
            if (yGhost <= screenBounds.getBottom() - 8.0f)
                g.fillEllipse(colX, yGhost - (dotSize - 1.0f) * 0.5f, dotSize - 1.0f, dotSize - 1.0f);
        }
    }

    // Drag & Drop Active Overlay
    if (isDraggingFile)
    {
        g.setColour(juce::Colour(0xF014161A));
        g.fillRoundedRectangle(screenBounds, 12.0f);

        g.setColour(juce::Colour(0xFFFF3B30));
        g.drawRoundedRectangle(screenBounds.reduced(4.0f), 10.0f, 2.5f);

        g.setColour(juce::Colour(0xFFFFFFFF));
        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.drawText("DROP .WAV / AUDIO FILE HERE", screenBounds, juce::Justification::centred);
    }

    // 5. TR-808 Style Sequencer Container Card
    auto seqCard = juce::Rectangle<float>(20.0f, 192.0f, static_cast<float>(getWidth() - 40), 140.0f);
    g.setColour(juce::Colour(0xFFEBEDF1));
    g.fillRoundedRectangle(seqCard, 12.0f);
    g.setColour(juce::Colour(0xFFD2D5DC));
    g.drawRoundedRectangle(seqCard, 12.0f, 1.2f);

    g.setColour(juce::Colour(0xFF5A5E68));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("16-STEP PATTERN SEQUENCER // TR-808 STYLE", seqCard.getX() + 16, seqCard.getY() + 8, 300, 16, juce::Justification::left);

    // Bottom Pad Section Container Card
    auto bottomSection = juce::Rectangle<float>(20.0f, static_cast<float>(getHeight() - 130),
                                                static_cast<float>(getWidth() - 40), 115.0f);
    g.setColour(juce::Colour(0xFFEBEDF1));
    g.fillRoundedRectangle(bottomSection, 12.0f);
    g.setColour(juce::Colour(0xFFD2D5DC));
    g.drawRoundedRectangle(bottomSection, 12.0f, 1.2f);
}

void SubdrumProcessorAudioProcessorEditor::resized()
{
    // Preset Selector and Load WAV Button in header
    presetComboBox.setBounds(getWidth() - 370, 18, 205, 28);
    loadSampleButton.setBounds(getWidth() - 155, 18, 135, 28);

    // Sequencer Layout
    const int seqX = 32;
    const int seqY = 222;

    // Transport Row
    playButton.setBounds(seqX, seqY, 95, 26);
    bpmSlider.setBounds(seqX + 105, seqY, 120, 26);
    defaultPatternButton.setBounds(seqX + 235, seqY, 130, 26);
    clearPatternButton.setBounds(seqX + 375, seqY, 70, 26);

    // Step Grid
    const int gridStartX = seqX + 80;
    const int gridW = getWidth() - gridStartX - 40;
    const int stepButtonW = (gridW - (15 * 4)) / 16;
    const int stepButtonH = 26;

    kickTrackLabel.setBounds(seqX, seqY + 36, 70, stepButtonH);
    snareTrackLabel.setBounds(seqX, seqY + 68, 70, stepButtonH);

    for (int s = 0; s < 16; ++s)
    {
        const int bx = gridStartX + s * (stepButtonW + 4);
        kickStepButtons[s].setBounds(bx, seqY + 36, stepButtonW, stepButtonH);
        snareStepButtons[s].setBounds(bx, seqY + 68, stepButtonW, stepButtonH);
    }

    // DSP Knobs (Row 1 & Row 2)
    const int knobsAreaTop = 345;
    const int rowHeight = 135;

    auto placeKnob = [](RotaryControl& ctrl, juce::Rectangle<int> box)
    {
        auto labelBox = box.removeFromBottom(22);
        ctrl.label.setBounds(labelBox);
        ctrl.slider.setBounds(box.reduced(6));
    };

    // Row 1: 4 Knobs (Drive, Mix, Filter, Reso)
    const int row1Y = knobsAreaTop;
    const int numRow1 = 4;
    const int knobWidth1 = (getWidth() - 40) / numRow1;

    placeKnob(driveKnob,     juce::Rectangle<int>(20 + 0 * knobWidth1, row1Y, knobWidth1, rowHeight));
    placeKnob(tapeMixKnob,   juce::Rectangle<int>(20 + 1 * knobWidth1, row1Y, knobWidth1, rowHeight));
    placeKnob(cutoffKnob,    juce::Rectangle<int>(20 + 2 * knobWidth1, row1Y, knobWidth1, rowHeight));
    placeKnob(resonanceKnob, juce::Rectangle<int>(20 + 3 * knobWidth1, row1Y, knobWidth1, rowHeight));

    // Row 2: 6 Knobs (Thresh, Attack, Decay, Volume, Dust, Master)
    const int row2Y = knobsAreaTop + rowHeight + 8;
    const int numRow2 = 6;
    const int knobWidth2 = (getWidth() - 40) / numRow2;

    placeKnob(compThreshKnob,  juce::Rectangle<int>(20 + 0 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(compAttackKnob,  juce::Rectangle<int>(20 + 1 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(compReleaseKnob, juce::Rectangle<int>(20 + 2 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(compMakeupKnob,  juce::Rectangle<int>(20 + 3 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(vinylNoiseKnob,  juce::Rectangle<int>(20 + 4 * knobWidth2, row2Y, knobWidth2, rowHeight));
    placeKnob(outputGainKnob,  juce::Rectangle<int>(20 + 5 * knobWidth2, row2Y, knobWidth2, rowHeight));

    // Bottom Squircles & Keyboard (8 Drum Pads)
    auto bottomArea = juce::Rectangle<int>(20, getHeight() - 130, getWidth() - 40, 115).reduced(10);
    auto padsRow = bottomArea.removeFromTop(40);
    const int numPads = 8;
    const int padW = (padsRow.getWidth() - ((numPads - 1) * 6)) / numPads;

    kickPad.setBounds(padsRow.removeFromLeft(padW));
    padsRow.removeFromLeft(6);
    snarePad.setBounds(padsRow.removeFromLeft(padW));
    padsRow.removeFromLeft(6);

    for (size_t i = 0; i < emptyPads.size(); ++i)
    {
        emptyPads[i].setBounds(padsRow.removeFromLeft(padW));
        if (i < emptyPads.size() - 1)
            padsRow.removeFromLeft(6);
    }

    bottomArea.removeFromTop(6);
    keyboardComponent.setBounds(bottomArea);
}
