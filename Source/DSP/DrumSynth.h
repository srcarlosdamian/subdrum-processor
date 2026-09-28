#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace underground::dsp
{

/**
 * @brief High-precision Drum synthesizer modeled strictly on spectrogram time-frequency profiles:
 *        - Kick: 68Hz-85Hz dense fundamental body, 145Hz transient sweep, fast 0.07s decay.
 *        - Snare: 165Hz-190Hz sustaining body tone (0.15s), fast pitch-drop attack, and 4.5kHz-16kHz crisp noise burst.
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

        // Snare Filters (Modeled on Spectrogram)
        // 1. Snare Body Resonator (175Hz fundamental body)
        snareBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 950.0f, 0.85f);
        snareBodyFilter.reset();

        // 2. Snare Wire Crack (4.8kHz Bandpass)
        snareWireFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 4800.0f, 1.8f);
        snareWireFilter.reset();

        // 3. Snare High Air / Sizzle (8kHz - 16kHz Highpass)
        snareAirFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 7500.0f, 0.707f);
        snareAirFilter.reset();

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
        snareWireFilter.reset();
        snareAirFilter.reset();

        rngState = 0x98765432;
    }

    void handleMidiEvent(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            // Kick Notes (Note 35, 36)
            if (note == 35 || note == 36 || note == 60)
            {
                kickEnv = vel * 1.35f;
                kickPitchEnv = 1.0f;
                kickBeaterEnv = vel * 1.0f;
                kickPhase = 0.0f;
                kickHarmonicPhase = 0.0f;
            }
            // Snare Notes (Note 38: Snare 1, Note 40: Snare 2, Note 39: Clap)
            else if (note == 38 || note == 40 || note == 39)
            {
                snareEnv = vel * 1.25f;
                snarePitchEnv = 1.0f;
                snareNoiseEnv = vel * 1.15f;
                snareSnapEnv = vel * 1.4f;
                snarePhase = 0.0f;
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

        // Kick decay rates
        const float kickDecayCoef = std::exp(-1.0f / (0.072f * static_cast<float>(sampleRate) * 0.45f));
        const float pitchDecayCoef = std::exp(-1.0f / (0.016f * static_cast<float>(sampleRate)));
        const float beaterDecayCoef = std::exp(-1.0f / (0.006f * static_cast<float>(sampleRate)));

        // Snare decay rates (Modeled on the 0.15s spectrogram profile)
        const float snareBodyDecayCoef = std::exp(-1.0f / (0.155f * static_cast<float>(sampleRate) * 0.55f));
        const float snarePitchDecayCoef = std::exp(-1.0f / (0.018f * static_cast<float>(sampleRate))); // 18ms pitch drop
        const float snareNoiseDecayCoef = std::exp(-1.0f / (0.110f * static_cast<float>(sampleRate) * 0.65f)); // 110ms noise rattle
        const float snareSnapDecayCoef = std::exp(-1.0f / (0.008f * static_cast<float>(sampleRate))); // 8ms initial transient crack

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            while (midiIterator != midiMessages.cend() && (*midiIterator).samplePosition == sampleIdx)
            {
                handleMidiEvent((*midiIterator).getMessage());
                ++midiIterator;
            }

            float synthSample = 0.0f;

            // 1. Kick Voice Processing
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

            // 2. Snare Voice Processing (Spectrogram-Calibrated)
            if (snareEnv > 1.0e-4f || snareNoiseEnv > 1.0e-4f)
            {
                // Body Tone: 172Hz fundamental with fast 280Hz initial pitch drop
                const float snareFreq = 172.0f + 110.0f * (snarePitchEnv * snarePitchEnv);
                const float snareSine = std::sin(snarePhase);
                const float snareTriangle = (std::abs(std::fmod(snarePhase / juce::MathConstants<float>::pi + 1.0f, 2.0f) - 1.0f) * 2.0f - 1.0f) * 0.4f;
                const float rawSnareBody = (snareSine + snareTriangle) * snareEnv;
                const float filteredSnareBody = snareBodyFilter.processSample(std::tanh(rawSnareBody * 1.4f));

                // Snare Noise Crack & Air
                const float rawNoise = nextRandomFloat() * 2.0f - 1.0f;
                const float wireNoise = snareWireFilter.processSample(rawNoise) * snareNoiseEnv * 1.1f;
                const float airNoise = snareAirFilter.processSample(rawNoise) * snareNoiseEnv * 0.65f;

                // Sharp Transient Snap (Crack Attack)
                float snapCrack = 0.0f;
                if (snareSnapEnv > 1.0e-3f)
                {
                    snapCrack = snareWireFilter.processSample(rawNoise) * snareSnapEnv * 1.2f;
                    snareSnapEnv *= snareSnapDecayCoef;
                }

                const float totalSnare = (filteredSnareBody * 0.95f) + wireNoise + airNoise + snapCrack;
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

    // Kick State
    float kickPhase { 0.0f };
    float kickHarmonicPhase { 0.0f };
    float kickEnv { 0.0f };
    float kickPitchEnv { 0.0f };
    float kickBeaterEnv { 0.0f };

    juce::dsp::IIR::Filter<float> kickBeaterFilter;
    juce::dsp::IIR::Filter<float> kickBodyFilter;

    // Snare State
    float snarePhase { 0.0f };
    float snareEnv { 0.0f };
    float snarePitchEnv { 0.0f };
    float snareNoiseEnv { 0.0f };
    float snareSnapEnv { 0.0f };

    juce::dsp::IIR::Filter<float> snareBodyFilter;
    juce::dsp::IIR::Filter<float> snareWireFilter;
    juce::dsp::IIR::Filter<float> snareAirFilter;
};

} // namespace underground::dsp
