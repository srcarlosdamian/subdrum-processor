#include "PluginProcessor.h"
#include "PluginEditor.h"

SubdrumProcessorAudioProcessor::SubdrumProcessorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    formatManager.registerBasicFormats();

    // 1. Kick Parameters
    kickPitchParam     = apvts.getRawParameterValue("kickPitch");
    kickTuneParam      = apvts.getRawParameterValue("kickTune");
    kickSweepParam     = apvts.getRawParameterValue("kickSweep");
    kickDecayParam     = apvts.getRawParameterValue("kickDecay");
    kickPunchParam     = apvts.getRawParameterValue("kickPunch");
    kickDriveParam     = apvts.getRawParameterValue("kickDrive");

    // 2. Snare Parameters
    snarePitchParam    = apvts.getRawParameterValue("snarePitch");
    snareDecayParam    = apvts.getRawParameterValue("snareDecay");
    snareSnapParam     = apvts.getRawParameterValue("snareSnap");
    snareToneParam     = apvts.getRawParameterValue("snareTone");
    snareCrackParam    = apvts.getRawParameterValue("snareCrack");
    snareDriveParam    = apvts.getRawParameterValue("snareDrive");

    // 3. Acoustic Clap Parameters
    clapPitchParam     = apvts.getRawParameterValue("clapPitch");
    clapDecayParam     = apvts.getRawParameterValue("clapDecay");
    clapWoodParam      = apvts.getRawParameterValue("clapWood");
    clapSlapParam      = apvts.getRawParameterValue("clapSlap");
    clapToneParam      = apvts.getRawParameterValue("clapTone");
    clapTailParam      = apvts.getRawParameterValue("clapTail");

    // 4. Closed Hat Parameters
    chatPitchParam     = apvts.getRawParameterValue("chatPitch");
    chatDecayParam     = apvts.getRawParameterValue("chatDecay");
    chatToneParam      = apvts.getRawParameterValue("chatTone");
    chatSizzleParam    = apvts.getRawParameterValue("chatSizzle");
    chatResParam       = apvts.getRawParameterValue("chatRes");
    chatDriveParam     = apvts.getRawParameterValue("chatDrive");

    // 5. Open Hat Parameters
    ohatPitchParam     = apvts.getRawParameterValue("ohatPitch");
    ohatDecayParam     = apvts.getRawParameterValue("ohatDecay");
    ohatToneParam      = apvts.getRawParameterValue("ohatTone");
    ohatSizzleParam    = apvts.getRawParameterValue("ohatSizzle");
    ohatChokeParam     = apvts.getRawParameterValue("ohatChoke");
    ohatDriveParam     = apvts.getRawParameterValue("ohatDrive");

    // 6. Rimshot Parameters
    rimPitchParam      = apvts.getRawParameterValue("rimPitch");
    rimDecayParam      = apvts.getRawParameterValue("rimDecay");
    rimTuneParam       = apvts.getRawParameterValue("rimTune");
    rimSnapParam       = apvts.getRawParameterValue("rimSnap");
    rimToneParam       = apvts.getRawParameterValue("rimTone");
    rimDriveParam      = apvts.getRawParameterValue("rimDrive");

    // 7. Sub 808 Parameters
    subTuneParam       = apvts.getRawParameterValue("subTune");
    subDecayParam      = apvts.getRawParameterValue("subDecay");
    subSweepParam      = apvts.getRawParameterValue("subSweep");
    subDriveParam      = apvts.getRawParameterValue("subDrive");
    subCutoffParam     = apvts.getRawParameterValue("subCutoff");
    subLevelParam      = apvts.getRawParameterValue("subLevel");

    // 8. Shaker & Vinyl Parameters
    shakerAttackParam  = apvts.getRawParameterValue("shakerAttack");
    shakerDecayParam   = apvts.getRawParameterValue("shakerDecay");
    shakerToneParam    = apvts.getRawParameterValue("shakerTone");
    vinylCrackleParam  = apvts.getRawParameterValue("vinylCrackle");
    vinylHissParam     = apvts.getRawParameterValue("vinylHiss");
    shakerDriveParam   = apvts.getRawParameterValue("shakerDrive");

    // Master DSP, Dub Echo & Room Ambience Parameters
    roomMixParam       = apvts.getRawParameterValue("roomMix");
    roomSizeParam      = apvts.getRawParameterValue("roomSize");
    echoTimeParam      = apvts.getRawParameterValue("echoTime");
    echoFeedbackParam  = apvts.getRawParameterValue("echoFeedback");
    echoMixParam       = apvts.getRawParameterValue("echoMix");
    driveParam         = apvts.getRawParameterValue("drive");
    tapeMixParam       = apvts.getRawParameterValue("tapeMix");
    cutoffParam        = apvts.getRawParameterValue("cutoff");
    resonanceParam     = apvts.getRawParameterValue("resonance");
    compThresholdParam = apvts.getRawParameterValue("compThreshold");
    compRatioParam     = apvts.getRawParameterValue("compRatio");
    compAttackParam    = apvts.getRawParameterValue("compAttack");
    compReleaseParam   = apvts.getRawParameterValue("compRelease");
    compMakeupParam    = apvts.getRawParameterValue("compMakeup");
    compMixParam       = apvts.getRawParameterValue("compMix");
    vinylNoiseParam    = apvts.getRawParameterValue("vinylNoise");
    vinylDustParam     = apvts.getRawParameterValue("vinylDust");
    outputGainParam    = apvts.getRawParameterValue("outputGain");

    // Lo-Fi Bits, Downsampler, Ducking & Swing Parameters
    bitDepthParam      = apvts.getRawParameterValue("bitDepth");
    downsampleParam    = apvts.getRawParameterValue("downsample");
    bitMixParam        = apvts.getRawParameterValue("bitMix");
    duckDepthParam     = apvts.getRawParameterValue("duckDepth");
    seqSwingParam      = apvts.getRawParameterValue("seqSwing");

    for (auto& item : visualizerFifo)
        item.store(0.0f, std::memory_order_relaxed);

    initFactoryPresets();
    loadPreset(0);
}

