#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace underground::dsp
{

/**
 * @brief High-precision Kick synthesizer modeled strictly on the spectrogram time-frequency
 *        profile (68Hz-85Hz dense fundamental body, 145Hz transient sweep, fast 0.07s decay).
 *        All other sounds are completely muted as requested.
 */
class DrumSynth
{
public:
    DrumSynth() = default;
    ~DrumSynth() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Bandpass filter for acoustic beater attack transient
        kickBeaterFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 3800.0f, 1.2f);
        kickBeaterFilter.reset();

        // Lowpass body smoothing filter
        kickBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 1200.0f, 0.707f);
        kickBodyFilter.reset();

        reset();
    }

    void reset()
    {
        kickPhase = 0.0f;
        kickHarmonicPhase = 0.0f;
        kickEnv = 0.0f;
        kickPitchEnv = 0.0f;
        kickBeaterEnv = 0.0f;

        kickBeaterFilter.reset();
        kickBodyFilter.reset();
        rngState = 0x98765432;
    }

    void handleMidiEvent(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            // Only trigger on Kick notes (Note 35, 36, or any key if playing single sound)
            if (note == 35 || note == 36 || note == 60)
            {
                kickEnv = vel * 1.35f;
                kickPitchEnv = 1.0f;
                kickBeaterEnv = vel * 1.0f;
                kickPhase = 0.0f;
                kickHarmonicPhase = 0.0f;
            }
        }
    }

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        auto midiIterator = midiMessages.findNextSamplePosition(0);
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        const float twoPi = juce::MathConstants<float>::twoPi;
        const float samplePeriod = 1.0f / static_cast<float>(sampleRate);

        // Calibrated decay rates matching the 0.07s spectrogram window
        const float kickDecayCoef = std::exp(-1.0f / (0.072f * static_cast<float>(sampleRate) * 0.45f));
        const float pitchDecayCoef = std::exp(-1.0f / (0.016f * static_cast<float>(sampleRate))); // 16ms pitch drop
        const float beaterDecayCoef = std::exp(-1.0f / (0.006f * static_cast<float>(sampleRate))); // 6ms acoustic click

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            while (midiIterator != midiMessages.cend() && (*midiIterator).samplePosition == sampleIdx)
            {
                handleMidiEvent((*midiIterator).getMessage());
                ++midiIterator;
            }

            float synthSample = 0.0f;

            if (kickEnv > 1.0e-4f)
            {
                // Pitch envelope: drops rapidly from 145 Hz down to 68 Hz fundamental
                const float kickFreq = 68.0f + 77.0f * (kickPitchEnv * kickPitchEnv);

                // Multi-harmonic synthesized body (Fundamental + 2nd harmonic warmth)
                const float fund = std::sin(kickPhase);
                const float harm2 = std::sin(kickHarmonicPhase) * 0.35f;
                const float rawBody = (fund + harm2) * kickEnv;

                // Non-linear saturation for intense low-end weight
                const float saturatedBody = std::tanh(rawBody * 1.6f);
                const float filteredBody = kickBodyFilter.processSample(saturatedBody);

                // Acoustic Beater click transient
                float beaterClick = 0.0f;
                if (kickBeaterEnv > 1.0e-3f)
                {
                    const float noise = nextRandomFloat() * 2.0f - 1.0f;
                    beaterClick = kickBeaterFilter.processSample(noise) * kickBeaterEnv * 0.75f;
                    kickBeaterEnv *= beaterDecayCoef;
                }

                synthSample = filteredBody * 1.3f + beaterClick;

                kickPhase += twoPi * kickFreq * samplePeriod;
                if (kickPhase >= twoPi)
                    kickPhase -= twoPi;

                kickHarmonicPhase += twoPi * (kickFreq * 2.0f) * samplePeriod;
                if (kickHarmonicPhase >= twoPi)
                    kickHarmonicPhase -= twoPi;

                kickEnv *= kickDecayCoef;
                kickPitchEnv *= pitchDecayCoef;
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

    float kickPhase { 0.0f };
    float kickHarmonicPhase { 0.0f };
    float kickEnv { 0.0f };
    float kickPitchEnv { 0.0f };
    float kickBeaterEnv { 0.0f };

    juce::dsp::IIR::Filter<float> kickBeaterFilter;
    juce::dsp::IIR::Filter<float> kickBodyFilter;
};

} // namespace underground::dsp
