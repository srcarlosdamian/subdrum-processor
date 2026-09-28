#include "PluginProcessor.h"
#include "PluginEditor.h"

SubdrumProcessorAudioProcessor::SubdrumProcessorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    formatManager.registerBasicFormats();

    // Kick Parameters
    kickTuneParam      = apvts.getRawParameterValue("kickTune");
    kickSweepParam     = apvts.getRawParameterValue("kickSweep");
    kickDecayParam     = apvts.getRawParameterValue("kickDecay");
    kickPunchParam     = apvts.getRawParameterValue("kickPunch");
    kickDriveParam     = apvts.getRawParameterValue("kickDrive");

    // Snare / 2-Step Clap Parameters
    snareDecayParam    = apvts.getRawParameterValue("snareDecay");
    snareNoiseParam    = apvts.getRawParameterValue("snareNoise");
    snareToneParam     = apvts.getRawParameterValue("snareTone");
    snareBrightParam   = apvts.getRawParameterValue("snareBright");
    snareBodyParam     = apvts.getRawParameterValue("snareBody");
    snareFlamParam     = apvts.getRawParameterValue("snareFlam");

    // Master DSP Parameters
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
            "uk_2step_default",
            {
                { "kickTune", 62.0f },
                { "kickSweep", 85.0f },
                { "kickDecay", 75.0f },
                { "kickPunch", 80.0f },
                { "kickDrive", 50.0f },

                { "snareDecay", 45.0f },
                { "snareNoise", 90.0f },
                { "snareTone", 7500.0f },
                { "snareBright", 6200.0f },
                { "snareBody", 0.0f },
                { "snareFlam", 7.5f },

                { "drive", 12.0f },
                { "tapeMix", 100.0f },
                { "cutoff", 6500.0f },
                { "resonance", 1.15f },
                { "compThreshold", -14.0f },
                { "compRatio", 4.0f },
                { "compAttack", 1.5f },
                { "compRelease", 35.0f },
                { "compMakeup", 2.0f },
                { "compMix", 100.0f },
                { "vinylNoise", 0.0f },
                { "vinylDust", 0.0f },
                { "outputGain", 0.0f }
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

juce::AudioProcessorValueTreeState::ParameterLayout SubdrumProcessorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // --- 1. KICK ENGINE PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickTune", 1 }, "Kick Tune",
        juce::NormalisableRange<float>(45.0f, 95.0f, 0.5f), 62.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickSweep", 1 }, "Pitch Drop",
        juce::NormalisableRange<float>(20.0f, 200.0f, 1.0f), 85.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickDecay", 1 }, "Kick Decay",
        juce::NormalisableRange<float>(30.0f, 300.0f, 1.0f), 75.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickPunch", 1 }, "Punch Click",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 80.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickDrive", 1 }, "Kick Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- 2. 2-STEP SNARE / CLAP PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "snareDecay", 1 }, "Snare Decay",
        juce::NormalisableRange<float>(15.0f, 250.0f, 1.0f, 0.4f), 45.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "snareNoise", 1 }, "Noise Level",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 90.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "snareTone", 1 }, "Noise Tone",
        juce::NormalisableRange<float>(1000.0f, 15000.0f, 10.0f, 0.35f), 7500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "snareBright", 1 }, "Brightness",
        juce::NormalisableRange<float>(2000.0f, 16000.0f, 10.0f, 0.35f), 6200.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "snareBody", 1 }, "Wood Tone",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "snareFlam", 1 }, "Clap Flam",
        juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 7.5f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // --- 3. MASTER DSP PARAMETERS ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 12.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "tapeMix", 1 }, "Tape Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "cutoff", 1 }, "Master Filter",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.25f), 6500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "resonance", 1 }, "Resonance",
        juce::NormalisableRange<float>(0.1f, 6.0f, 0.05f), 1.15f,
        juce::AudioParameterFloatAttributes().withLabel("Q")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compThreshold", 1 }, "Threshold",
        juce::NormalisableRange<float>(-40.0f, 0.0f, 0.1f), -14.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compRatio", 1 }, "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compAttack", 1 }, "Attack",
        juce::NormalisableRange<float>(0.1f, 50.0f, 0.1f, 0.35f), 1.5f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compRelease", 1 }, "Decay",
        juce::NormalisableRange<float>(10.0f, 400.0f, 1.0f, 0.4f), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compMakeup", 1 }, "Volume",
        juce::NormalisableRange<float>(-6.0f, 18.0f, 0.1f), 2.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compMix", 1 }, "Comp Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vinylNoise", 1 }, "Dust Hiss",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vinylDust", 1 }, "Dust Crackle",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outputGain", 1 }, "Master",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

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
    tapeSaturation.prepare(spec);
    samplerFilter.prepare(spec);
    compressor.prepare(spec);
    vinylNoise.prepare(spec);

    outputGain.prepare(spec);
    outputGain.setRampDurationSeconds(0.02);
}

void SubdrumProcessorAudioProcessor::releaseResources()
{
    stepSequencer.reset();
    samplePlayer.reset();
    drumSynth.reset();
    tapeSaturation.reset();
    samplerFilter.reset();
    compressor.reset();
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

    // 1. Process TR-808 Style Step Sequencer Clock
    double hostBpm = 0.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto posOpt = playHead->getPosition())
        {
            if (posOpt->getBpm().hasValue())
                hostBpm = *posOpt->getBpm();
        }
    }
    stepSequencer.process(midiMessages, numSamples, hostBpm);

    // 2. Process Virtual/Computer Keyboard MIDI messages
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // 3. Update Drum Synth Parameters for Kick & Snare
    drumSynth.setKickTune(kickTuneParam->load(std::memory_order_relaxed));
    drumSynth.setKickPitchSweep(kickSweepParam->load(std::memory_order_relaxed));
    drumSynth.setKickDecay(kickDecayParam->load(std::memory_order_relaxed));
    drumSynth.setKickPunch(kickPunchParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setKickDrive(kickDriveParam->load(std::memory_order_relaxed) * 0.01f);

    drumSynth.setSnareDecay(snareDecayParam->load(std::memory_order_relaxed));
    drumSynth.setSnareNoiseLevel(snareNoiseParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setSnareNoiseTone(snareToneParam->load(std::memory_order_relaxed));
    drumSynth.setSnareBrightness(snareBrightParam->load(std::memory_order_relaxed));
    drumSynth.setSnareBodyLevel(snareBodyParam->load(std::memory_order_relaxed) * 0.01f);
    drumSynth.setSnareFlam(snareFlamParam->load(std::memory_order_relaxed));

    // 4. Synthesize Internal Drum Voices directly
    drumSynth.process(buffer, midiMessages);

    // 3. Update DSP parameters atomically and lock-free
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

    vinylNoise.setAmount(vinylNoiseParam->load(std::memory_order_relaxed) * 0.01f);
    vinylNoise.setDustDensity(vinylDustParam->load(std::memory_order_relaxed) * 0.01f);

    outputGain.setGainDecibels(outputGainParam->load(std::memory_order_relaxed));

    // 4. Sequential DSP Pipeline
    juce::dsp::AudioBlock<float> audioBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> context(audioBlock);

    tapeSaturation.process(context);
    samplerFilter.process(context);
    compressor.process(context);
    vinylNoise.process(context);
    outputGain.process(context);

    // 5. Calculate peak amplitude envelope for visualizer
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

// Creation entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubdrumProcessorAudioProcessor();
}