SubdrumProcessorAudioProcessor::~SubdrumProcessorAudioProcessor() = default;

void SubdrumProcessorAudioProcessor::initFactoryPresets()
{
    factoryPresets = {
        {
            "01 // 2-Step Solid Suite",
            {
                { "kickPitch", 0.0f }, { "kickTune", 54.0f }, { "kickSweep", 175.0f }, { "kickDecay", 175.0f }, { "kickPunch", 85.0f }, { "kickDrive", 60.0f },
                { "snarePitch", 0.0f }, { "snareDecay", 180.0f }, { "snareSnap", 85.0f }, { "snareTone", 195.0f }, { "snareCrack", 80.0f }, { "snareDrive", 40.0f },
                { "clapPitch", 0.0f }, { "clapDecay", 160.0f }, { "clapWood", 92.0f }, { "clapSlap", 85.0f }, { "clapTone", 6800.0f }, { "clapTail", 35.0f },
                { "chatPitch", 0.0f }, { "chatDecay", 70.0f }, { "chatTone", 7800.0f }, { "chatSizzle", 65.0f }, { "chatRes", 1.6f }, { "chatDrive", 35.0f },
                { "ohatPitch", 0.0f }, { "ohatDecay", 320.0f }, { "ohatTone", 8500.0f }, { "ohatSizzle", 70.0f }, { "ohatChoke", 100.0f }, { "ohatDrive", 35.0f },
                { "rimPitch", 0.0f }, { "rimDecay", 28.0f }, { "rimTune", 480.0f }, { "rimSnap", 85.0f }, { "rimTone", 6000.0f }, { "rimDrive", 40.0f },
                { "subTune", 42.0f }, { "subDecay", 500.0f }, { "subSweep", 40.0f }, { "subDrive", 45.0f }, { "subCutoff", 320.0f }, { "subLevel", 0.0f },
                { "shakerAttack", 12.0f }, { "shakerDecay", 75.0f }, { "shakerTone", 6200.0f }, { "vinylCrackle", 30.0f }, { "vinylHiss", 20.0f }, { "shakerDrive", 25.0f },
                { "roomMix", 18.0f }, { "roomSize", 45.0f }, { "echoTime", 260.0f }, { "echoFeedback", 40.0f }, { "echoMix", 18.0f },
                { "drive", 12.0f }, { "tapeMix", 100.0f }, { "cutoff", 18000.0f }, { "resonance", 1.0f },
                { "compThreshold", -14.0f }, { "compRatio", 4.0f }, { "compAttack", 1.5f }, { "compRelease", 35.0f }, { "compMakeup", 2.0f }, { "compMix", 100.0f },
                { "vinylNoise", 0.0f }, { "vinylDust", 0.0f }, { "outputGain", 0.0f },
                { "bitDepth", 16.0f }, { "downsample", 1.0f }, { "bitMix", 0.0f }, { "duckDepth", 40.0f }, { "seqSwing", 58.0f }
            }
        },
        {
            "02 // 90s Dusty Vinyl Garage",
            {
                { "kickPitch", -2.0f }, { "kickTune", 58.0f }, { "kickSweep", 90.0f }, { "kickDecay", 95.0f }, { "kickPunch", 85.0f }, { "kickDrive", 65.0f },
                { "snarePitch", -1.0f }, { "snareDecay", 160.0f }, { "snareSnap", 90.0f }, { "snareTone", 185.0f }, { "snareCrack", 85.0f }, { "snareDrive", 55.0f },
                { "clapPitch", -1.0f }, { "clapDecay", 160.0f }, { "clapWood", 90.0f }, { "clapSlap", 80.0f }, { "clapTone", 5800.0f }, { "clapTail", 50.0f },
                { "chatPitch", -1.0f }, { "chatDecay", 40.0f }, { "chatTone", 8200.0f }, { "chatSizzle", 50.0f }, { "chatRes", 2.2f }, { "chatDrive", 45.0f },
                { "ohatPitch", -1.0f }, { "ohatDecay", 280.0f }, { "ohatTone", 7800.0f }, { "ohatSizzle", 65.0f }, { "ohatChoke", 100.0f }, { "ohatDrive", 50.0f },
                { "rimPitch", -1.0f }, { "rimDecay", 25.0f }, { "rimTune", 460.0f }, { "rimSnap", 80.0f }, { "rimTone", 5500.0f }, { "rimDrive", 50.0f },
                { "subTune", 40.0f }, { "subDecay", 550.0f }, { "subSweep", 45.0f }, { "subDrive", 60.0f }, { "subCutoff", 280.0f }, { "subLevel", 1.0f },
                { "shakerAttack", 14.0f }, { "shakerDecay", 85.0f }, { "shakerTone", 5600.0f }, { "vinylCrackle", 45.0f }, { "vinylHiss", 35.0f }, { "shakerDrive", 35.0f },
                { "roomMix", 24.0f }, { "roomSize", 55.0f }, { "echoTime", 320.0f }, { "echoFeedback", 45.0f }, { "echoMix", 22.0f },
                { "drive", 16.0f }, { "tapeMix", 100.0f }, { "cutoff", 14500.0f }, { "resonance", 1.2f },
                { "compThreshold", -18.0f }, { "compRatio", 5.0f }, { "compAttack", 2.0f }, { "compRelease", 45.0f }, { "compMakeup", 3.0f }, { "compMix", 100.0f },
                { "vinylNoise", 35.0f }, { "vinylDust", 40.0f }, { "outputGain", 0.0f },
                { "bitDepth", 16.0f }, { "downsample", 1.0f }, { "bitMix", 0.0f }, { "duckDepth", 50.0f }, { "seqSwing", 62.0f }
            }
        },
        {
            "03 // Dark Dubstep Weight",
            {
                { "kickPitch", -4.0f }, { "kickTune", 48.0f }, { "kickSweep", 120.0f }, { "kickDecay", 110.0f }, { "kickPunch", 90.0f }, { "kickDrive", 80.0f },
                { "snarePitch", -2.0f }, { "snareDecay", 220.0f }, { "snareSnap", 95.0f }, { "snareTone", 175.0f }, { "snareCrack", 90.0f }, { "snareDrive", 70.0f },
                { "clapPitch", -2.0f }, { "clapDecay", 220.0f }, { "clapWood", 95.0f }, { "clapSlap", 95.0f }, { "clapTone", 6200.0f }, { "clapTail", 65.0f },
                { "chatPitch", -2.0f }, { "chatDecay", 50.0f }, { "chatTone", 7500.0f }, { "chatSizzle", 65.0f }, { "chatRes", 2.5f }, { "chatDrive", 60.0f },
                { "ohatPitch", -2.0f }, { "ohatDecay", 380.0f }, { "ohatTone", 7200.0f }, { "ohatSizzle", 80.0f }, { "ohatChoke", 100.0f }, { "ohatDrive", 65.0f },
                { "rimPitch", -2.0f }, { "rimDecay", 35.0f }, { "rimTune", 440.0f }, { "rimSnap", 90.0f }, { "rimTone", 5000.0f }, { "rimDrive", 65.0f },
                { "subTune", 38.0f }, { "subDecay", 700.0f }, { "subSweep", 60.0f }, { "subDrive", 75.0f }, { "subCutoff", 250.0f }, { "subLevel", 2.5f },
                { "shakerAttack", 10.0f }, { "shakerDecay", 95.0f }, { "shakerTone", 5200.0f }, { "vinylCrackle", 30.0f }, { "vinylHiss", 25.0f }, { "shakerDrive", 45.0f },
                { "roomMix", 30.0f }, { "roomSize", 75.0f }, { "echoTime", 375.0f }, { "echoFeedback", 55.0f }, { "echoMix", 28.0f },
                { "drive", 20.0f }, { "tapeMix", 100.0f }, { "cutoff", 16000.0f }, { "resonance", 1.4f },
                { "compThreshold", -20.0f }, { "compRatio", 6.0f }, { "compAttack", 1.0f }, { "compRelease", 60.0f }, { "compMakeup", 4.0f }, { "compMix", 100.0f },
                { "vinylNoise", 15.0f }, { "vinylDust", 20.0f }, { "outputGain", 0.0f },
                { "bitDepth", 16.0f }, { "downsample", 1.0f }, { "bitMix", 0.0f }, { "duckDepth", 60.0f }, { "seqSwing", 54.0f }
            }
        },
        {
            "04 // Lo-Fi 12-Bit Grime Crunch",
            {
                { "kickPitch", 1.0f }, { "kickTune", 62.0f }, { "kickSweep", 85.0f }, { "kickDecay", 65.0f }, { "kickPunch", 80.0f }, { "kickDrive", 70.0f },
                { "snarePitch", 1.0f }, { "snareDecay", 150.0f }, { "snareSnap", 90.0f }, { "snareTone", 210.0f }, { "snareCrack", 85.0f }, { "snareDrive", 60.0f },
                { "clapPitch", 2.0f }, { "clapDecay", 150.0f }, { "clapWood", 80.0f }, { "clapSlap", 85.0f }, { "clapTone", 5200.0f }, { "clapTail", 30.0f },
                { "chatPitch", 2.0f }, { "chatDecay", 35.0f }, { "chatTone", 6800.0f }, { "chatSizzle", 70.0f }, { "chatRes", 2.8f }, { "chatDrive", 65.0f },
                { "ohatPitch", 2.0f }, { "ohatDecay", 240.0f }, { "ohatTone", 6500.0f }, { "ohatSizzle", 75.0f }, { "ohatChoke", 100.0f }, { "ohatDrive", 70.0f },
                { "rimPitch", 1.0f }, { "rimDecay", 22.0f }, { "rimTune", 520.0f }, { "rimSnap", 85.0f }, { "rimTone", 4800.0f }, { "rimDrive", 60.0f },
                { "subTune", 44.0f }, { "subDecay", 450.0f }, { "subSweep", 35.0f }, { "subDrive", 65.0f }, { "subCutoff", 300.0f }, { "subLevel", 1.0f },
                { "shakerAttack", 8.0f }, { "shakerDecay", 65.0f }, { "shakerTone", 4800.0f }, { "vinylCrackle", 40.0f }, { "vinylHiss", 30.0f }, { "shakerDrive", 50.0f },
                { "roomMix", 15.0f }, { "roomSize", 40.0f }, { "echoTime", 220.0f }, { "echoFeedback", 35.0f }, { "echoMix", 15.0f },
                { "drive", 18.0f }, { "tapeMix", 100.0f }, { "cutoff", 12000.0f }, { "resonance", 1.6f },
                { "compThreshold", -16.0f }, { "compRatio", 4.5f }, { "compAttack", 1.5f }, { "compRelease", 30.0f }, { "compMakeup", 2.5f }, { "compMix", 100.0f },
                { "vinylNoise", 25.0f }, { "vinylDust", 30.0f }, { "outputGain", 0.0f },
                { "bitDepth", 12.0f }, { "downsample", 2.0f }, { "bitMix", 100.0f }, { "duckDepth", 45.0f }, { "seqSwing", 60.0f }
            }
        }
    };
}

