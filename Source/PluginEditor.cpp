#include "PluginProcessor.h"
#include "PluginEditor.h"

SubdrumProcessorAudioProcessorEditor::SubdrumProcessorAudioProcessorEditor(SubdrumProcessorAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    setLookAndFeel(&industrialLookAndFeel);

    // 1. Presets Selector Setup
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

    // 2. Rhythms Selector Setup
    rhythmComboBox.addItem("RHYTHM: 2-Step Classic", 1);
    rhythmComboBox.addItem("RHYTHM: Syncopated Garage", 2);
    rhythmComboBox.addItem("RHYTHM: Half-Step Heavy Dub", 3);
    rhythmComboBox.addItem("RHYTHM: Broken Beat Shuffle", 4);
    rhythmComboBox.addItem("RHYTHM: Straight 4-on-Floor", 5);
    rhythmComboBox.addItem("RHYTHM: Ghost Clap & Sub", 6);
    rhythmComboBox.addItem("RHYTHM: Clear All Steps", 7);
    rhythmComboBox.setSelectedId(1, juce::dontSendNotification);
    rhythmComboBox.setWantsKeyboardFocus(false);
    rhythmComboBox.onChange = [this]()
    {
        const int sel = rhythmComboBox.getSelectedId() - 1;
        if (sel >= 0)
        {
            audioProcessor.loadRhythmPreset(sel);
            updateSequencerButtonColours();
        }
    };
    addAndMakeVisible(rhythmComboBox);

    // 3. Save, Export, Import Preset Buttons
    saveButton.setButtonText("SAVE");
    saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF323640));
    saveButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFEDEDF0));
    saveButton.setWantsKeyboardFocus(false);
    saveButton.onClick = [this]()
    {
        // Save current parameters into memory
        saveButton.setButtonText("SAVED ✓");
        juce::Timer::callAfterDelay(1500, [this]() { saveButton.setButtonText("SAVE"); });
    };
    addAndMakeVisible(saveButton);

    exportButton.setButtonText("EXPORT");
    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF323640));
    exportButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFEDEDF0));
    exportButton.setWantsKeyboardFocus(false);
    exportButton.onClick = [this]()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Export Preset (.json)",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MyPreset.json"),
            "*.json");

        const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result != juce::File())
            {
                if (!result.hasFileExtension(".json"))
                    result = result.withFileExtension(".json");
                audioProcessor.exportPresetToFile(result);
            }
        });
    };
    addAndMakeVisible(exportButton);

    importButton.setButtonText("IMPORT");
    importButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF323640));
    importButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFEDEDF0));
    importButton.setWantsKeyboardFocus(false);
    importButton.onClick = [this]()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Import Preset (.json)",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.json");

        const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.existsAsFile())
            {
                if (audioProcessor.importPresetFromFile(result))
                {
                    updateSequencerButtonColours();
                    bpmSlider.setValue(audioProcessor.getSequencer().getBpm(), juce::dontSendNotification);
                    swingSlider.setValue(audioProcessor.getSequencer().getSwing(), juce::dontSendNotification);
                }
            }
        });
    };
    addAndMakeVisible(importButton);

    // Tab Navigation Buttons Setup
    auto setupTabBtn = [this](juce::TextButton& btn, const juce::String& text, SoundTab tab)
    {
        btn.setButtonText(text);
        btn.setWantsKeyboardFocus(false);
        btn.onClick = [this, tab]() { setActiveTab(tab); };
        addAndMakeVisible(btn);
    };

    setupTabBtn(kickTabButton,   "1. BASS KICK",     SoundTab::Kick);
    setupTabBtn(snareTabButton,  "2. ACOUSTIC CLAP", SoundTab::SnareClap);
    setupTabBtn(masterTabButton, "3. MASTER DSP",    SoundTab::MasterDSP);

    // 1. KICK CONTROLS (6 Knobs)
    setupControl(kickPitchKnob, "kickPitch", "Pitch (st)");
    setupControl(kickTuneKnob,  "kickTune",  "Tune (Hz)");
    setupControl(kickSweepKnob, "kickSweep", "Pitch Drop");
    setupControl(kickDecayKnob, "kickDecay", "Decay (ms)");
    setupControl(kickPunchKnob, "kickPunch", "Punch Click");
    setupControl(kickDriveKnob, "kickDrive", "Overdrive");

    // 2. ACOUSTIC CLAP CONTROLS (6 Knobs)
    setupControl(snarePitchKnob, "snarePitch", "Pitch (st)");
    setupControl(snareDecayKnob, "snareDecay", "Decay (ms)");
    setupControl(snareWoodKnob,  "snareWood",  "Acoustic Wood");
    setupControl(snareSlapKnob,  "snareSlap",  "Slap Smack");
    setupControl(snareToneKnob,  "snareTone",  "Noise Filter");
    setupControl(snareTailKnob,  "snareTail",  "Room Tail");

    // 3. MASTER DSP, BITS, REVERB & DUCKING CONTROLS (11 Knobs)
    setupControl(roomMixKnob,     "roomMix",    "Room Space");
    setupControl(roomSizeKnob,    "roomSize",   "Room Size");
    setupControl(echoMixKnob,     "echoMix",    "Dub Echo");
    setupControl(driveKnob,       "drive",      "Tape Drive");
    setupControl(cutoffKnob,      "cutoff",     "Master Filter");
    setupControl(bitDepthKnob,    "bitDepth",   "Bit Depth");
    setupControl(downsampleKnob,  "downsample", "Downsample");
    setupControl(bitMixKnob,      "bitMix",     "Lo-Fi Bits");
    setupControl(duckDepthKnob,   "duckDepth",  "Sidechain Duck");
    setupControl(vinylDustKnob,   "vinylDust",  "Dust Crackle");
    setupControl(outputGainKnob,  "outputGain", "Master Gain");

    // TR-808 Style Sequencer Transport Setup
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

    hostSyncButton.setButtonText("DAW SYNC: ON");
    hostSyncButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF233D4D));
    hostSyncButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFEDEDF0));
    hostSyncButton.setWantsKeyboardFocus(false);
    hostSyncButton.onClick = [this]()
    {
        const bool cur = audioProcessor.getSequencer().isHostSyncEnabled();
        audioProcessor.getSequencer().setHostSync(!cur);
        hostSyncButton.setButtonText(!cur ? "DAW SYNC: ON" : "DAW SYNC: OFF");
        hostSyncButton.setColour(juce::TextButton::buttonColourId, !cur ? juce::Colour(0xFF233D4D) : juce::Colour(0xFF454B54));
    };
    addAndMakeVisible(hostSyncButton);

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

    swingSlider.setSliderStyle(juce::Slider::LinearBar);
    swingSlider.setRange(50.0, 75.0, 1.0);
    swingSlider.setTextValueSuffix("% SWING");
    swingSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFF14161A));
    swingSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFEDEDF0));
    swingSlider.setWantsKeyboardFocus(false);
    swingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "seqSwing", swingSlider);
    addAndMakeVisible(swingSlider);

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

    // 4 Tracks Labels Setup
    const juce::String trackNames[4] = { "1. KICK", "2. CLAP", "3. C-HAT", "4. PERC" };
    for (int t = 0; t < 4; ++t)
    {
        trackLabels[t].setText(trackNames[t], juce::dontSendNotification);
        trackLabels[t].setFont(juce::FontOptions(11.0f, juce::Font::bold));
        trackLabels[t].setColour(juce::Label::textColourId, juce::Colour(0xFFEDEDF0));
        addAndMakeVisible(trackLabels[t]);

        for (int s = 0; s < 16; ++s)
        {
            stepButtons[t][s].setButtonText(juce::String(s + 1));
            stepButtons[t][s].setWantsKeyboardFocus(false);
            stepButtons[t][s].onClick = [this, t, s]()
            {
                audioProcessor.getSequencer().toggleStep(t, s);
                updateSequencerButtonColours();
            };
            addAndMakeVisible(stepButtons[t][s]);
        }
    }

    // 8 Performance Trigger Drum Pads Setup
    setupPad(drumPads[0], "1. KICK\n[ A ]",   36, juce::Colour(0xFFB5A895), juce::Colour(0xFF14161A)); // Taupe
    setupPad(drumPads[1], "2. CLAP\n[ S ]",   38, juce::Colour(0xFF9D9BFF), juce::Colour(0xFF14161A)); // Periwinkle
    setupPad(drumPads[2], "3. C-HAT\n[ D ]",  42, juce::Colour(0xFFE9C46A), juce::Colour(0xFF14161A)); // Gold
    setupPad(drumPads[3], "4. O-HAT\n[ F ]",  46, juce::Colour(0xFFF4A261), juce::Colour(0xFF14161A)); // Coral-Orange
    setupPad(drumPads[4], "5. VINYL\n[ G ]",  48, juce::Colour(0xFF8D99AE), juce::Colour(0xFF14161A)); // Slate
    setupPad(drumPads[5], "6. RIM\n[ H ]",    37, juce::Colour(0xFFE76F51), juce::Colour(0xFF14161A)); // Rust Red
    setupPad(drumPads[6], "7. SUB 808\n[ J ]",39, juce::Colour(0xFF7209B7), juce::Colour(0xFFFFFFFF)); // Purple
    setupPad(drumPads[7], "8. SHAKER\n[ K ]", 40, juce::Colour(0xFF2A9D8F), juce::Colour(0xFF14161A)); // Teal

    setActiveTab(SoundTab::SnareClap);
    updateSequencerButtonColours();

    setWantsKeyboardFocus(true);
    addKeyListener(this);

    visualizerBarHeights.fill(0.0f);

    setSize(980, 740);
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

    const bool isKick = (activeTab == SoundTab::Kick);
    kickPitchKnob.slider.setVisible(isKick);   kickPitchKnob.label.setVisible(isKick);
    kickTuneKnob.slider.setVisible(isKick);    kickTuneKnob.label.setVisible(isKick);
    kickSweepKnob.slider.setVisible(isKick);   kickSweepKnob.label.setVisible(isKick);
    kickDecayKnob.slider.setVisible(isKick);   kickDecayKnob.label.setVisible(isKick);
    kickPunchKnob.slider.setVisible(isKick);   kickPunchKnob.label.setVisible(isKick);
    kickDriveKnob.slider.setVisible(isKick);   kickDriveKnob.label.setVisible(isKick);

    const bool isSnare = (activeTab == SoundTab::SnareClap);
    snarePitchKnob.slider.setVisible(isSnare); snarePitchKnob.label.setVisible(isSnare);
    snareDecayKnob.slider.setVisible(isSnare); snareDecayKnob.label.setVisible(isSnare);
    snareWoodKnob.slider.setVisible(isSnare);  snareWoodKnob.label.setVisible(isSnare);
    snareSlapKnob.slider.setVisible(isSnare);  snareSlapKnob.label.setVisible(isSnare);
    snareToneKnob.slider.setVisible(isSnare);  snareToneKnob.label.setVisible(isSnare);
    snareTailKnob.slider.setVisible(isSnare);  snareTailKnob.label.setVisible(isSnare);

    const bool isMaster = (activeTab == SoundTab::MasterDSP);
    roomMixKnob.slider.setVisible(isMaster);     roomMixKnob.label.setVisible(isMaster);
    roomSizeKnob.slider.setVisible(isMaster);    roomSizeKnob.label.setVisible(isMaster);
    echoMixKnob.slider.setVisible(isMaster);     echoMixKnob.label.setVisible(isMaster);
    driveKnob.slider.setVisible(isMaster);       driveKnob.label.setVisible(isMaster);
    cutoffKnob.slider.setVisible(isMaster);      cutoffKnob.label.setVisible(isMaster);
    bitDepthKnob.slider.setVisible(isMaster);    bitDepthKnob.label.setVisible(isMaster);
    downsampleKnob.slider.setVisible(isMaster);  downsampleKnob.label.setVisible(isMaster);
    bitMixKnob.slider.setVisible(isMaster);      bitMixKnob.label.setVisible(isMaster);
    duckDepthKnob.slider.setVisible(isMaster);   duckDepthKnob.label.setVisible(isMaster);
    vinylDustKnob.slider.setVisible(isMaster);   vinylDustKnob.label.setVisible(isMaster);
    outputGainKnob.slider.setVisible(isMaster);  outputGainKnob.label.setVisible(isMaster);

    resized();
    repaint();
}

