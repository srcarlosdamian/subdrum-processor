#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/DrumSynth.h"
#include "DSP/TapeSaturation.h"
#include "DSP/SamplerFilter.h"
#include "DSP/VCACompressor.h"
#include "DSP/VinylNoise.h"

class SubdrumProcessorAudioProcessor : public juce::AudioProcessor
{
public:
    SubdrumProcessorAudioProcessor();
    ~SubdrumProcessorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    float getGainReduction() const noexcept { return compressor.getGainReductionDb(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // DSP Chain
    underground::dsp::DrumSynth drumSynth;
    underground::dsp::TapeSaturation tapeSaturation;
    underground::dsp::SamplerFilter samplerFilter;
    underground::dsp::VCACompressor compressor;
    underground::dsp::VinylNoise vinylNoise;
    juce::dsp::Gain<float> outputGain;

    // Cached atomic parameter pointers for real-time safe lock-free access
    std::atomic<float>* driveParam { nullptr };
    std::atomic<float>* tapeMixParam { nullptr };
    std::atomic<float>* cutoffParam { nullptr };
    std::atomic<float>* resonanceParam { nullptr };
    std::atomic<float>* compThresholdParam { nullptr };
    std::atomic<float>* compRatioParam { nullptr };
    std::atomic<float>* compAttackParam { nullptr };
    std::atomic<float>* compReleaseParam { nullptr };
    std::atomic<float>* compMakeupParam { nullptr };
    std::atomic<float>* compMixParam { nullptr };
    std::atomic<float>* vinylNoiseParam { nullptr };
    std::atomic<float>* vinylDustParam { nullptr };
    std::atomic<float>* outputGainParam { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubdrumProcessorAudioProcessor)
};
