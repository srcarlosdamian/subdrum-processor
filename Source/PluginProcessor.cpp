#include "PluginProcessor.h"
#include "PluginEditor.h"

SubdrumProcessorAudioProcessor::SubdrumProcessorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
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
    loadPreset(0); // Load Spectrogram Signature preset on startup
}

SubdrumProcessorAudioProcessor::~SubdrumProcessorAudioProcessor() = default;

void SubdrumProcessorAudioProcessor::initFactoryPresets()
{
    factoryPresets = {
        {
            "01. Deep Sub-LoFi Crunch (Spectrogram Signature)",
            {
                { "drive", 15.5f },       // Rich saturation creating dense harmonic bed
                { "tapeMix", 100.0f },
                { "cutoff", 7200.0f },     // Exact steep high-frequency roll-off from spectrogram
                { "resonance", 1.65f },    // Reconstruction bump near 7kHz cutoff
                { "compThreshold", -20.0f },// Tightly clamps the 1.1s spike visible in visual
                { "compRatio", 5.5f },
                { "compAttack", 2.0f },    // Ultra-fast transient snap
                { "compRelease", 40.0f },  // Fast recovery pumping the sub tail
                { "compMakeup", 3.5f },
                { "compMix", 100.0f },
                { "vinylNoise", 6.0f },    // Diffuse background texture floor
                { "vinylDust", 15.0f },
                { "outputGain", -1.0f }
            }
        },
        {
            "02. Default Warm & Punchy",
            {
                { "drive", 8.0f },
                { "tapeMix", 100.0f },
                { "cutoff", 14500.0f },
                { "resonance", 0.707f },
                { "compThreshold", -14.0f },
                { "compRatio", 4.0f },
                { "compAttack", 4.0f },
                { "compRelease", 55.0f },
                { "compMakeup", 0.0f },
                { "compMix", 100.0f },
                { "vinylNoise", 0.0f },
                { "vinylDust", 0.0f },
                { "outputGain", 0.0f }
            }
        },
        {
            "03. 2-Step Underground Tape",
            {
                { "drive", 17.0f },
                { "tapeMix", 100.0f },
                { "cutoff", 11500.0f },
                { "resonance", 1.35f },
                { "compThreshold", -18.0f },
                { "compRatio", 6.0f },
                { "compAttack", 2.2f },
                { "compRelease", 45.0f },
                { "compMakeup", 3.0f },
                { "compMix", 100.0f },
                { "vinylNoise", 4.0f },
                { "vinylDust", 18.0f },
                { "outputGain", -1.0f }
            }
        },
        {
            "04. 90s Vintage Sampler Lo-Fi",
            {
                { "drive", 11.0f },
                { "tapeMix", 95.0f },
                { "cutoff", 6400.0f },
                { "resonance", 2.5f },
                { "compThreshold", -16.0f },
                { "compRatio", 4.5f },
                { "compAttack", 5.0f },
                { "compRelease", 70.0f },
                { "compMakeup", 2.0f },
                { "compMix", 100.0f },
                { "vinylNoise", 10.0f },
                { "vinylDust", 30.0f },
                { "outputGain", 0.0f }
            }
        },
        {
            "05. Heavy VCA Drum Glue",
            {
                { "drive", 7.0f },
                { "tapeMix", 80.0f },
                { "cutoff", 16500.0f },
                { "resonance", 0.707f },
                { "compThreshold", -22.0f },
                { "compRatio", 8.0f },
                { "compAttack", 1.2f },
                { "compRelease", 35.0f },
                { "compMakeup", 4.5f },
                { "compMix", 85.0f },
                { "vinylNoise", 0.0f },
                { "vinylDust", 0.0f },
                { "outputGain", -2.0f }
            }
        },
        {
            "06. Grimy Dust & Drive",
            {
                { "drive", 24.0f },
                { "tapeMix", 100.0f },
                { "cutoff", 8800.0f },
                { "resonance", 1.8f },
                { "compThreshold", -15.0f },
                { "compRatio", 5.0f },
                { "compAttack", 3.5f },
                { "compRelease", 50.0f },
                { "compMakeup", 2.5f },
                { "compMix", 100.0f },
                { "vinylNoise", 18.0f },
                { "vinylDust", 45.0f },
                { "outputGain", -1.5f }
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

    // 1. Tape Saturation
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 15.5f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "tapeMix", 1 }, "Drive Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 2. Sampler Lowpass Filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "cutoff", 1 }, "Filter",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.25f), 7200.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "resonance", 1 }, "Resonance",
        juce::NormalisableRange<float>(0.1f, 6.0f, 0.05f), 1.65f,
        juce::AudioParameterFloatAttributes().withLabel("Q")));

    // 3. VCA Drum Compressor
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compThreshold", 1 }, "Threshold",
        juce::NormalisableRange<float>(-40.0f, 0.0f, 0.1f), -20.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compRatio", 1 }, "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 5.5f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compAttack", 1 }, "Attack",
        juce::NormalisableRange<float>(0.1f, 50.0f, 0.1f, 0.35f), 2.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compRelease", 1 }, "Decay",
        juce::NormalisableRange<float>(10.0f, 400.0f, 1.0f, 0.4f), 40.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compMakeup", 1 }, "Volume",
        juce::NormalisableRange<float>(-6.0f, 18.0f, 0.1f), 3.5f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compMix", 1 }, "Comp Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 4. Vinyl & Dust Noise
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vinylNoise", 1 }, "Dust Hiss",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 6.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vinylDust", 1 }, "Dust Crackle",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 15.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 5. Output Trim
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outputGain", 1 }, "Master",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), -1.0f,
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

void SubdrumProcessorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(getTotalNumOutputChannels())
    };

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

    // 1. Process Virtual/Computer Keyboard MIDI messages
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // 2. Synthesize internal drum hits directly into buffer
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
