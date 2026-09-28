#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace underground::dsp
{

/**
 * @brief High-precision 2-Voice Drum Synthesizer:
 *        - Sound 1 (KICK): 68Hz-85Hz fundamental, 145Hz pitch drop, 0.07s decay.
 *        - Sound 2 (2-STEP DRY CLAP / SNARE): Authentic UK 2-Step multi-burst pre-flam engine
 *          (4 micro-taps spaced at 0ms, 11ms, 22ms, 33ms) passing through a 4-band resonant
 *          formant filter bank (1150Hz wood, 2800Hz crack, 7200Hz sizzle, 360Hz rim knock).
 *        - All other sounds strictly muted.
 */
class DrumSynth
{
public:
    DrumSynth() = default;
    ~DrumSynth() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

        // Kick Filters
        kickBeaterFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 3800.0f, 1.2f);
        kickBeaterFilter.reset();

        kickBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 1200.0f, 0.707f);
        kickBodyFilter.reset();

        // Authentic UK 2-Step 4-Band Formant Filter Array
        // Band 1: Wooden Body / Hollow Box Formant (1150 Hz, Q = 3.2)
        clapWoodFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1150.0f, 3.2f);
        clapWoodFilter.reset();

        // Band 2: Dry Hand Slap & Crack Formant (2800 Hz, Q = 3.8)
        clapCrackFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 2800.0f, 3.8f);
        clapCrackFilter.reset();

        // Band 3: High Sizzle & S950 Air (7200 Hz, Q = 2.0)
        clapSizzleFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 7200.0f, 2.0f);
        clapSizzleFilter.reset();

        // Band 4: Tight Rim Knock (360 Hz, Q = 2.5)
        clapRimFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 360.0f, 2.5f);
        clapRimFilter.reset();

        reset();
    }

    void reset()
    {
        kickPhase = 0.0f;
        kickHarmonicPhase = 0.0f;
        kickEnv = 0.0f;
        kickPitchEnv = 0.0f;
        kickBeaterEnv = 0.0f;

        clapSampleCounter = 999999;
        clapBurst1Env = 0.0f;
        clapBurst2Env = 0.0f;
        clapBurst3Env = 0.0f;
        clapMainEnv = 0.0f;
        currentVelocity = 0.0f;

        rimTonePhase1 = 0.0f;
        rimTonePhase2 = 0.0f;
        rimToneEnv = 0.0f;

        kickBeaterFilter.reset();
        kickBodyFilter.reset();
        clapWoodFilter.reset();
        clapCrackFilter.reset();
        clapSizzleFilter.reset();
        clapRimFilter.reset();

        rngState = 0x98765432;
    }

    void handleMidiEvent(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            // Sound 1: KICK (Note 35, 36, 60)
            if (note == 35 || note == 36 || note == 60)
            {
                kickEnv = vel * 1.35f;
                kickPitchEnv = 1.0f;
                kickBeaterEnv = vel * 1.0f;
                kickPhase = 0.0f;
                kickHarmonicPhase = 0.0f;
            }
            // Sound 2: 2-STEP DRY CLAP / SNARE (Note 38 / key 'S')
            else if (note == 38)
            {
                currentVelocity = vel;
                clapSampleCounter = 0;
                clapBurst1Env = vel * 0.75f; // First micro-flam
                clapBurst2Env = 0.0f;
                clapBurst3Env = 0.0f;
                clapMainEnv = 0.0f;

                rimToneEnv = vel * 1.1f;
                rimTonePhase1 = 0.0f;
                rimTonePhase2 = 0.0f;
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

        // 2-Step Multi-burst Flam Timings (11ms, 22ms, 33ms)
        const int flam2Sample = static_cast<int>(0.011f * static_cast<float>(sampleRate));
        const int flam3Sample = static_cast<int>(0.022f * static_cast<float>(sampleRate));
        const int mainBurstSample = static_cast<int>(0.033f * static_cast<float>(sampleRate));

        // 2-Step Decay Rates (Ultra-Dry, snappy gating)
        const float microBurstDecay = std::exp(-1.0f / (0.0075f * static_cast<float>(sampleRate))); // 7.5ms fast micro-tap
        const float mainBurstDecay = std::exp(-1.0f / (0.095f * static_cast<float>(sampleRate)));   // 95ms tight dry decay
        const float rimToneDecay = std::exp(-1.0f / (0.032f * static_cast<float>(sampleRate)));     // 32ms wooden rim click

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

            // 2. Authentic UK 2-Step Dry Clap / Snare Engine (Sound 2)
            if (clapSampleCounter < static_cast<int>(0.35f * static_cast<float>(sampleRate)))
            {
                // Trigger subsequent micro-flams at exact sample delays
                if (clapSampleCounter == flam2Sample)
                    clapBurst2Env = currentVelocity * 0.85f;
                if (clapSampleCounter == flam3Sample)
                    clapBurst3Env = currentVelocity * 1.05f;
                if (clapSampleCounter == mainBurstSample)
                    clapMainEnv = currentVelocity * 1.45f;

                const float totalBurstNoise = (clapBurst1Env + clapBurst2Env + clapBurst3Env + clapMainEnv);

                if (totalBurstNoise > 1.0e-4f || rimToneEnv > 1.0e-4f)
                {
                    const float rawNoise = (nextRandomFloat() * 2.0f - 1.0f) * totalBurstNoise;

                    // Multi-Band Formant Filter Array
                    const float woodPart   = clapWoodFilter.processSample(rawNoise) * 1.25f;   // 1150Hz hollow wood
                    const float crackPart  = clapCrackFilter.processSample(rawNoise) * 1.55f;  // 2800Hz dry smack
                    const float sizzlePart = clapSizzleFilter.processSample(rawNoise) * 0.90f; // 7200Hz top sizzle
                    const float rimPart    = clapRimFilter.processSample(rawNoise) * 0.80f;    // 360Hz analog rim

                    // Dual Metallic / Wooden Inharmonic Rim Ping (340Hz + 890Hz)
                    const float rimSine1 = std::sin(rimTonePhase1) * 0.65f;
                    const float rimSine2 = std::sin(rimTonePhase2) * 0.35f;
                    const float rimTonal = (rimSine1 + rimSine2) * rimToneEnv;

                    const float combined2StepClap = woodPart + crackPart + sizzlePart + rimPart + rimTonal;
                    synthSample += std::tanh(combined2StepClap * 1.4f);

                    // Phase advancement for rim pings
                    rimTonePhase1 += twoPi * 340.0f * samplePeriod;
                    if (rimTonePhase1 >= twoPi) rimTonePhase1 -= twoPi;

                    rimTonePhase2 += twoPi * 890.0f * samplePeriod;
                    if (rimTonePhase2 >= twoPi) rimTonePhase2 -= twoPi;

                    // Decays
                    clapBurst1Env *= microBurstDecay;
                    clapBurst2Env *= microBurstDecay;
                    clapBurst3Env *= microBurstDecay;
                    clapMainEnv   *= mainBurstDecay;
                    rimToneEnv    *= rimToneDecay;
                }

                ++clapSampleCounter;
            }

            if (synthSample != 0.0f)
            {
                const float outSample = std::tanh(synthSample * 0.92f);
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

    // Authentic UK 2-Step Clap / Snare State (Sound 2)
    int clapSampleCounter { 999999 };
    float currentVelocity { 1.0f };
    float clapBurst1Env { 0.0f };
    float clapBurst2Env { 0.0f };
    float clapBurst3Env { 0.0f };
    float clapMainEnv { 0.0f };

    float rimTonePhase1 { 0.0f };
    float rimTonePhase2 { 0.0f };
    float rimToneEnv { 0.0f };

    // 4-Band Formant Filter Array
    juce::dsp::IIR::Filter<float> clapWoodFilter;   // 1150 Hz
    juce::dsp::IIR::Filter<float> clapCrackFilter;  // 2800 Hz
    juce::dsp::IIR::Filter<float> clapSizzleFilter; // 7200 Hz
    juce::dsp::IIR::Filter<float> clapRimFilter;    // 360 Hz
};

} // namespace underground::dsp