void SubdrumProcessorAudioProcessor::loadPreset(int index)
{
    if (index >= 0 && index < static_cast<int>(factoryPresets.size()))
    {
        currentProgram = index;
        const auto& preset = factoryPresets[index];

        for (const auto& [paramId, value] : preset.params)
        {
            if (auto* param = apvts.getParameter(paramId))
            {
                const float normalized = param->getNormalisableRange().convertTo0to1(value);
                param->setValueNotifyingHost(normalized);
            }
        }
    }
}

int SubdrumProcessorAudioProcessor::getNumPrograms()
{
    return static_cast<int>(factoryPresets.size());
}

int SubdrumProcessorAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void SubdrumProcessorAudioProcessor::setCurrentProgram(int index)
{
    loadPreset(index);
}

const juce::String SubdrumProcessorAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < static_cast<int>(factoryPresets.size()))
        return factoryPresets[index].name;
    return {};
}

void SubdrumProcessorAudioProcessor::changeProgramName(int, const juce::String&) {}

void SubdrumProcessorAudioProcessor::loadRhythmPreset(int index)
{
    stepSequencer.loadRhythmPreset(static_cast<underground::dsp::StepSequencer::RhythmPreset>(
        juce::jlimit(0, 6, index)));
}

juce::String SubdrumProcessorAudioProcessor::exportPresetAsJson()
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", getProgramName(currentProgram));
    obj->setProperty("format", "SubdrumProcessorPreset");
    obj->setProperty("version", 2);

    auto* paramObj = new juce::DynamicObject();
    for (auto* param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            paramObj->setProperty(p->paramID, p->getValue());
        }
    }
    obj->setProperty("parameters", juce::var(paramObj));

    auto* seqObj = new juce::DynamicObject();
    seqObj->setProperty("bpm", stepSequencer.getBpm());
    seqObj->setProperty("swing", stepSequencer.getSwing());

    juce::Array<juce::var> tracksArray;
    for (int t = 0; t < underground::dsp::StepSequencer::numTracks; ++t)
    {
        juce::Array<juce::var> stepsArray;
        for (int s = 0; s < underground::dsp::StepSequencer::numSteps; ++s)
            stepsArray.add(stepSequencer.getStep(t, s) ? 1 : 0);
        tracksArray.add(stepsArray);
    }
    seqObj->setProperty("tracks", tracksArray);
    obj->setProperty("sequencer", juce::var(seqObj));

    return juce::JSON::toString(juce::var(obj), true);
}

