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
}

SubdrumProcessorAudioProcessor::~SubdrumProcessorAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout SubdrumProcessorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Tape Saturation
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "drive", 1 }, "Tape Drive",
        juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 8.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "tapeMix", 1 }, "Tape Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 2. Sampler Lowpass Filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "cutoff", 1 }, "Filter Cutoff",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.25f), 12500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "resonance", 1 }, "Filter Resonance",
        juce::NormalisableRange<float>(0.1f, 6.0f, 0.05f), 0.707f,
        juce::AudioParameterFloatAttributes().withLabel("Q")));

    // 3. VCA Drum Compressor
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compThreshold", 1 }, "Comp Threshold",
        juce::NormalisableRange<float>(-40.0f, 0.0f, 0.1f), -14.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compRatio", 1 }, "Comp Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compAttack", 1 }, "Comp Attack",
        juce::NormalisableRange<float>(0.1f, 50.0f, 0.1f, 0.35f), 4.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compRelease", 1 }, "Comp Release",
        juce::NormalisableRange<float>(10.0f, 400.0f, 1.0f, 0.4f), 55.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compMakeup", 1 }, "Comp Makeup",
        juce::NormalisableRange<float>(-6.0f, 18.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compMix", 1 }, "Comp Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 4. Vinyl & Dust Noise
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vinylNoise", 1 }, "Vinyl Noise",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 12.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "vinylDust", 1 }, "Dust Crackle",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 5. Output Trim
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outputGain", 1 }, "Output Gain",
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
    return false;
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

int SubdrumProcessorAudioProcessor::getNumPrograms()
{
    return 1;
}

int SubdrumProcessorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SubdrumProcessorAudioProcessor::setCurrentProgram(int) {}

const juce::String SubdrumProcessorAudioProcessor::getProgramName(int)
{
    return {};
}

void SubdrumProcessorAudioProcessor::changeProgramName(int, const juce::String&) {}

void SubdrumProcessorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(getTotalNumOutputChannels())
    };

    tapeSaturation.prepare(spec);
    samplerFilter.prepare(spec);
    compressor.prepare(spec);
    vinylNoise.prepare(spec);

    outputGain.prepare(spec);
    outputGain.setRampDurationSeconds(0.02);
}

void SubdrumProcessorAudioProcessor::releaseResources()
{
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

void SubdrumProcessorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    // 1. Update DSP parameters atomically and lock-free
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

    // 2. Sequential DSP Pipeline
    juce::dsp::AudioBlock<float> audioBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> context(audioBlock);

    // Stage 1: 4x Oversampled Tape Saturation
    tapeSaturation.process(context);

    // Stage 2: Lo-Fi Sampler Resonant Reconstruction Filter
    samplerFilter.process(context);

    // Stage 3: Aggressive VCA Transient Drum Compressor
    compressor.process(context);

    // Stage 4: Vinyl Hiss & Crackle Noise Layer
    vinylNoise.process(context);

    // Stage 5: Master Trim
    outputGain.process(context);
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
