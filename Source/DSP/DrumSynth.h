#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <atomic>

namespace underground::dsp
{

/**
 * @brief High-precision 2-Voice Drum Synthesizer with independent per-voice acoustic & noise sculpting:
 *        - Sound 1 (KICK): Controllable Tune (45-90Hz), Pitch Sweep, Decay, Punch Click, Drive.
 *        - Sound 2 (2-STEP CLAP / SNARE): Controllable Decay, Noise Level, Noise Color/Tone,
 *          Brightness, Body Level (0-100% to remove all woodiness), Body Tune, Flam Spread.
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

        // Snare / Clap Filters
        updateSnareFilters();

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

        snareBodyPhase = 0.0f;
        snareBodyEnv = 0.0f;

        kickBeaterFilter.reset();
        kickBodyFilter.reset();
        snareNoiseLowPass.reset();
        snareNoiseHighPass.reset();
        snareWoodFilter.reset();
        snareCrackFilter.reset();
        snareShimmerFilter.reset();

        rngState = 0x98765432;
    }

    // --- Kick Parameter Setters ---
    void setKickTune(float hz) noexcept { kickBaseFreq.store(hz, std::memory_order_relaxed); }
    void setKickPitchSweep(float hz) noexcept { kickSweepDepth.store(hz, std::memory_order_relaxed); }
    void setKickDecay(float ms) noexcept { kickDecayMs.store(ms, std::memory_order_relaxed); }
    void setKickPunch(float norm) noexcept { kickPunchLevel.store(norm, std::memory_order_relaxed); }
    void setKickDrive(float driveNorm) noexcept { kickDriveAmount.store(driveNorm, std::memory_order_relaxed); }

    // --- Snare / Clap Parameter Setters ---
    void setSnareDecay(float ms) noexcept { snareDecayMs.store(ms, std::memory_order_relaxed); }
    void setSnareNoiseLevel(float norm) noexcept { snareNoiseLevel.store(norm, std::memory_order_relaxed); }
    void setSnareNoiseTone(float cutoffHz) noexcept
    {
        if (std::abs(lastNoiseToneHz - cutoffHz) > 10.0f)
        {
            lastNoiseToneHz = cutoffHz;
            snareNoiseLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, juce::jlimit(800.0f, 18000.0f, cutoffHz), 0.707f);
        }
    }
    void setSnareBrightness(float shelfHz) noexcept
    {
        if (std::abs(lastBrightnessHz - shelfHz) > 10.0f)
        {
            lastBrightnessHz = shelfHz;
            snareShimmerFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, juce::jlimit(2000.0f, 16000.0f, shelfHz), 0.707f);
        }
    }
    void setSnareBodyLevel(float norm) noexcept { snareBodyLevel.store(norm, std::memory_order_relaxed); }
    void setSnareBodyTune(float hz) noexcept { snareBodyFreq.store(hz, std::memory_order_relaxed); }
    void setSnareFlam(float ms) noexcept { snareFlamMs.store(ms, std::memory_order_relaxed); }

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
                kickBeaterEnv = vel * kickPunchLevel.load(std::memory_order_relaxed);
                kickPhase = 0.0f;
                kickHarmonicPhase = 0.0f;
            }
            // Sound 2: 2-STEP DRY CLAP / SNARE (Note 38 / key 'S')
            else if (note == 38)
            {
                currentVelocity = vel;
                clapSampleCounter = 0;
                clapBurst1Env = vel * 0.9f;
                clapBurst2Env = 0.0f;
                clapBurst3Env = 0.0f;
                clapMainEnv = 0.0f;

                snareBodyEnv = vel * snareBodyLevel.load(std::memory_order_relaxed);
                snareBodyPhase = 0.0f;
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

        // Dynamic Kick Coefficients
        const float kDecaySec = kickDecayMs.load(std::memory_order_relaxed) * 0.001f;
        const float kickDecayCoef = std::exp(-1.0f / (kDecaySec * static_cast<float>(sampleRate)));
        const float pitchDecayCoef = std::exp(-1.0f / (0.016f * static_cast<float>(sampleRate)));
        const float beaterDecayCoef = std::exp(-1.0f / (0.006f * static_cast<float>(sampleRate)));
        const float kBase = kickBaseFreq.load(std::memory_order_relaxed);
        const float kSweep = kickSweepDepth.load(std::memory_order_relaxed);
        const float kDrive = 1.0f + kickDriveAmount.load(std::memory_order_relaxed) * 1.5f;

        // Dynamic Snare / Clap Coefficients
        const float sDecaySec = snareDecayMs.load(std::memory_order_relaxed) * 0.001f;
        const float sFlamSec = snareFlamMs.load(std::memory_order_relaxed) * 0.001f;
        const float sNoiseGain = snareNoiseLevel.load(std::memory_order_relaxed);
        const float sBodyGain = snareBodyLevel.load(std::memory_order_relaxed);
        const float sBodyPitch = snareBodyFreq.load(std::memory_order_relaxed);

        const int flam2Sample = static_cast<int>(sFlamSec * 1.0f * static_cast<float>(sampleRate));
        const int flam3Sample = static_cast<int>(sFlamSec * 2.0f * static_cast<float>(sampleRate));
        const int mainBurstSample = static_cast<int>(sFlamSec * 3.0f * static_cast<float>(sampleRate));

        const float microBurstDecay = std::exp(-1.0f / (0.006f * static_cast<float>(sampleRate)));
        const float mainBurstDecay = std::exp(-1.0f / (sDecaySec * static_cast<float>(sampleRate)));
        const float bodyDecay = std::exp(-1.0f / (0.045f * static_cast<float>(sampleRate)));

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
                const float kickFreq = kBase + kSweep * (kickPitchEnv * kickPitchEnv);
                const float fund = std::sin(kickPhase);
                const float harm2 = std::sin(kickHarmonicPhase) * 0.35f;
                const float rawBody = (fund + harm2) * kickEnv;
                const float saturatedBody = std::tanh(rawBody * kDrive);
                const float filteredBody = kickBodyFilter.processSample(saturatedBody);

                float beaterClick = 0.0f;
                if (kickBeaterEnv > 1.0e-3f)
                {
                    const float noise = nextRandomFloat() * 2.0f - 1.0f;
                    beaterClick = kickBeaterFilter.processSample(noise) * kickBeaterEnv * 0.85f;
                    kickBeaterEnv *= beaterDecayCoef;
                }

                synthSample += filteredBody * 1.35f + beaterClick;

                kickPhase += twoPi * kickFreq * samplePeriod;
                if (kickPhase >= twoPi) kickPhase -= twoPi;

                kickHarmonicPhase += twoPi * (kickFreq * 2.0f) * samplePeriod;
                if (kickHarmonicPhase >= twoPi) kickHarmonicPhase -= twoPi;

                kickEnv *= kickDecayCoef;
                kickPitchEnv *= pitchDecayCoef;
            }

            // 2. 2-Step Dry Snare / Clap Voice Processing
            if (clapSampleCounter < static_cast<int>(0.35f * static_cast<float>(sampleRate)))
            {
                if (sFlamSec > 0.001f)
                {
                    if (clapSampleCounter == flam2Sample)
                        clapBurst2Env = currentVelocity * 0.95f;
                    if (clapSampleCounter == flam3Sample)
                        clapBurst3Env = currentVelocity * 1.15f;
                    if (clapSampleCounter == mainBurstSample)
                        clapMainEnv = currentVelocity * 1.65f;
                }
                else
                {
                    if (clapSampleCounter == 0)
                        clapMainEnv = currentVelocity * 1.65f;
                }

                const float totalBurstNoise = (clapBurst1Env + clapBurst2Env + clapBurst3Env + clapMainEnv) * sNoiseGain;

                if (totalBurstNoise > 1.0e-4f || snareBodyEnv > 1.0e-4f)
                {
                    const float rawNoise = (nextRandomFloat() * 2.0f - 1.0f) * totalBurstNoise;

                    // Multi-Stage Tone & Brightness Filtering
                    const float toneFilteredNoise = snareNoiseLowPass.processSample(rawNoise);
                    const float crackBand = snareCrackFilter.processSample(toneFilteredNoise) * 1.4f;
                    const float shimmerHigh = snareShimmerFilter.processSample(rawNoise) * 0.8f;
                    const float woodRes = snareWoodFilter.processSample(rawNoise) * (sBodyGain * 0.8f);

                    // Pitch Body Click (only if sBodyGain > 0)
                    float bodyPing = 0.0f;
                    if (sBodyGain > 0.01f)
                    {
                        bodyPing = std::sin(snareBodyPhase) * snareBodyEnv * sBodyGain * 0.6f;
                        snareBodyPhase += twoPi * sBodyPitch * samplePeriod;
                        if (snareBodyPhase >= twoPi) snareBodyPhase -= twoPi;
                        snareBodyEnv *= bodyDecay;
                    }

                    const float totalClap = crackBand + shimmerHigh + woodRes + bodyPing;
                    synthSample += std::tanh(totalClap * 1.45f);

                    clapBurst1Env *= microBurstDecay;
                    clapBurst2Env *= microBurstDecay;
                    clapBurst3Env *= microBurstDecay;
                    clapMainEnv   *= mainBurstDecay;
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
    void updateSnareFilters()
    {
        snareNoiseLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 7500.0f, 0.707f);
        snareCrackFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 2600.0f, 2.5f);
        snareShimmerFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 6200.0f, 0.707f);
        snareWoodFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1100.0f, 2.8f);
    }

    inline float nextRandomFloat() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return static_cast<float>(rngState & 0x00FFFFFF) / static_cast<float>(0x01000000);
    }

    double sampleRate { 44100.0 };
    uint32_t rngState { 0x98765432 };

    // Kick State & Parameters
    std::atomic<float> kickBaseFreq { 62.0f };
    std::atomic<float> kickSweepDepth { 85.0f };
    std::atomic<float> kickDecayMs { 75.0f };
    std::atomic<float> kickPunchLevel { 0.8f };
    std::atomic<float> kickDriveAmount { 0.5f };

    float kickPhase { 0.0f };
    float kickHarmonicPhase { 0.0f };
    float kickEnv { 0.0f };
    float kickPitchEnv { 0.0f };
    float kickBeaterEnv { 0.0f };

    juce::dsp::IIR::Filter<float> kickBeaterFilter;
    juce::dsp::IIR::Filter<float> kickBodyFilter;

    // Snare / Clap State & Parameters
    std::atomic<float> snareDecayMs { 45.0f };
    std::atomic<float> snareNoiseLevel { 1.0f };
    std::atomic<float> snareBodyLevel { 0.0f }; // 0% by default for pure dry electronic snap!
    std::atomic<float> snareBodyFreq { 180.0f };
    std::atomic<float> snareFlamMs { 7.5f };

    float lastNoiseToneHz { 7500.0f };
    float lastBrightnessHz { 6200.0f };

    int clapSampleCounter { 999999 };
    float currentVelocity { 1.0f };
    float clapBurst1Env { 0.0f };
    float clapBurst2Env { 0.0f };
    float clapBurst3Env { 0.0f };
    float clapMainEnv { 0.0f };

    float snareBodyPhase { 0.0f };
    float snareBodyEnv { 0.0f };

    juce::dsp::IIR::Filter<float> snareNoiseLowPass;
    juce::dsp::IIR::Filter<float> snareNoiseHighPass;
    juce::dsp::IIR::Filter<float> snareCrackFilter;
    juce::dsp::IIR::Filter<float> snareShimmerFilter;
    juce::dsp::IIR::Filter<float> snareWoodFilter;
};

} // namespace underground::dsp