bool SubdrumProcessorAudioProcessor::importPresetFromJson(const juce::String& jsonText)
{
    juce::var parsedJson;
    if (juce::JSON::parse(jsonText, parsedJson).wasOk() && parsedJson.isObject())
    {
        if (auto* obj = parsedJson.getDynamicObject())
        {
            if (obj->hasProperty("parameters"))
            {
                if (auto* paramObj = obj->getProperty("parameters").getDynamicObject())
                {
                    for (const auto& prop : paramObj->getProperties())
                    {
                        if (auto* p = apvts.getParameter(prop.name.toString()))
                            p->setValueNotifyingHost(static_cast<float>(prop.value));
                    }
                }
            }

            if (obj->hasProperty("sequencer"))
            {
                if (auto* seqObj = obj->getProperty("sequencer").getDynamicObject())
                {
                    if (seqObj->hasProperty("bpm"))
                        stepSequencer.setBpm(static_cast<double>(seqObj->getProperty("bpm")));
                    if (seqObj->hasProperty("swing"))
                        stepSequencer.setSwing(static_cast<float>(seqObj->getProperty("swing")));
                    if (seqObj->hasProperty("tracks"))
                    {
                        if (auto* trkArr = seqObj->getProperty("tracks").getArray())
                        {
                            const int numT = std::min(static_cast<int>(trkArr->size()), underground::dsp::StepSequencer::numTracks);
                            for (int t = 0; t < numT; ++t)
                            {
                                if (auto* stpArr = (*trkArr)[t].getArray())
                                {
                                    const int numS = std::min(static_cast<int>(stpArr->size()), underground::dsp::StepSequencer::numSteps);
                                    for (int s = 0; s < numS; ++s)
                                        stepSequencer.setStep(t, s, static_cast<int>((*stpArr)[s]) != 0);
                                }
                            }
                        }
                    }
                }
            }
            return true;
        }
    }
    return false;
}

