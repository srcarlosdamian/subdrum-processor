#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace underground::dsp
{

/**
 * @brief Real-time polyphonic lo-fi drum voice synthesizer for testing and performance.
 *        Generates authentic 2-step / garage drum hits:
 *        - Note 36 (C1 / 'A'): Punchy Garage Kick
 *        - Note 38 (D1 / 'S'): Crispy Underground Snare
 *        - Note 42 (F#1 / 'D'): Tight 2-Step Closed Hat
 *        - Note 46 (A#1 / 'F'): Sizzling Open Hat
 *        - Note 48 (C2 / 'G'): Warm Sub Bass Tone
 */
class DrumSynth
{
public:
    DrumSynth() = default;
    ~DrumSynth() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        hatFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 7000.0f, 1.2f);
        hatFilter.reset();

        snareFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 2200.0f, 1.0f);
        snareFilter.reset();

        reset();
    }

    void reset()
    {
        kickPhase = 0.0f;
        kickEnv = 0.0f;
        kickPitchEnv = 0.0f;

        snarePhase = 0.0f;
        snareToneEnv = 0.0f;
        snareNoiseEnv = 0.0f;

        hatEnv = 0.0f;
        hatDecayRate = 0.999f;

        subPhase = 0.0f;
        subEnv = 0.0f;

        hatFilter.reset();
        snareFilter.reset();
        rngState = 0x98765432;
    }

    void handleMidiEvent(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            if (note == 36 || note == 60) // C1 or C3 (Kick / 'A')
            {
                kickEnv = vel * 1.0f;
                kickPitchEnv = 1.0f;
                kickPhase = 0.0f;
            }
            else if (note == 38 || note == 62) // D1 or D3 (Snare / 'S')
            {
                snareToneEnv = vel * 0.8f;
                snareNoiseEnv = vel * 0.9f;
                snarePhase = 0.0f;
            }
            else if (note == 42 || note == 64) // F#1 or E3 (Closed Hat / 'D')
            {
                hatEnv = vel * 0.7f;
                hatDecayRate = std::exp(-1.0f / (0.001f * 35.0f * static_cast<float>(sampleRate)));
            }
            else if (note == 46 || note == 65) // A#1 or F3 (Open Hat / 'F')
            {
                hatEnv = vel * 0.75f;
                hatDecayRate = std::exp(-1.0f / (0.001f * 320.0f * static_cast<float>(sampleRate)));
            }
            else if (note == 48 || note == 67) // C2 or G3 (Sub Bass / 'G')
            {
                subEnv = vel * 0.9f;
                subPhase = 0.0f;
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

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            // Process MIDI messages occurring at this sample
            while (midiIterator != midiMessages.cend() && (*midiIterator).samplePosition == sampleIdx)
            {
                handleMidiEvent((*midiIterator).getMessage());
                ++midiIterator;
            }

            float synthSample = 0.0f;

            // 1. Kick Voice
            if (kickEnv > 1.0e-4f)
            {
                const float kickFreq = 48.0f + 120.0f * (kickPitchEnv * kickPitchEnv);
                synthSample += std::sin(kickPhase) * kickEnv * 1.2f;

                kickPhase += twoPi * kickFreq * samplePeriod;
                if (kickPhase >= twoPi)
                    kickPhase -= twoPi;

                kickEnv *= 0.99965f; // Fast exponential decay
                kickPitchEnv *= 0.996f; // Faster pitch drop for transient snap
            }

            // 2. Snare Voice
            if (snareToneEnv > 1.0e-4f || snareNoiseEnv > 1.0e-4f)
            {
                const float snareTone = std::sin(snarePhase) * snareToneEnv * 0.6f;
                const float rawNoise = nextRandomFloat() * 2.0f - 1.0f;
                const float filteredNoise = snareFilter.processSample(rawNoise) * snareNoiseEnv * 0.9f;

                synthSample += (snareTone + filteredNoise);

                snarePhase += twoPi * 185.0f * samplePeriod;
                if (snarePhase >= twoPi)
                    snarePhase -= twoPi;

                snareToneEnv *= 0.9992f;
                snareNoiseEnv *= 0.9993f;
            }

            // 3. Hi-Hat Voice
            if (hatEnv > 1.0e-4f)
            {
                const float noise = nextRandomFloat() * 2.0f - 1.0f;
                const float filteredHat = hatFilter.processSample(noise) * hatEnv * 0.8f;
                synthSample += filteredHat;

                hatEnv *= hatDecayRate;
            }

            // 4. Sub Bass Voice
            if (subEnv > 1.0e-4f)
            {
                const float subTone = std::sin(subPhase) * subEnv * 0.9f;
                synthSample += subTone;

                subPhase += twoPi * 45.0f * samplePeriod;
                if (subPhase >= twoPi)
                    subPhase -= twoPi;

                subEnv *= 0.99992f; // Long sustain
            }

            // Soft-limit synthesized mix and add to both stereo channels
            if (synthSample != 0.0f)
            {
                const float limitedSample = std::tanh(synthSample * 0.85f);
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    buffer.addSample(ch, sampleIdx, limitedSample);
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

    // Kick State
    float kickPhase { 0.0f };
    float kickEnv { 0.0f };
    float kickPitchEnv { 0.0f };

    // Snare State
    float snarePhase { 0.0f };
    float snareToneEnv { 0.0f };
    float snareNoiseEnv { 0.0f };
    juce::dsp::IIR::Filter<float> snareFilter;

    // Hat State
    float hatEnv { 0.0f };
    float hatDecayRate { 0.999f };
    juce::dsp::IIR::Filter<float> hatFilter;

    // Sub State
    float subPhase { 0.0f };
    float subEnv { 0.0f };
};

} // namespace underground::dsp
