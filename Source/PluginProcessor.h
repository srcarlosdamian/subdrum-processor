#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <vector>
#include "DSP/SamplePlayer.h"
#include "DSP/DrumSynth.h"
#include "DSP/StepSequencer.h"
#include "DSP/TapeSaturation.h"
#include "DSP/SamplerFilter.h"
#include "DSP/VCACompressor.h"
#include "DSP/VinylNoise.h"
#include "DSP/TapeEcho.h"
#include "DSP/RoomAmbience.h"
#include "DSP/BitCrusher.h"

class SubdrumProcessorAudioProcessor : public juce::AudioProcessor
{
public:
    struct Preset
    {
        juce::String name;
        std::map<juce::String, float> params;
    };

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

    // Presets / Programs Management
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void loadPreset(int index);
    const std::vector<Preset>& getPresets() const noexcept { return factoryPresets; }

    // Preset & Rhythm Pattern Import / Export (JSON format)
    juce::String exportPresetAsJson();
    bool importPresetFromJson(const juce::String& jsonText);
    bool exportPresetToFile(const juce::File& targetFile);
    bool importPresetFromFile(const juce::File& sourceFile);
    void loadRhythmPreset(int index);

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    float getGainReduction() const noexcept { return compressor.getGainReductionDb(); }

    // Step Sequencer (16-Step Pattern Player with Swing & Host Sync)
    underground::dsp::StepSequencer& getSequencer() noexcept { return stepSequencer; }

    // Sample Engine (Drag & Drop .WAV / Audio Files)
    bool loadSampleFile(const juce::File& file);
    bool hasLoadedSample() const noexcept { return samplePlayer.hasSample(); }
    juce::String getLoadedSampleFileName() const noexcept { return loadedSampleFileName; }
    void triggerSample() noexcept { samplePlayer.trigger(); }

    // Real-time Visualizer Buffer (Lock-free FIFO for scope)
    static constexpr int visualizerBufferSize = 64;
    void getVisualizerData(float* destinationBuffer) noexcept
    {
        for (int i = 0; i < visualizerBufferSize; ++i)
            destinationBuffer[i] = visualizerFifo[i].load(std::memory_order_relaxed);
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void initFactoryPresets();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    juce::AudioFormatManager formatManager;
    juce::String loadedSampleFileName { "NONE (SYNTH ENGINE)" };

    std::vector<Preset> factoryPresets;
    int currentProgram { 0 };

    // Visualizer atomic ring buffer
    std::array<std::atomic<float>, visualizerBufferSize> visualizerFifo {};
    int visualizerWriteIndex { 0 };

    // DSP Chain & Sequencer
    underground::dsp::StepSequencer stepSequencer;
    underground::dsp::SamplePlayer samplePlayer;
    underground::dsp::DrumSynth drumSynth;
    underground::dsp::BitCrusher bitCrusher;
    underground::dsp::TapeSaturation tapeSaturation;
    underground::dsp::SamplerFilter samplerFilter;
    underground::dsp::VCACompressor compressor;
    underground::dsp::TapeEcho tapeEcho;
    underground::dsp::RoomAmbience roomAmbience;
    underground::dsp::VinylNoise vinylNoise;
    juce::dsp::Gain<float> outputGain;

    // 1. Kick Engine Parameter Pointers
    std::atomic<float>* kickPitchParam { nullptr };
    std::atomic<float>* kickTuneParam { nullptr };
    std::atomic<float>* kickSweepParam { nullptr };
    std::atomic<float>* kickDecayParam { nullptr };
    std::atomic<float>* kickPunchParam { nullptr };
    std::atomic<float>* kickDriveParam { nullptr };

    // 2. Snare Engine Parameter Pointers
    std::atomic<float>* snarePitchParam { nullptr };
    std::atomic<float>* snareDecayParam { nullptr };
    std::atomic<float>* snareSnapParam { nullptr };
    std::atomic<float>* snareToneParam { nullptr };
    std::atomic<float>* snareCrackParam { nullptr };
    std::atomic<float>* snareDriveParam { nullptr };

    // 3. Acoustic Clap Engine Parameter Pointers
    std::atomic<float>* clapPitchParam { nullptr };
    std::atomic<float>* clapDecayParam { nullptr };
    std::atomic<float>* clapWoodParam { nullptr };
    std::atomic<float>* clapSlapParam { nullptr };
    std::atomic<float>* clapToneParam { nullptr };
    std::atomic<float>* clapTailParam { nullptr };

    // 4. Closed Hat Engine Parameter Pointers
    std::atomic<float>* chatPitchParam { nullptr };
    std::atomic<float>* chatDecayParam { nullptr };
    std::atomic<float>* chatToneParam { nullptr };
    std::atomic<float>* chatSizzleParam { nullptr };
    std::atomic<float>* chatResParam { nullptr };
    std::atomic<float>* chatDriveParam { nullptr };

    // 5. Open Hat Engine Parameter Pointers
    std::atomic<float>* ohatPitchParam { nullptr };
    std::atomic<float>* ohatDecayParam { nullptr };
    std::atomic<float>* ohatToneParam { nullptr };
    std::atomic<float>* ohatSizzleParam { nullptr };
    std::atomic<float>* ohatChokeParam { nullptr };
    std::atomic<float>* ohatDriveParam { nullptr };

    // 6. Rimshot Engine Parameter Pointers
    std::atomic<float>* rimPitchParam { nullptr };
    std::atomic<float>* rimDecayParam { nullptr };
    std::atomic<float>* rimTuneParam { nullptr };
    std::atomic<float>* rimSnapParam { nullptr };
    std::atomic<float>* rimToneParam { nullptr };
    std::atomic<float>* rimDriveParam { nullptr };

    // 7. Sub 808 Engine Parameter Pointers
    std::atomic<float>* subTuneParam { nullptr };
    std::atomic<float>* subDecayParam { nullptr };
    std::atomic<float>* subSweepParam { nullptr };
    std::atomic<float>* subDriveParam { nullptr };
    std::atomic<float>* subCutoffParam { nullptr };
    std::atomic<float>* subLevelParam { nullptr };

    // 8. Shaker & Vinyl Engine Parameter Pointers
    std::atomic<float>* shakerAttackParam { nullptr };
    std::atomic<float>* shakerDecayParam { nullptr };
    std::atomic<float>* shakerToneParam { nullptr };
    std::atomic<float>* vinylCrackleParam { nullptr };
    std::atomic<float>* vinylHissParam { nullptr };
    std::atomic<float>* shakerDriveParam { nullptr };

    // Master DSP, Dub Echo & Room Ambience Parameter Pointers
    std::atomic<float>* roomMixParam { nullptr };
    std::atomic<float>* roomSizeParam { nullptr };
    std::atomic<float>* echoTimeParam { nullptr };
    std::atomic<float>* echoFeedbackParam { nullptr };
    std::atomic<float>* echoMixParam { nullptr };
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

    // Lo-Fi Bits, Downsampler, Ducking & Swing Pointers
    std::atomic<float>* bitDepthParam { nullptr };
    std::atomic<float>* downsampleParam { nullptr };
    std::atomic<float>* bitMixParam { nullptr };
    std::atomic<float>* duckDepthParam { nullptr };
    std::atomic<float>* seqSwingParam { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubdrumProcessorAudioProcessor)
};