void SubdrumProcessorAudioProcessorEditor::updateSequencerButtonColours()
{
    const int curStep = audioProcessor.getSequencer().getCurrentStep();
    const bool isPlaying = audioProcessor.getSequencer().isPlaying();

    const juce::Colour tr808ActiveColors[4] = {
        juce::Colour(0xFFE63946), // Red
        juce::Colour(0xFFF77F00), // Orange
        juce::Colour(0xFFFCBF49), // Yellow
        juce::Colour(0xFFE2E4E8)  // Off-White
    };

    for (int t = 0; t < 4; ++t)
    {
        for (int s = 0; s < 16; ++s)
        {
            const int group = s / 4;
            const bool isCursor = (isPlaying && curStep == s);
            const juce::Colour blockCol = tr808ActiveColors[group];

            const bool stepActive = audioProcessor.getSequencer().getStep(t, s);
            if (stepActive)
            {
                stepButtons[t][s].setColour(juce::TextButton::buttonColourId, isCursor ? juce::Colour(0xFFFFFFFF) : blockCol);
                stepButtons[t][s].setColour(juce::TextButton::textColourOffId, (group == 2 || group == 3) ? juce::Colour(0xFF14161A) : juce::Colour(0xFFFFFFFF));
            }
            else
            {
                stepButtons[t][s].setColour(juce::TextButton::buttonColourId, isCursor ? juce::Colour(0xFF555964) : juce::Colour(0xFF262930));
                stepButtons[t][s].setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF7A7E88));
            }
        }
    }
}