bool SubdrumProcessorAudioProcessor::exportPresetToFile(const juce::File& targetFile)
{
    const juce::String json = exportPresetAsJson();
    return targetFile.replaceWithText(json);
}

bool SubdrumProcessorAudioProcessor::importPresetFromFile(const juce::File& sourceFile)
{
    if (sourceFile.existsAsFile())
    {
        const juce::String json = sourceFile.loadFileAsString();
        return importPresetFromJson(json);
    }
    return false;
}

juce::AudioProcessorValueTreeState::ParameterLayout SubdrumProcessorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // --- 1. KICK PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "kickPitch", 1 }, "Kick Pitch", juce::NormalisableRange<float>(-24.0f, 12.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("st")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "kickTune", 1 }, "Kick Tune", juce::NormalisableRange<float>(35.0f, 100.0f, 0.5f), 54.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "kickSweep", 1 }, "Kick Pitch Drop", juce::NormalisableRange<float>(0.0f, 250.0f, 1.0f), 175.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "kickDecay", 1 }, "Kick Decay", juce::NormalisableRange<float>(20.0f, 400.0f, 1.0f), 175.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "kickPunch", 1 }, "Kick Punch", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 85.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "kickDrive", 1 }, "Kick Drive", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 60.0f, juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- 2. SNARE PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "snarePitch", 1 }, "Snare Pitch", juce::NormalisableRange<float>(-24.0f, 12.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("st")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "snareDecay", 1 }, "Snare Decay", juce::NormalisableRange<float>(30.0f, 500.0f, 1.0f, 0.4f), 180.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "snareSnap", 1 }, "Snare Wires", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 85.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "snareTone", 1 }, "Snare Body", juce::NormalisableRange<float>(100.0f, 400.0f, 1.0f), 195.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "snareCrack", 1 }, "Snare Rim Crack", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 80.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "snareDrive", 1 }, "Snare Drive", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 40.0f, juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- 3. ACOUSTIC CLAP PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "clapPitch", 1 }, "Clap Pitch", juce::NormalisableRange<float>(-24.0f, 12.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("st")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "clapDecay", 1 }, "Clap Decay", juce::NormalisableRange<float>(30.0f, 500.0f, 1.0f, 0.4f), 160.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "clapWood", 1 }, "Acoustic Wood", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 92.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "clapSlap", 1 }, "Slap Smack", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 85.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "clapTone", 1 }, "Noise Filter", juce::NormalisableRange<float>(1000.0f, 16000.0f, 10.0f, 0.35f), 6800.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "clapTail", 1 }, "Room Tail", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f, juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- 4. CLOSED HAT PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "chatPitch", 1 }, "Hat Pitch", juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("st")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "chatDecay", 1 }, "Hat Decay", juce::NormalisableRange<float>(15.0f, 200.0f, 1.0f), 70.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "chatTone", 1 }, "Hat Cutoff", juce::NormalisableRange<float>(1500.0f, 14000.0f, 10.0f, 0.35f), 7800.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "chatSizzle", 1 }, "Hat Sizzle", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 65.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "chatRes", 1 }, "Hat Resonance", juce::NormalisableRange<float>(0.5f, 5.0f, 0.1f), 1.6f, juce::AudioParameterFloatAttributes().withLabel("Q")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "chatDrive", 1 }, "Hat Drive", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f, juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- 5. OPEN HAT PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "ohatPitch", 1 }, "Open Hat Pitch", juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("st")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "ohatDecay", 1 }, "Open Hat Decay", juce::NormalisableRange<float>(80.0f, 600.0f, 1.0f), 320.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "ohatTone", 1 }, "Open Hat Cutoff", juce::NormalisableRange<float>(3000.0f, 14000.0f, 10.0f, 0.35f), 8500.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "ohatSizzle", 1 }, "Open Hat Sizzle", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 70.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "ohatChoke", 1 }, "Hat Choke Group", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "ohatDrive", 1 }, "Open Hat Drive", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f, juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- 6. RIMSHOT PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "rimPitch", 1 }, "Rim Pitch", juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("st")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "rimDecay", 1 }, "Rim Decay", juce::NormalisableRange<float>(10.0f, 120.0f, 1.0f), 28.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "rimTune", 1 }, "Rim Shell Tone", juce::NormalisableRange<float>(200.0f, 1000.0f, 1.0f), 480.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "rimSnap", 1 }, "Rim Snap", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 85.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "rimTone", 1 }, "Rim Cutoff", juce::NormalisableRange<float>(2000.0f, 12000.0f, 10.0f, 0.35f), 6000.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "rimDrive", 1 }, "Rim Drive", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 40.0f, juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- 7. SUB 808 PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "subTune", 1 }, "Sub Tune", juce::NormalisableRange<float>(30.0f, 85.0f, 0.5f), 42.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "subDecay", 1 }, "Sub Decay", juce::NormalisableRange<float>(100.0f, 1200.0f, 1.0f, 0.4f), 500.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "subSweep", 1 }, "Sub Pitch Drop", juce::NormalisableRange<float>(0.0f, 120.0f, 1.0f), 40.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "subDrive", 1 }, "Sub Saturation", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 45.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "subCutoff", 1 }, "Sub Filter", juce::NormalisableRange<float>(80.0f, 600.0f, 1.0f), 320.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "subLevel", 1 }, "Sub Level", juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));

    // --- 8. SHAKER & VINYL PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "shakerAttack", 1 }, "Shaker Attack", juce::NormalisableRange<float>(2.0f, 40.0f, 0.5f), 12.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "shakerDecay", 1 }, "Shaker Decay", juce::NormalisableRange<float>(20.0f, 250.0f, 1.0f), 75.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "shakerTone", 1 }, "Shaker Filter", juce::NormalisableRange<float>(2000.0f, 10000.0f, 10.0f, 0.35f), 6200.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "vinylCrackle", 1 }, "Vinyl Crackle", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "vinylHiss", 1 }, "Needle Hiss", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "shakerDrive", 1 }, "Shaker Drive", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 25.0f, juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- MASTER DSP, REVERB & TAPE PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "roomMix", 1 }, "Room Space", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 18.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "roomSize", 1 }, "Room Size", juce::NormalisableRange<float>(10.0f, 95.0f, 0.5f), 45.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "echoMix", 1 }, "Dub Echo Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 18.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "echoTime", 1 }, "Echo Time", juce::NormalisableRange<float>(40.0f, 800.0f, 1.0f), 260.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "echoFeedback", 1 }, "Echo Feedback", juce::NormalisableRange<float>(0.0f, 85.0f, 0.1f), 40.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "drive", 1 }, "Drive", juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 12.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "tapeMix", 1 }, "Tape Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "cutoff", 1 }, "Master Filter", juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.25f), 18000.0f, juce::AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "resonance", 1 }, "Resonance", juce::NormalisableRange<float>(0.1f, 6.0f, 0.05f), 1.0f, juce::AudioParameterFloatAttributes().withLabel("Q")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "compThreshold", 1 }, "Threshold", juce::NormalisableRange<float>(-40.0f, 0.0f, 0.1f), -14.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "compRatio", 1 }, "Ratio", juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 4.0f, juce::AudioParameterFloatAttributes().withLabel(":1")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "compAttack", 1 }, "Attack", juce::NormalisableRange<float>(0.1f, 50.0f, 0.1f, 0.35f), 1.5f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "compRelease", 1 }, "Decay", juce::NormalisableRange<float>(10.0f, 400.0f, 1.0f, 0.4f), 35.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "compMakeup", 1 }, "Volume", juce::NormalisableRange<float>(-6.0f, 18.0f, 0.1f), 2.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "compMix", 1 }, "Comp Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "vinylNoise", 1 }, "Dust Hiss", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "vinylDust", 1 }, "Dust Crackle", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "outputGain", 1 }, "Master", juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));

    // --- LO-FI BITS, DUCKING & SWING PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "bitDepth", 1 }, "Bit Depth", juce::NormalisableRange<float>(2.0f, 16.0f, 0.1f), 16.0f, juce::AudioParameterFloatAttributes().withLabel("bits")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "downsample", 1 }, "Downsample", juce::NormalisableRange<float>(1.0f, 16.0f, 0.1f), 1.0f, juce::AudioParameterFloatAttributes().withLabel("x")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "bitMix", 1 }, "Lo-Fi Bits Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "duckDepth", 1 }, "Sidechain Duck", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 40.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "seqSwing", 1 }, "Sequencer Swing", juce::NormalisableRange<float>(50.0f, 75.0f, 0.5f), 58.0f, juce::AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}

