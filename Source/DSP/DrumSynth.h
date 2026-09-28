#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace underground::dsp
{

/**
 * @brief High-precision 2-Voice Drum Synthesizer strictly matching provided spectrograms:
 *        - Sound 1 (KICK): 68Hz-85Hz fundamental, 145Hz pitch drop, 0.07s decay.
 *        - Sound 2 (SNARE): 150Hz body tone, scooped mids (1.5-3kHz), 8-14kHz sizzle, 0.27s decay.
 *        - All other sounds strictly muted/disabled.
 */
class DrumSynth
{
public:
    DrumSynth() = default;
    ~DrumSynth() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Kick Filters
        kickBeaterFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 3800.0f, 1.2f);
        kickBeaterFilter.reset();

        kickBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 1200.0f, 0.707f);
        kickBodyFilter.reset();

        // Snare Filters (Strictly calibrated to 0.27s Spectrogram)
        // 1. Low fundamental body resonator (150Hz)
        snareBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 340.0f, 0.9f);
        snareBodyFilter.reset();

        // 2. High-Frequency Noise Sizzle & Hiss (8kHz - 14kHz)
        snareSizzleFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 9800.0f, 1.4f);
        snareSizzleFilter.reset();

        // 3. Transient Crack (4.5kHz)
        snareCrackFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 4500.0f, 2.0f);
        snareCrackFilter.reset();

        reset();
    }

    void reset()
    {
        kickPhase = 0.0f;
        kickHarmonicPhase = 0.0f;
        kickEnv = 0.0f;
        kickPitchEnv = 0.0f;
        kickBeaterEnv = 0.0f;

        snarePhase = 0.0f;
        snareEnv = 0.0f;
        snarePitchEnv = 0.0f;
        snareNoiseEnv = 0.0f;
        snareSnapEnv = 0.0f;

        kickBeaterFilter.reset();
        kickBodyFilter.reset();
        snareBodyFilter.reset();
        snareSizzleFilter.reset();
        snareCrackFilter.reset();

        rngState = 0x98765432;
    }

    void handleMidiEvent(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            // Sound 1: KICK (Note 35, 36)
            if (note == 35 || note == 36 || note == 60)
            {
                kickEnv = vel * 1.35f;
                kickPitchEnv = 1.0f;
                kickBeaterEnv = vel * 1.0f;
                kickPhase = 0.0f;
                kickHarmonicPhase = 0.0f;
            }
            // Sound 2: SNARE (Note 38 / key 'S')
            else if (note == 38)
            {
                snareEnv = vel * 1.3f;
                snarePitchEnv = 1.0f;
                snareNoiseEnv = vel * 1.2f;
                snareSnapEnv = vel * 1.5f;
                snarePhase = 0.0f;
            }
            // Strict Mute on all other notes
        }
    }

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        auto midiIterator = midiMessages.findNextSamplePosition(0);
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        const float twoPi = juce::MathConstants<float>::twoPi;
        const float samplePeriod = 1.0f / static_cast<float>(sampleRate);

        // Kick decay rates (0.07s)
        const float kickDecayCoef = std::exp(-1.0f / (0.072f * static_cast<float>(sampleRate) * 0.45f));
        const float pitchDecayCoef = std::exp(-1.0f / (0.016f * static_cast<float>(sampleRate)));
        const float beaterDecayCoef = std::exp(-1.0f / (0.006f * static_cast<float>(sampleRate)));

        // Snare decay rates (Calibrated to exactly 0.27s Spectrogram)
        const float snareBodyDecayCoef = std::exp(-1.0f / (0.260f * static_cast<float>(sampleRate) * 0.45f)); // 260ms body
        const float snarePitchDecayCoef = std::exp(-1.0f / (0.022f * static_cast<float>(sampleRate))); // 22ms initial drop
        const float snareNoiseDecayCoef = std::exp(-1.0f / (0.270f * static_cast<float>(sampleRate) * 0.50f)); // 270ms sizzle tail
        const float snareSnapDecayCoef = std::exp(-1.0f / (0.012f * static_cast<float>(sampleRate))); // 12ms initial crack

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            while (midiIterator != midiMessages.cend() && (*midiIterator).samplePosition == sampleIdx)
            {
                handleMidiEvent((*midiIterator).getMessage());
                ++midiIterator;
            }

            float synthSample = 0.0f;

            // 1. Kick Voice (Sound 1)
            if (kickEnv > 1.0e-4f)
            {
                const float kickFreq = 68.0f + 77.0f * (kickPitchEnv * kickPitchEnv);
                const float fund = std::sin(kickPhase);
                const float harm2 = std::sin(kickHarmonicPhase) * 0.35f;
                const float rawBody = (fund + harm2) * kickEnv;
                const float saturatedBody = std::tanh(rawBody * 1.6f);
                const float filteredBody = kickBodyFilter.processSample(saturatedBody);

                float beaterClick = 0.0f;
                if (kickBeaterEnv > 1.0e-3f)
                {
                    const float noise = nextRandomFloat() * 2.0f - 1.0f;
                    beaterClick = kickBeaterFilter.processSample(noise) * kickBeaterEnv * 0.75f;
                    kickBeaterEnv *= beaterDecayCoef;
                }

                synthSample += filteredBody * 1.3f + beaterClick;

                kickPhase += twoPi * kickFreq * samplePeriod;
                if (kickPhase >= twoPi) kickPhase -= twoPi;

                kickHarmonicPhase += twoPi * (kickFreq * 2.0f) * samplePeriod;
                if (kickHarmonicPhase >= twoPi) kickHarmonicPhase -= twoPi;

                kickEnv *= kickDecayCoef;
                kickPitchEnv *= pitchDecayCoef;
            }

            // 2. Snare Voice (Sound 2 - 0.27s Spectrogram Profile)
            if (snareEnv > 1.0e-4f || snareNoiseEnv > 1.0e-4f)
            {
                // Low fundamental tone (150Hz decaying over 0.26s)
                const float snareFreq = 150.0f + 95.0f * (snarePitchEnv * snarePitchEnv);
                const float fund = std::sin(snarePhase);
                const float filteredBody = snareBodyFilter.processSample(std::tanh(fund * snareEnv * 1.8f));

                // 8kHz - 14kHz Sustained Noise Tail (Sizzle & Hiss)
                const float noise = nextRandomFloat() * 2.0f - 1.0f;
                const float sizzleNoise = snareSizzleFilter.processSample(noise) * snareNoiseEnv * 1.15f;

                // Initial attack crack (4.5kHz burst in first 15ms)
                float crack = 0.0f;
                if (snareSnapEnv > 1.0e-3f)
                {
                    crack = snareCrackFilter.processSample(noise) * snareSnapEnv * 1.3f;
                    snareSnapEnv *= snareSnapDecayCoef;
                }

                const float totalSnare = (filteredBody * 1.1f) + sizzleNoise + crack;
                synthSample += totalSnare;

                snarePhase += twoPi * snareFreq * samplePeriod;
                if (snarePhase >= twoPi) snarePhase -= twoPi;

                snareEnv *= snareBodyDecayCoef;
                snarePitchEnv *= snarePitchDecayCoef;
                snareNoiseEnv *= snareNoiseDecayCoef;
            }

            if (synthSample != 0.0f)
            {
                const float outSample = std::tanh(synthSample * 0.9f);
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    buffer.addSample(ch, sampleIdx, outSample);
                }
            }
        }
    }

private:
    inline float nextRandomFloat() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return static_cast<float>(rngState & 0x00FFFFFF) / static_cast<float>(0x01000000);
    }

    double sampleRate { 44100.0 };
    uint32_t rngState { 0x98765432 };

    // Kick State (Sound 1)
    float kickPhase { 0.0f };
    float kickHarmonicPhase { 0.0f };
    float kickEnv { 0.0f };
    float kickPitchEnv { 0.0f };
    float kickBeaterEnv { 0.0f };

    juce::dsp::IIR::Filter<float> kickBeaterFilter;
    juce::dsp::IIR::Filter<float> kickBodyFilter;

    // Snare State (Sound 2)
    float snarePhase { 0.0f };
    float snareEnv { 0.0f };
    float snarePitchEnv { 0.0f };
    float snareNoiseEnv { 0.0f };
    float snareSnapEnv { 0.0f };

    juce::dsp::IIR::Filter<float> snareBodyFilter;
    juce::dsp::IIR::Filter<float> snareSizzleFilter;
    juce::dsp::IIR::Filter<float> snareCrackFilter;
};

} // namespace underground::dsp