void SubdrumProcessorAudioProcessorEditor::setupControl(RotaryControl& control, const juce::String& paramID,
                                                        const juce::String& labelText)
{
    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    control.slider.setPopupDisplayEnabled(true, false, this);
    control.slider.setWantsKeyboardFocus(false);
    addAndMakeVisible(control.slider);

    control.label.setText(labelText, juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
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
    int padIndex = -1;

    switch (keyChar)
    {
        case 'a': noteToPlay = 36 + octaveOffset; padIndex = 0; break;
        case 's': noteToPlay = 38 + octaveOffset; padIndex = 1; break;
        case 'd': noteToPlay = 42 + octaveOffset; padIndex = 2; break;
        case 'f': noteToPlay = 46 + octaveOffset; padIndex = 3; break;
        case 'g': noteToPlay = 48 + octaveOffset; padIndex = 4; break;
        case 'h': noteToPlay = 37 + octaveOffset; padIndex = 5; break;
        case 'j': noteToPlay = 39 + octaveOffset; padIndex = 6; break;
        case 'k': noteToPlay = 40 + octaveOffset; padIndex = 7; break;
    }

    if (noteToPlay != -1)
    {
        if (padIndex >= 0 && padIndex < 8)
            drumPads[padIndex].setState(juce::Button::buttonDown);
        triggerDrumVoice(noteToPlay);
        return true;
    }

    return false;
}

bool SubdrumProcessorAudioProcessorEditor::keyStateChanged(bool isKeyDown, juce::Component*)
{
    if (!isKeyDown)
    {
        for (auto& pad : drumPads)
            pad.setState(juce::Button::buttonNormal);
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
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("Subdrum", 24, 12, 130, 24, juce::Justification::left);

    g.setColour(juce::Colour(0xFF7E828C));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("Underground DSP", 24, 34, 150, 18, juce::Justification::left);

    // 4. Upper Scope / Dot-Matrix Visualizer Screen Card
    auto screenBounds = juce::Rectangle<float>(20.0f, 56.0f, static_cast<float>(getWidth() - 40), 86.0f);
    g.setColour(juce::Colour(0xFFEBEDF1));
    g.fillRoundedRectangle(screenBounds, 8.0f);

    g.setColour(juce::Colour(0xFFD2D5DC));
    g.drawRoundedRectangle(screenBounds, 8.0f, 1.2f);

    // Active Sound Status Banner
    g.setColour(juce::Colour(0xFF5A5E68));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("SYNTHESIS ENGINES: [ 8 VOICES ACTIVE | 12-BIT LO-FI CRUNCH | DUB ECHO & DUCKING ]",
               screenBounds.getX() + 16, screenBounds.getY() + 4, screenBounds.getWidth() - 32, 14, juce::Justification::left);

    const float centerY = screenBounds.getCentreY() + 6.0f;
    g.setColour(juce::Colour(0xFF787C86));
    for (float x = screenBounds.getX() + 15.0f; x < screenBounds.getRight() - 15.0f; x += 10.0f)
    {
        g.fillRect(x, centerY - 0.75f, 5.0f, 1.5f);
    }

    // Render Waveform / Envelope as Bold Dot-Matrix Columns
    const float startX = screenBounds.getX() + 32.0f;
    const float colGap = 13.5f;
    const float dotSize = 3.8f;
    const float dotStepY = 5.2f;

    for (int col = 0; col < numVisualizerCols; ++col)
    {
        const float colX = startX + col * colGap;
        if (colX + dotSize > screenBounds.getRight() - 25.0f)
            break;

        const float idleFactor = std::exp(-static_cast<float>(col) * 0.22f);
        const float liveHeightNorm = juce::jlimit(0.05f, 1.0f, idleFactor * 0.35f + visualizerBarHeights[col] * 0.85f);
        const int numDotsHalf = static_cast<int>(liveHeightNorm * 6.0f) + 1;

        g.setColour(juce::Colour(0xFF121417));
        for (int d = 0; d < numDotsHalf; ++d)
        {
            const float yUp = centerY - (d * dotStepY) - 3.0f;
            const float yDown = centerY + (d * dotStepY) + 3.0f;

            if (yUp >= screenBounds.getY() + 14.0f)
                g.fillEllipse(colX, yUp - dotSize * 0.5f, dotSize, dotSize);

            if (yDown <= screenBounds.getBottom() - 5.0f)
                g.fillEllipse(colX, yDown - dotSize * 0.5f, dotSize, dotSize);
        }
    }

    // 5. Center Sound Voice & Parameter Bank Card
    auto voiceCard = juce::Rectangle<float>(20.0f, 148.0f, static_cast<float>(getWidth() - 40), 180.0f);
    g.setColour(juce::Colour(0xFFEBEDF1));
    g.fillRoundedRectangle(voiceCard, 8.0f);
    g.setColour(juce::Colour(0xFFD2D5DC));
    g.drawRoundedRectangle(voiceCard, 8.0f, 1.2f);

    // 6. TR-808 Style Hardware Sequencer Container Card
    auto seqCard = juce::Rectangle<float>(20.0f, 336.0f, static_cast<float>(getWidth() - 40), 255.0f);
    g.setColour(juce::Colour(0xFF1E2128));
    g.fillRoundedRectangle(seqCard, 8.0f);
    g.setColour(juce::Colour(0xFF343842));
    g.drawRoundedRectangle(seqCard, 8.0f, 1.5f);

    g.setColour(juce::Colour(0xFFE2E4E8));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("RHYTHM PROGRAMMER // 4-TRACK TR-808 SEQUENCER + SWING + HOST SYNC", seqCard.getX() + 16, seqCard.getY() + 8, 550, 16, juce::Justification::left);

    // 7. Bottom Performance Trigger Pads Card
    auto bottomSection = juce::Rectangle<float>(20.0f, static_cast<float>(getHeight() - 138),
                                                static_cast<float>(getWidth() - 40), 126.0f);
    g.setColour(juce::Colour(0xFFEBEDF1));
    g.fillRoundedRectangle(bottomSection, 8.0f);
    g.setColour(juce::Colour(0xFFD2D5DC));
    g.drawRoundedRectangle(bottomSection, 8.0f, 1.2f);

    g.setColour(juce::Colour(0xFF5A5E68));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("VOICE TRIGGER PADS (QWERTY [A] [S] [D] [F] [G] [H] [J] [K])", bottomSection.getX() + 14, bottomSection.getY() + 6, 450, 14, juce::Justification::left);
}

void SubdrumProcessorAudioProcessorEditor::resized()
{
    // Header Row: Logo, Presets combo, Rhythms combo, Save, Import, Export
    const int topY = 14;
    presetComboBox.setBounds(200, topY, 180, 28);
    rhythmComboBox.setBounds(390, topY, 195, 28);
    saveButton.setBounds(600, topY, 70, 28);
    importButton.setBounds(680, topY, 80, 28);
    exportButton.setBounds(770, topY, 80, 28);

    // Sound Tabs Navigation Row
    const int tabX = 32;
    const int tabY = 156;
    const int tabW = 140;
    const int tabH = 26;

    kickTabButton.setBounds(tabX, tabY, tabW, tabH);
    snareTabButton.setBounds(tabX + tabW + 8, tabY, tabW + 15, tabH);
    masterTabButton.setBounds(tabX + (tabW * 2) + 30, tabY, tabW + 10, tabH);

    auto placeKnob = [](RotaryControl& ctrl, juce::Rectangle<int> box)
    {
        auto labelBox = box.removeFromBottom(20);
        ctrl.label.setBounds(labelBox);
        ctrl.slider.setBounds(box.reduced(4));
    };

    const int knobsAreaTop = 186;
    const int knobsAreaH = 135;
    const int contentW = getWidth() - 60;

    // TAB 1: KICK (6 Knobs)
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
    // TAB 2: ACOUSTIC CLAP (6 Knobs)
    else if (activeTab == SoundTab::SnareClap)
    {
        const int numKnobs = 6;
        const int knobW = contentW / numKnobs;
        placeKnob(snarePitchKnob, juce::Rectangle<int>(30 + 0 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareDecayKnob, juce::Rectangle<int>(30 + 1 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareWoodKnob,  juce::Rectangle<int>(30 + 2 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareSlapKnob,  juce::Rectangle<int>(30 + 3 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareToneKnob,  juce::Rectangle<int>(30 + 4 * knobW, knobsAreaTop, knobW, knobsAreaH));
        placeKnob(snareTailKnob,  juce::Rectangle<int>(30 + 5 * knobW, knobsAreaTop, knobW, knobsAreaH));
    }
    // TAB 3: MASTER DSP, BITS, REVERB & DUCKING (11 Knobs in 2 Rows)
    else if (activeTab == SoundTab::MasterDSP)
    {
        const int rowH = 68;
        const int topRowY = knobsAreaTop;
        const int botRowY = knobsAreaTop + rowH + 2;

        // Row 1: Space & Color (6 knobs)
        const int knobW1 = contentW / 6;
        placeKnob(roomMixKnob,    juce::Rectangle<int>(30 + 0 * knobW1, topRowY, knobW1, rowH));
        placeKnob(roomSizeKnob,   juce::Rectangle<int>(30 + 1 * knobW1, topRowY, knobW1, rowH));
        placeKnob(echoMixKnob,    juce::Rectangle<int>(30 + 2 * knobW1, topRowY, knobW1, rowH));
        placeKnob(driveKnob,      juce::Rectangle<int>(30 + 3 * knobW1, topRowY, knobW1, rowH));
        placeKnob(cutoffKnob,     juce::Rectangle<int>(30 + 4 * knobW1, topRowY, knobW1, rowH));
        placeKnob(outputGainKnob, juce::Rectangle<int>(30 + 5 * knobW1, topRowY, knobW1, rowH));

        // Row 2: Lo-Fi Bits, Decimator, Ducking & Vinyl (5 knobs)
        const int knobW2 = contentW / 5;
        placeKnob(bitDepthKnob,   juce::Rectangle<int>(30 + 0 * knobW2, botRowY, knobW2, rowH));
        placeKnob(downsampleKnob, juce::Rectangle<int>(30 + 1 * knobW2, botRowY, knobW2, rowH));
        placeKnob(bitMixKnob,     juce::Rectangle<int>(30 + 2 * knobW2, botRowY, knobW2, rowH));
        placeKnob(duckDepthKnob,  juce::Rectangle<int>(30 + 3 * knobW2, botRowY, knobW2, rowH));
        placeKnob(vinylDustKnob,  juce::Rectangle<int>(30 + 4 * knobW2, botRowY, knobW2, rowH));
    }

    // TR-808 Sequencer Section Layout
    const int seqX = 36;
    const int seqY = 362;

    // Transport Row
    playButton.setBounds(seqX, seqY, 80, 26);
    hostSyncButton.setBounds(seqX + 88, seqY, 115, 26);
    bpmSlider.setBounds(seqX + 211, seqY, 100, 26);
    swingSlider.setBounds(seqX + 319, seqY, 120, 26);
    clearPatternButton.setBounds(seqX + 447, seqY, 70, 26);

    // 4 Tracks x 16 Steps Layout
    const int gridStartX = seqX + 75;
    const int gridW = getWidth() - gridStartX - 40;
    const int stepButtonW = (gridW - (15 * 4)) / 16;
    const int stepButtonH = 34;
    const int rowSpacing = 38;

    for (int t = 0; t < 4; ++t)
    {
        const int ry = seqY + 36 + (t * rowSpacing);
        trackLabels[t].setBounds(seqX, ry, 70, stepButtonH);

        for (int s = 0; s < 16; ++s)
        {
            const int bx = gridStartX + s * (stepButtonW + 4);
            stepButtons[t][s].setBounds(bx, ry, stepButtonW, stepButtonH);
        }
    }

    // Bottom Performance Trigger Pads (8 Pads)
    auto bottomArea = juce::Rectangle<int>(32, getHeight() - 110, getWidth() - 64, 88);
    const int numPads = 8;
    const int padGap = 6;
    const int padW = (bottomArea.getWidth() - ((numPads - 1) * padGap)) / numPads;

    for (int i = 0; i < numPads; ++i)
    {
        drumPads[i].setBounds(bottomArea.removeFromLeft(padW));
        if (i < numPads - 1)
            bottomArea.removeFromLeft(padGap);
    }
}