const juce::String SubdrumProcessorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SubdrumProcessorAudioProcessor::acceptsMidi() const
{
    return true;
}

bool SubdrumProcessorAudioProcessor::producesMidi() const
{
    return false;
}

bool SubdrumProcessorAudioProcessor::isMidiEffect() const
{
    return false;
}

double SubdrumProcessorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

bool SubdrumProcessorAudioProcessor::loadSampleFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader != nullptr)
    {
        juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(&tempBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        samplePlayer.loadSample(tempBuffer, reader->sampleRate);
        loadedSampleFileName = file.getFileName();
        return true;
    }
    return false;
}

void SubdrumProcessorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(getTotalNumOutputChannels())
    };

    stepSequencer.prepare(sampleRate);
    samplePlayer.prepare(spec);
    drumSynth.prepare(spec);
    bitCrusher.prepare(spec);
    tapeSaturation.prepare(spec);
    samplerFilter.prepare(spec);
    compressor.prepare(spec);
    tapeEcho.prepare(spec);
    roomAmbience.prepare(spec);
    vinylNoise.prepare(spec);

    outputGain.prepare(spec);
    outputGain.setRampDurationSeconds(0.02);
}

void SubdrumProcessorAudioProcessor::releaseResources()
{
    stepSequencer.reset();
    samplePlayer.reset();
    drumSynth.reset();
    bitCrusher.reset();
    tapeSaturation.reset();
    samplerFilter.reset();
    compressor.reset();
    tapeEcho.reset();
    roomAmbience.reset();
    vinylNoise.reset();
}

