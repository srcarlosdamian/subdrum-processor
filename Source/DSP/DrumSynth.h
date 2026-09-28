#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace underground::dsp
{

/**
 * @brief Multi-voice drum synthesizer mapped to standard General MIDI (GM)
 *        and keyboard trigger performance notes (Notes 36 - 51).
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

        clapFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1400.0f, 1.4f);
        clapFilter.reset();

        rimFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1800.0f, 3.5f);
        rimFilter.reset();

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

        clapEnv = 0.0f;
        clapStep = 0;
        clapTimer = 0;

        rimEnv = 0.0f;
        rimPhase = 0.0f;

        hatEnv = 0.0f;
        hatDecayRate = 0.999f;

        tomEnv = 0.0f;
        tomPhase = 0.0f;
        tomFreq = 120.0f;

        subPhase = 0.0f;
        subEnv = 0.0f;

        hatFilter.reset();
        snareFilter.reset();
        clapFilter.reset();
        rimFilter.reset();

        rngState = 0x98765432;
    }

    void handleMidiEvent(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            const int rawNote = msg.getNoteNumber();
            int note = rawNote;
            if (note >= 60 && note <= 75)
                note -= 24;
            else if (note >= 48 && note <= 59 && note != 48)
                note -= 12;

            const float vel = msg.getFloatVelocity();

            switch (note)
            {
                case 35: // Acoustic Bass Drum
                case 36: // C1 (Key 'A') - Bass Drum / Kick
                    kickEnv = vel * 1.1f;
                    kickPitchEnv = 1.0f;
                    kickPhase = 0.0f;
                    break;

                case 37: // C#1 (Key 'W') - Side Stick / Rimshot
                    rimEnv = vel * 1.0f;
                    rimPhase = 0.0f;
                    break;

                case 38: // D1 (Key 'S') - Snare Acoustic / Lo-fi
                case 40: // E1 (Key 'D') - Electric Snare
                    snareToneEnv = vel * 0.85f;
                    snareNoiseEnv = vel * 0.95f;
                    snarePhase = 0.0f;
                    break;

                case 39: // D#1 (Key 'E') - Hand Clap
                    clapEnv = vel * 1.0f;
                    clapStep = 0;
                    clapTimer = 0;
                    break;

                case 41: // F1 (Key 'F') - Low Floor Tom
                    tomFreq = 85.0f;
                    tomEnv = vel * 0.9f;
                    tomPhase = 0.0f;
                    break;

                case 43: // G1 (Key 'G') - Low Tom
                    tomFreq = 110.0f;
                    tomEnv = vel * 0.9f;
                    tomPhase = 0.0f;
                    break;

                case 45: // A1 (Key 'H') - Mid Tom
                    tomFreq = 145.0f;
                    tomEnv = vel * 0.9f;
                    tomPhase = 0.0f;
                    break;

                case 47: // B1 (Key 'J') - High Tom
                    tomFreq = 190.0f;
                    tomEnv = vel * 0.9f;
                    tomPhase = 0.0f;
                    break;

                case 42: // F#1 (Key 'T') - Closed Hi-Hat
                case 44: // G#1 (Key 'Y') - Pedal Hi-Hat
                    hatEnv = vel * 0.75f;
                    hatDecayRate = std::exp(-1.0f / (0.001f * 38.0f * static_cast<float>(sampleRate)));
                    break;

                case 46: // A#1 (Key 'U') - Open Hi-Hat
                case 49: // C#2 (Key 'O') - Crash Cymbal
                case 51: // D#2 - Ride Cymbal
                    hatEnv = vel * 0.85f;
                    hatDecayRate = std::exp(-1.0f / (0.001f * 360.0f * static_cast<float>(sampleRate)));
                    break;

                case 48: // C2 (Key 'K') - Heavy Sub 808 Bass
                    subEnv = vel * 1.0f;
                    subPhase = 0.0f;
                    break;

                default:
                    if (rawNote > 48)
                    {
                        subEnv = vel * 0.9f;
                        subPhase = 0.0f;
                    }
                    break;
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
            while (midiIterator != midiMessages.cend() && (*midiIterator).samplePosition == sampleIdx)
            {
                handleMidiEvent((*midiIterator).getMessage());
                ++midiIterator;
            }

            float synthSample = 0.0f;

            // 1. Kick Voice
            if (kickEnv > 1.0e-4f)
            {
                const float kickFreq = 46.0f + 130.0f * (kickPitchEnv * kickPitchEnv);
                synthSample += std::sin(kickPhase) * kickEnv * 1.25f;

                kickPhase += twoPi * kickFreq * samplePeriod;
                if (kickPhase >= twoPi)
                    kickPhase -= twoPi;

                kickEnv *= 0.99962f;
                kickPitchEnv *= 0.9958f;
            }

            // 2. Snare Voice
            if (snareToneEnv > 1.0e-4f || snareNoiseEnv > 1.0e-4f)
            {
                const float snareTone = std::sin(snarePhase) * snareToneEnv * 0.6f;
                const float rawNoise = nextRandomFloat() * 2.0f - 1.0f;
                const float filteredNoise = snareFilter.processSample(rawNoise) * snareNoiseEnv * 0.95f;

                synthSample += (snareTone + filteredNoise);

                snarePhase += twoPi * 185.0f * samplePeriod;
                if (snarePhase >= twoPi)
                    snarePhase -= twoPi;

                snareToneEnv *= 0.9992f;
                snareNoiseEnv *= 0.99935f;
            }

            // 3. Hand Clap Voice
            if (clapEnv > 1.0e-4f)
            {
                clapTimer++;
                const int burstSamples = static_cast<int>(sampleRate * 0.011f);
                if (clapStep < 3 && clapTimer > burstSamples)
                {
                    clapTimer = 0;
                    clapStep++;
                    clapEnv = 0.9f;
                }

                const float rawNoise = nextRandomFloat() * 2.0f - 1.0f;
                const float filteredClap = clapFilter.processSample(rawNoise) * clapEnv * 1.1f;
                synthSample += filteredClap;

                clapEnv *= 0.9994f;
            }

            // 4. Rimshot Voice
            if (rimEnv > 1.0e-4f)
            {
                const float rimTone = std::sin(rimPhase) * rimEnv * 1.2f;
                synthSample += rimFilter.processSample(rimTone);

                rimPhase += twoPi * 1650.0f * samplePeriod;
                if (rimPhase >= twoPi)
                    rimPhase -= twoPi;

                rimEnv *= 0.997f;
            }

            // 5. Toms Voice
            if (tomEnv > 1.0e-4f)
            {
                synthSample += std::sin(tomPhase) * tomEnv * 0.8f;
                tomPhase += twoPi * tomFreq * samplePeriod;
                if (tomPhase >= twoPi)
                    tomPhase -= twoPi;

                tomEnv *= 0.9994f;
            }

            // 6. Hi-Hat / Cymbals
            if (hatEnv > 1.0e-4f)
            {
                const float noise = nextRandomFloat() * 2.0f - 1.0f;
                const float filteredHat = hatFilter.processSample(noise) * hatEnv * 0.85f;
                synthSample += filteredHat;

                hatEnv *= hatDecayRate;
            }

            // 7. Sub 808 Bass Voice
            if (subEnv > 1.0e-4f)
            {
                const float subTone = std::sin(subPhase) * subEnv * 0.95f;
                synthSample += subTone;

                subPhase += twoPi * 44.0f * samplePeriod;
                if (subPhase >= twoPi)
                    subPhase -= twoPi;

                subEnv *= 0.99992f;
            }

            if (synthSample != 0.0f)
            {
                const float limited = std::tanh(synthSample * 0.85f);
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    buffer.addSample(ch, sampleIdx, limited);
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
    float kickEnv { 0.0f };
    float kickPitchEnv { 0.0f };

    float snarePhase { 0.0f };
    float snareToneEnv { 0.0f };
    float snareNoiseEnv { 0.0f };
    juce::dsp::IIR::Filter<float> snareFilter;

    float clapEnv { 0.0f };
    int clapStep { 0 };
    int clapTimer { 0 };
    juce::dsp::IIR::Filter<float> clapFilter;

    float rimEnv { 0.0f };
    float rimPhase { 0.0f };
    juce::dsp::IIR::Filter<float> rimFilter;

    float tomEnv { 0.0f };
    float tomPhase { 0.0f };
    float tomFreq { 120.0f };

    float hatEnv { 0.0f };
    float hatDecayRate { 0.999f };
    juce::dsp::IIR::Filter<float> hatFilter;

    float subPhase { 0.0f };
    float subEnv { 0.0f };
};

} // namespace underground::dsp
