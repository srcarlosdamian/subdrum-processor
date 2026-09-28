#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace underground::dsp
{

/**
 * @brief Precision drum synthesizer calibrated from exact spectrograms:
 *        - Note 36 (Key 'A'): Kick (Sub-heavy 52Hz fundamental, 130Hz transient)
 *        - Note 38 (Key 'S'): Snare 1 (Image 2: 195Hz body + 3-8kHz dense lo-fi sizzle)
 *        - Note 40 (Key 'D'): Snare 2 (Image 3: 215Hz resonant body + 7-10kHz high crack)
 */
class DrumSynth
{
public:
    DrumSynth() = default;
    ~DrumSynth() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        kickClickFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 3500.0f, 0.707f);
        kickClickFilter.reset();

        // Snare 1 Filters (Image 2: wide mid-high textured band)
        snare1Filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 4500.0f, 0.9f);
        snare1Filter.reset();

        // Snare 2 Filters (Image 3: focused high crack around 7.5 kHz)
        snare2Filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 6200.0f, 1.4f);
        snare2Filter.reset();

        snare2MidFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1200.0f, 1.8f);
        snare2MidFilter.reset();

        hatFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 7000.0f, 1.2f);
        hatFilter.reset();

        reset();
    }

    void reset()
    {
        kickPhase = 0.0f;
        kickEnv = 0.0f;
        kickPitchEnv = 0.0f;
        kickClickEnv = 0.0f;

        // Snare 1
        snare1Phase = 0.0f;
        snare1ToneEnv = 0.0f;
        snare1NoiseEnv = 0.0f;

        // Snare 2
        snare2Phase = 0.0f;
        snare2ToneEnv = 0.0f;
        snare2NoiseEnv = 0.0f;

        hatEnv = 0.0f;
        hatDecayRate = 0.999f;
        subPhase = 0.0f;
        subEnv = 0.0f;

        kickClickFilter.reset();
        snare1Filter.reset();
        snare2Filter.reset();
        snare2MidFilter.reset();
        hatFilter.reset();

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
                case 35:
                case 36: // C1 (Key 'A') - Image 1: Kick
                    kickEnv = vel * 1.25f;
                    kickPitchEnv = 1.0f;
                    kickClickEnv = vel * 0.8f;
                    kickPhase = 0.0f;
                    break;

                case 38: // D1 (Key 'S') - Image 2: Snare 1
                    snare1ToneEnv = vel * 1.0f;
                    snare1NoiseEnv = vel * 1.15f;
                    snare1Phase = 0.0f;
                    break;

                case 37: // C#1 (Key 'W')
                case 39: // D#1 (Key 'E')
                case 40: // E1 (Key 'D') - Image 3: Snare 2
                    snare2ToneEnv = vel * 0.95f;
                    snare2NoiseEnv = vel * 1.2f;
                    snare2Phase = 0.0f;
                    break;

                case 42: // F#1 (Key 'T') - Closed Hat
                case 44:
                    hatEnv = vel * 0.75f;
                    hatDecayRate = std::exp(-1.0f / (0.001f * 38.0f * static_cast<float>(sampleRate)));
                    break;

                case 46: // A#1 (Key 'U') - Open Hat
                case 49:
                    hatEnv = vel * 0.85f;
                    hatDecayRate = std::exp(-1.0f / (0.001f * 360.0f * static_cast<float>(sampleRate)));
                    break;

                case 48: // C2 (Key 'K') - Sub 808
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

        const float kickDecayCoef = std::exp(-1.0f / (0.65f * static_cast<float>(sampleRate) * 0.35f));
        const float pitchDecayCoef = std::exp(-1.0f / (0.024f * static_cast<float>(sampleRate)));
        const float clickDecayCoef = std::exp(-1.0f / (0.004f * static_cast<float>(sampleRate)));

        // Snare decay rates calibrated to ~0.20s
        const float snareToneDecay = std::exp(-1.0f / (0.12f * static_cast<float>(sampleRate)));
        const float snareNoiseDecay = std::exp(-1.0f / (0.19f * static_cast<float>(sampleRate)));

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            while (midiIterator != midiMessages.cend() && (*midiIterator).samplePosition == sampleIdx)
            {
                handleMidiEvent((*midiIterator).getMessage());
                ++midiIterator;
            }

            float synthSample = 0.0f;

            // 1. Kick Voice (Image 1)
            if (kickEnv > 1.0e-4f)
            {
                const float kickFreq = 52.0f + 80.0f * (kickPitchEnv * kickPitchEnv);
                const float bodySine = std::sin(kickPhase) * kickEnv;
                const float warmBody = bodySine + 0.15f * (bodySine * bodySine) * (bodySine > 0.0f ? 1.0f : -1.0f);

                float clickSample = 0.0f;
                if (kickClickEnv > 1.0e-3f)
                {
                    const float rawClick = nextRandomFloat() * 2.0f - 1.0f;
                    clickSample = kickClickFilter.processSample(rawClick) * kickClickEnv * 0.45f;
                    kickClickEnv *= clickDecayCoef;
                }

                synthSample += (warmBody * 1.35f + clickSample);

                kickPhase += twoPi * kickFreq * samplePeriod;
                if (kickPhase >= twoPi)
                    kickPhase -= twoPi;

                kickEnv *= kickDecayCoef;
                kickPitchEnv *= pitchDecayCoef;
            }

            // 2. Snare 1 Voice (Image 2: 195 Hz body + 3-8 kHz dense sizzle)
            if (snare1ToneEnv > 1.0e-4f || snare1NoiseEnv > 1.0e-4f)
            {
                const float tone = std::sin(snare1Phase) * snare1ToneEnv * 0.75f;
                const float rawNoise = nextRandomFloat() * 2.0f - 1.0f;
                const float filteredNoise = snare1Filter.processSample(rawNoise) * snare1NoiseEnv * 1.1f;

                synthSample += (tone + filteredNoise);

                snare1Phase += twoPi * 195.0f * samplePeriod;
                if (snare1Phase >= twoPi)
                    snare1Phase -= twoPi;

                snare1ToneEnv *= snareToneDecay;
                snare1NoiseEnv *= snareNoiseDecay;
            }

            // 3. Snare 2 Voice (Image 3: 215 Hz body + 7-10 kHz high crack)
            if (snare2ToneEnv > 1.0e-4f || snare2NoiseEnv > 1.0e-4f)
            {
                const float tone = std::sin(snare2Phase) * snare2ToneEnv * 0.7f;
                const float rawNoise = nextRandomFloat() * 2.0f - 1.0f;
                const float highCrack = snare2Filter.processSample(rawNoise) * snare2NoiseEnv * 1.25f;
                const float midBite = snare2MidFilter.processSample(rawNoise) * snare2NoiseEnv * 0.4f;

                synthSample += (tone + highCrack + midBite);

                snare2Phase += twoPi * 215.0f * samplePeriod;
                if (snare2Phase >= twoPi)
                    snare2Phase -= twoPi;

                snare2ToneEnv *= snareToneDecay;
                snare2NoiseEnv *= (snareNoiseDecay * 0.9997f);
            }

            // 4. Hi-Hat & Sub
            if (hatEnv > 1.0e-4f)
            {
                const float noise = nextRandomFloat() * 2.0f - 1.0f;
                synthSample += hatFilter.processSample(noise) * hatEnv * 0.85f;
                hatEnv *= hatDecayRate;
            }

            if (subEnv > 1.0e-4f)
            {
                synthSample += std::sin(subPhase) * subEnv * 0.95f;
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

    // Kick State (Image 1)
    float kickPhase { 0.0f };
    float kickEnv { 0.0f };
    float kickPitchEnv { 0.0f };
    float kickClickEnv { 0.0f };
    juce::dsp::IIR::Filter<float> kickClickFilter;

    // Snare 1 State (Image 2)
    float snare1Phase { 0.0f };
    float snare1ToneEnv { 0.0f };
    float snare1NoiseEnv { 0.0f };
    juce::dsp::IIR::Filter<float> snare1Filter;

    // Snare 2 State (Image 3)
    float snare2Phase { 0.0f };
    float snare2ToneEnv { 0.0f };
    float snare2NoiseEnv { 0.0f };
    juce::dsp::IIR::Filter<float> snare2Filter;
    juce::dsp::IIR::Filter<float> snare2MidFilter;

    // Hat State
    float hatEnv { 0.0f };
    float hatDecayRate { 0.999f };
    juce::dsp::IIR::Filter<float> hatFilter;

    // Sub State
    float subPhase { 0.0f };
    float subEnv { 0.0f };
};

} // namespace underground::dsp