bool SubdrumProcessorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void SubdrumProcessorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    // 1. Process TR-808 Style Step Sequencer Clock with Host DAW Sync
    double hostBpm = 0.0;
    bool hostPlaying = false;
    double hostPpq = -1.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto posOpt = playHead->getPosition())
        {
            if (posOpt->getBpm().hasValue())
                hostBpm = *posOpt->getBpm();
            if (posOpt->getIsPlaying())
                hostPlaying = true;
            if (posOpt->getPpqPosition().hasValue())
                hostPpq = *posOpt->getPpqPosition();
        }
    }
    stepSequencer.setSwing(seqSwingParam->load(std::memory_order_relaxed));
    stepSequencer.process(midiMessages, numSamples, hostBpm, hostPlaying, hostPpq);

    // 2. Process Virtual/Computer Keyboard MIDI messages
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // 3. Update Drum Synth Parameters for all 8 voices
    // Voice 1: Kick
    drumSynth.setKickPitchSemi(kickPitchParam->load(std::memory_order_relaxed));
    drumSynth.setKickTune(kickTuneParam->load(std::memory_order_relaxed));
    drumSynth.setKickPitchSweep(kickSweepParam->load(std::memory_order_relaxed));
    drumSynth.setKickDecay(kickDecayParam->load(std::memory_order_relaxed));
    drumSynth.setKickPunch(kickPunchParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setKickDrive(kickDriveParam->load(std::memory_order_relaxed) * 0.01f);

    // Voice 2: Snare
    drumSynth.setSnarePitchSemi(snarePitchParam->load(std::memory_order_relaxed));
    drumSynth.setSnareDecay(snareDecayParam->load(std::memory_order_relaxed));
    drumSynth.setSnareSnap(snareSnapParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setSnareTone(snareToneParam->load(std::memory_order_relaxed));
    drumSynth.setSnareCrack(snareCrackParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setSnareDrive(snareDriveParam->load(std::memory_order_relaxed) * 0.01f);

    // Voice 3: Clap
    drumSynth.setClapPitchSemi(clapPitchParam->load(std::memory_order_relaxed));
    drumSynth.setClapDecay(clapDecayParam->load(std::memory_order_relaxed));
    drumSynth.setClapWood(clapWoodParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setClapSlap(clapSlapParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setClapTone(clapToneParam->load(std::memory_order_relaxed));
    drumSynth.setClapTail(clapTailParam->load(std::memory_order_relaxed) * 0.01f);

    // Voice 4: Closed Hat
    drumSynth.setChatPitchSemi(chatPitchParam->load(std::memory_order_relaxed));
    drumSynth.setChatDecay(chatDecayParam->load(std::memory_order_relaxed));
    drumSynth.setChatTone(chatToneParam->load(std::memory_order_relaxed));
    drumSynth.setChatSizzle(chatSizzleParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setChatRes(chatResParam->load(std::memory_order_relaxed));
    drumSynth.setChatDrive(chatDriveParam->load(std::memory_order_relaxed) * 0.01f);

    // Voice 5: Open Hat
    drumSynth.setOhatPitchSemi(ohatPitchParam->load(std::memory_order_relaxed));
    drumSynth.setOhatDecay(ohatDecayParam->load(std::memory_order_relaxed));
    drumSynth.setOhatTone(ohatToneParam->load(std::memory_order_relaxed));
    drumSynth.setOhatSizzle(ohatSizzleParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setOhatChoke(ohatChokeParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setOhatDrive(ohatDriveParam->load(std::memory_order_relaxed) * 0.01f);

    // Voice 6: Rimshot
    drumSynth.setRimPitchSemi(rimPitchParam->load(std::memory_order_relaxed));
    drumSynth.setRimDecay(rimDecayParam->load(std::memory_order_relaxed));
    drumSynth.setRimTune(rimTuneParam->load(std::memory_order_relaxed));
    drumSynth.setRimSnap(rimSnapParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setRimTone(rimToneParam->load(std::memory_order_relaxed));
    drumSynth.setRimDrive(rimDriveParam->load(std::memory_order_relaxed) * 0.01f);

    // Voice 7: Sub 808
    drumSynth.setSubTune(subTuneParam->load(std::memory_order_relaxed));
    drumSynth.setSubDecay(subDecayParam->load(std::memory_order_relaxed));
    drumSynth.setSubSweep(subSweepParam->load(std::memory_order_relaxed));
    drumSynth.setSubDrive(subDriveParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setSubCutoff(subCutoffParam->load(std::memory_order_relaxed));
    drumSynth.setSubLevel(subLevelParam->load(std::memory_order_relaxed));

    // Voice 8: Shaker & Vinyl
    drumSynth.setShakerAttack(shakerAttackParam->load(std::memory_order_relaxed));
    drumSynth.setShakerDecay(shakerDecayParam->load(std::memory_order_relaxed));
    drumSynth.setShakerTone(shakerToneParam->load(std::memory_order_relaxed));
    drumSynth.setVinylCrackle(vinylCrackleParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setVinylHiss(vinylHissParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setShakerDrive(shakerDriveParam->load(std::memory_order_relaxed) * 0.01f);

    // 4. Synthesize Internal Drum Voices directly
    drumSynth.process(buffer, midiMessages);

    // 5. Lo-Fi BitCrusher (SP-1200 / Akai 12-bit / 8-bit grit)
    bitCrusher.setBitDepth(bitDepthParam->load(std::memory_order_relaxed));
    bitCrusher.setDownsample(downsampleParam->load(std::memory_order_relaxed));
    bitCrusher.setMix(bitMixParam->load(std::memory_order_relaxed) * 0.01f);
    bitCrusher.process(buffer);

    // 6. Update DSP parameters atomically and lock-free
    tapeSaturation.setDrive(driveParam->load(std::memory_order_relaxed));
    tapeSaturation.setMix(tapeMixParam->load(std::memory_order_relaxed) * 0.01f);

    samplerFilter.setCutoff(cutoffParam->load(std::memory_order_relaxed));
    samplerFilter.setResonance(resonanceParam->load(std::memory_order_relaxed));

    compressor.setThreshold(compThresholdParam->load(std::memory_order_relaxed));
    compressor.setRatio(compRatioParam->load(std::memory_order_relaxed));
    compressor.setAttack(compAttackParam->load(std::memory_order_relaxed));
    compressor.setRelease(compReleaseParam->load(std::memory_order_relaxed));
    compressor.setMakeupGain(compMakeupParam->load(std::memory_order_relaxed));
    compressor.setMix(compMixParam->load(std::memory_order_relaxed) * 0.01f);

    tapeEcho.setTime(echoTimeParam->load(std::memory_order_relaxed));
    tapeEcho.setFeedback(echoFeedbackParam->load(std::memory_order_relaxed) * 0.01f);
    tapeEcho.setMix(echoMixParam->load(std::memory_order_relaxed) * 0.01f);

    roomAmbience.setMix(roomMixParam->load(std::memory_order_relaxed) * 0.01f);
    roomAmbience.setRoomSize(roomSizeParam->load(std::memory_order_relaxed) * 0.01f);

    vinylNoise.setAmount(vinylNoiseParam->load(std::memory_order_relaxed) * 0.01f);
    vinylNoise.setDustDensity(vinylDustParam->load(std::memory_order_relaxed) * 0.01f);

    outputGain.setGainDecibels(outputGainParam->load(std::memory_order_relaxed));

    // 7. Sequential DSP Pipeline
    juce::dsp::AudioBlock<float> audioBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> context(audioBlock);

    tapeSaturation.process(context);
    samplerFilter.process(context);
    compressor.process(context);
    tapeEcho.process(context);
    roomAmbience.process(context);
    vinylNoise.process(context);

    // 8. Internal Sidechain Ducking (Kick attenuates Reverb / Room / Echo space)
    const float duckDepth = duckDepthParam->load(std::memory_order_relaxed) * 0.01f;
    if (duckDepth > 0.01f)
    {
        const float kickDuck = drumSynth.getKickDuckLevel() * duckDepth;
        const float duckMultiplier = 1.0f - (kickDuck * 0.50f);
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer(ch);
            for (int s = 0; s < numSamples; ++s)
            {
                channelData[s] *= duckMultiplier;
            }
        }
    }

    outputGain.process(context);

    // 9. Calculate peak amplitude envelope for visualizer
    float peakValue = 0.0f;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        const float* readPtr = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            peakValue = std::max(peakValue, std::abs(readPtr[i]));
        }
    }

    visualizerFifo[visualizerWriteIndex].store(peakValue, std::memory_order_relaxed);
    visualizerWriteIndex = (visualizerWriteIndex + 1) % visualizerBufferSize;
}

bool SubdrumProcessorAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SubdrumProcessorAudioProcessor::createEditor()
{
    return new SubdrumProcessorAudioProcessorEditor(*this);
}

void SubdrumProcessorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SubdrumProcessorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubdrumProcessorAudioProcessor();
}
