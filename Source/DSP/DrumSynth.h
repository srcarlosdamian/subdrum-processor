#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace underground::dsp
{

/**
 * @brief Precision drum synthesizer with calibrated voices from spectrograms:
 *        - Note 36 (Key 'A'): Kick 1 (Deep Sub 52Hz fundamental, 0.65s tail)
 *        - Note 35 / 37 (Key 'W'): Kick 2 (Punchy 64Hz body + 6-8kHz slap beater transient)
 *        - Note 38 (Key 'S'): Snare 1 (195Hz body + 3-8kHz wide sizzle)
 *        - Note 40 (Key 'D'): Snare 2 (215Hz body + 7-10kHz high crack)
 *        - Note 39 (Key 'E'): Hand Clap
 *        - Note 42 (Key 'T'): Closed Hi-Hat
 *        - Note 46 (Key 'U'): Open Hi-Hat
 *        - Note 48 (Key 'K'): Sub 808
 */
class DrumSynth
{
public:
    DrumSynth() = default;
    ~DrumSynth() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Kick 1 click
        kick1ClickFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 3500.0f, 0.707f);
        kick1ClickFilter.reset();

        // Kick 2 beater click (hot slap burst around 6.5 - 9 kHz)
        kick2SlapFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 7200.0f, 1.2f);
        kick2SlapFilter.reset();

        // Snare 1 Filter (wide mid-high textured band)
        snare1Filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 4500.0f, 0.9f);
        snare1Filter.reset();

        // Snare 2 Filters (focused high crack)
        snare2Filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 6200.0f, 1.4f);
        snare2Filter.reset();

        snare2MidFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1200.0f, 1.8f);
        snare2MidFilter.reset();

        // Clap Filter
        clapFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1400.0f, 1.4f);
        clapFilter.reset();

        // Hat Filter
        hatFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 7000.0f, 1.2f);
        hatFilter.reset();

        reset();
    }

    void reset()
    {
        // Kick 1
        kick1Phase = 0.0f;
        kick1Env = 0.0f;
        kick1PitchEnv = 0.0f;
        kick1ClickEnv = 0.0f;

        // Kick 2
        kick2Phase = 0.0f;
        kick2Env = 0.0f;
        kick2PitchEnv = 0.0f;
        kick2SlapEnv = 0.0f;

        // Snare 1
        snare1Phase = 0.0f;
        snare1ToneEnv = 0.0f;
        snare1NoiseEnv = 0.0f;

        // Snare 2
        snare2Phase = 0.0f;
        snare2ToneEnv = 0.0f;
        snare2NoiseEnv = 0.0f;

        clapEnv = 0.0f;
        clapStep = 0;
        clapTimer = 0;

        hatEnv = 0.0f;
        hatDecayRate = 0.999f;
        subPhase = 0.0f;
        subEnv = 0.0f;

        kick1ClickFilter.reset();
        kick2SlapFilter.reset();
        snare1Filter.reset();
        snare2Filter.reset();
        snare2MidFilter.reset();
        clapFilter.reset();
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
                case 36: // C1 (Key 'A') - Kick 1: Deep Sub 52Hz
                    kick1Env = vel * 1.25f;
                    kick1PitchEnv = 1.0f;
                    kick1ClickEnv = vel * 0.8f;
                    kick1Phase = 0.0f;
                    break;

                case 35:
                case 37: // C#1 (Key 'W') - Kick 2: Punchy 64Hz + 7kHz slap transient
                    kick2Env = vel * 1.3f;
                    kick2PitchEnv = 1.0f;
                    kick2SlapEnv = vel * 1.1f;
                    kick2Phase = 0.0f;
                    break;

                case 38: // D1 (Key 'S') - Snare 1: 195Hz + 4.5kHz sizzle
                    snare1ToneEnv = vel * 1.0f;
                    snare1NoiseEnv = vel * 1.15f;
                    snare1Phase = 0.0f;
                    break;

                case 40: // E1 (Key 'D') - Snare 2: 215Hz + 7.5kHz crack
                    snare2ToneEnv = vel * 0.95f;
                    snare2NoiseEnv = vel * 1.2f;
                    snare2Phase = 0.0f;
                    break;

                case 39: // D#1 (Key 'E') - Hand Clap
                    clapEnv = vel * 1.0f;
                    clapStep = 0;
                    clapTimer = 0;
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

        const float kick1DecayCoef = std::exp(-1.0f / (0.65f * static_cast<float>(sampleRate) * 0.35f));
        const float kick2DecayCoef = std::exp(-1.0f / (0.38f * static_cast<float>(sampleRate) * 0.45f)); // Tighter punchy decay
        const float pitchDecayCoef = std::exp(-1.0f / (0.024f * static_cast<float>(sampleRate)));
        const float slapDecayCoef = std::exp(-1.0f / (0.018f * static_cast<float>(sampleRate))); // 18ms slap burst

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

            // 1. Kick 1 Voice (Deep 52 Hz)
            if (kick1Env > 1.0e-4f)
            {
                const float kickFreq = 52.0f + 80.0f * (kick1PitchEnv * kick1PitchEnv);
                const float bodySine = std::sin(kick1Phase) * kick1Env;
                const float warmBody = bodySine + 0.15f * (bodySine * bodySine) * (bodySine > 0.0f ? 1.0f : -1.0f);

                float clickSample = 0.0f;
                if (kick1ClickEnv > 1.0e-3f)
                {
                    const float rawClick = nextRandomFloat() * 2.0f - 1.0f;
                    clickSample = kick1ClickFilter.processSample(rawClick) * kick1ClickEnv * 0.45f;
                    kick1ClickEnv *= pitchDecayCoef;
                }

                synthSample += (warmBody * 1.35f + clickSample);

                kick1Phase += twoPi * kickFreq * samplePeriod;
                if (kick1Phase >= twoPi)
                    kick1Phase -= twoPi;

                kick1Env *= kick1DecayCoef;
                kick1PitchEnv *= pitchDecayCoef;
            }

            // 2. Kick 2 Voice (Punchy 64 Hz + 7 kHz Slap Transient)
            if (kick2Env > 1.0e-4f)
            {
                const float kickFreq = 64.0f + 110.0f * (kick2PitchEnv * kick2PitchEnv);
                const float bodySine = std::sin(kick2Phase) * kick2Env;
                const float warmBody = bodySine + 0.20f * (bodySine * bodySine) * (bodySine > 0.0f ? 1.0f : -1.0f);

                float slapSample = 0.0f;
                if (kick2SlapEnv > 1.0e-3f)
                {
                    const float rawClick = nextRandomFloat() * 2.0f - 1.0f;
                    slapSample = kick2SlapFilter.processSample(rawClick) * kick2SlapEnv * 0.85f;
                    kick2SlapEnv *= slapDecayCoef;
                }

                synthSample += (warmBody * 1.4f + slapSample);

                kick2Phase += twoPi * kickFreq * samplePeriod;
                if (kick2Phase >= twoPi)
                    kick2Phase -= twoPi;

                kick2Env *= kick2DecayCoef;
                kick2PitchEnv *= pitchDecayCoef;
            }

            // 3. Snare 1 Voice (195 Hz + 4.5 kHz)
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

            // 4. Snare 2 Voice (215 Hz + 7.5 kHz Crack)
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

            // 5. Hand Clap
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

            // 6. Hi-Hat & Sub
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

    // Kick 1
    float kick1Phase { 0.0f };
    float kick1Env { 0.0f };
    float kick1PitchEnv { 0.0f };
    float kick1ClickEnv { 0.0f };
    juce::dsp::IIR::Filter<float> kick1ClickFilter;

    // Kick 2
    float kick2Phase { 0.0f };
    float kick2Env { 0.0f };
    float kick2PitchEnv { 0.0f };
    float kick2SlapEnv { 0.0f };
    juce::dsp::IIR::Filter<float> kick2SlapFilter;

    // Snare 1
    float snare1Phase { 0.0f };
    float snare1ToneEnv { 0.0f };
    float snare1NoiseEnv { 0.0f };
    juce::dsp::IIR::Filter<float> snare1Filter;

    // Snare 2
    float snare2Phase { 0.0f };
    float snare2ToneEnv { 0.0f };
    float snare2NoiseEnv { 0.0f };
    juce::dsp::IIR::Filter<float> snare2Filter;
    juce::dsp::IIR::Filter<float> snare2MidFilter;

    // Clap
    float clapEnv { 0.0f };
    int clapStep { 0 };
    int clapTimer { 0 };
    juce::dsp::IIR::Filter<float> clapFilter;

    // Hat
    float hatEnv { 0.0f };
    float hatDecayRate { 0.999f };
    juce::dsp::IIR::Filter<float> hatFilter;

    // Sub
    float subPhase { 0.0f };
    float subEnv { 0.0f };
};

} // namespace underground::dsp
