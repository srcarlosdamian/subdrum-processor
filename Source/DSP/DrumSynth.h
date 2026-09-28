#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <atomic>

namespace underground::dsp
{

/**
 * @brief Authentic UK 2-Step / Underground Drum Synthesizer:
 *        - Sound 1 (KICK): Deep analog sub-bass, pitch drop, punch click beater, saturation, and Sampler Pitch Drop (Varispeed).
 *        - Sound 2 (SNARE): Calibrated UK garage snare/clap with punchy body, 4.5kHz crack, 9.8kHz sizzle, and Sampler Pitch Drop (Varispeed).
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

        // Snare Filters (Calibrated to 0.27s Spectrogram)
        snareBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 340.0f, 0.9f);
        snareBodyFilter.reset();

        snareSizzleFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 9800.0f, 1.4f);
        snareSizzleFilter.reset();

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

    // --- Kick Parameter Setters ---
    void setKickPitchSemi(float semi) noexcept { kickPitchSemi.store(semi, std::memory_order_relaxed); }
    void setKickTune(float hz) noexcept { kickBaseFreq.store(hz, std::memory_order_relaxed); }
    void setKickPitchSweep(float hz) noexcept { kickSweepDepth.store(hz, std::memory_order_relaxed); }
    void setKickDecay(float ms) noexcept { kickDecayMs.store(ms, std::memory_order_relaxed); }
    void setKickPunch(float norm) noexcept { kickPunchLevel.store(norm, std::memory_order_relaxed); }
    void setKickDrive(float driveNorm) noexcept { kickDriveAmount.store(driveNorm, std::memory_order_relaxed); }

    // --- Snare Parameter Setters ---
    void setSnarePitchSemi(float semi) noexcept { snarePitchSemi.store(semi, std::memory_order_relaxed); }
    void setSnareDecay(float ms) noexcept { snareDecayMs.store(ms, std::memory_order_relaxed); }
    void setSnareNoiseLevel(float norm) noexcept { snareNoiseLevel.store(norm, std::memory_order_relaxed); }
    void setSnareSnapLevel(float norm) noexcept { snareSnapLevel.store(norm, std::memory_order_relaxed); }
    void setSnareSizzleFreq(float hz) noexcept
    {
        if (std::abs(lastSizzleHz - hz) > 20.0f)
        {
            lastSizzleHz = hz;
            snareSizzleFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, juce::jlimit(4000.0f, 16000.0f, hz), 1.4f);
        }
    }
    void setSnareBodyLevel(float norm) noexcept { snareBodyLevel.store(norm, std::memory_order_relaxed); }
    void setSnareBodyTune(float hz) noexcept { snareBodyFreq.store(hz, std::memory_order_relaxed); }

    void handleMidiEvent(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            // Sound 1: KICK (Note 35, 36, 60 / Key 'A')
            if (note == 35 || note == 36 || note == 60)
            {
                kickEnv = vel * 1.35f;
                kickPitchEnv = 1.0f;
                kickBeaterEnv = vel * (0.4f + 0.9f * kickPunchLevel.load(std::memory_order_relaxed));
                kickPhase = 0.0f;
                kickHarmonicPhase = 0.0f;
            }
            // Sound 2: SNARE / CLAP (Note 38 / Key 'S')
            else if (note == 38)
            {
                snareEnv = vel * (1.3f * snareBodyLevel.load(std::memory_order_relaxed));
                snarePitchEnv = 1.0f;
                snareNoiseEnv = vel * (1.2f * snareNoiseLevel.load(std::memory_order_relaxed));
                snareSnapEnv = vel * (1.5f * snareSnapLevel.load(std::memory_order_relaxed));
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

        // Varispeed Sampler Pitch Ratios: R = 2^(semitones / 12)
        // Dropping pitch slows down decay time (1 / R) just like classic Akai/E-MU samplers!
        const float kSemi = kickPitchSemi.load(std::memory_order_relaxed);
        const float kPitchRatio = std::pow(2.0f, kSemi / 12.0f);
        const float kSpeedFactor = 1.0f / juce::jmax(0.2f, kPitchRatio);

        const float sSemi = snarePitchSemi.load(std::memory_order_relaxed);
        const float sPitchRatio = std::pow(2.0f, sSemi / 12.0f);
        const float sSpeedFactor = 1.0f / juce::jmax(0.2f, sPitchRatio);

        // Dynamic Kick Coefficients
        const float kDecaySec = (kickDecayMs.load(std::memory_order_relaxed) * 0.001f) * 0.45f * kSpeedFactor;
        const float kickDecayCoef = std::exp(-1.0f / (juce::jmax(0.005f, kDecaySec) * static_cast<float>(sampleRate)));
        const float pitchDecayCoef = std::exp(-1.0f / ((0.016f * kSpeedFactor) * static_cast<float>(sampleRate)));
        const float beaterDecayCoef = std::exp(-1.0f / ((0.006f * kSpeedFactor) * static_cast<float>(sampleRate)));

        const float kBase = kickBaseFreq.load(std::memory_order_relaxed) * kPitchRatio;
        const float kSweep = kickSweepDepth.load(std::memory_order_relaxed) * kPitchRatio;
        const float kDriveFactor = 1.2f + 0.8f * kickDriveAmount.load(std::memory_order_relaxed);

        // Dynamic Snare Coefficients
        const float sDecayMs = snareDecayMs.load(std::memory_order_relaxed) * sSpeedFactor;
        const float snareBodyDecayCoef = std::exp(-1.0f / (juce::jmax(0.01f, sDecayMs * 0.001f * 0.45f) * static_cast<float>(sampleRate)));
        const float snarePitchDecayCoef = std::exp(-1.0f / ((0.022f * sSpeedFactor) * static_cast<float>(sampleRate)));
        const float snareNoiseDecayCoef = std::exp(-1.0f / (juce::jmax(0.01f, sDecayMs * 0.001f * 0.50f) * static_cast<float>(sampleRate)));
        const float snareSnapDecayCoef = std::exp(-1.0f / ((0.012f * sSpeedFactor) * static_cast<float>(sampleRate)));

        const float sBodyBase = snareBodyFreq.load(std::memory_order_relaxed) * sPitchRatio;

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            while (midiIterator != midiMessages.cend() && (*midiIterator).samplePosition == sampleIdx)
            {
                handleMidiEvent((*midiIterator).getMessage());
                ++midiIterator;
            }

            float synthSample = 0.0f;

            // 1. Kick Voice (Sound 1 - Deep Analog Sub + Punch Click + Sampler Pitching)
            if (kickEnv > 1.0e-4f)
            {
                const float kickFreq = kBase + kSweep * (kickPitchEnv * kickPitchEnv);
                const float fund = std::sin(kickPhase);
                const float harm2 = std::sin(kickHarmonicPhase) * 0.35f;
                const float rawBody = (fund + harm2) * kickEnv;
                const float saturatedBody = std::tanh(rawBody * kDriveFactor);
                const float filteredBody = kickBodyFilter.processSample(saturatedBody);

                float beaterClick = 0.0f;
                if (kickBeaterEnv > 1.0e-3f)
                {
                    const float noise = nextRandomFloat() * 2.0f - 1.0f;
                    beaterClick = kickBeaterFilter.processSample(noise) * kickBeaterEnv * 0.75f;
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

            // 2. Snare Voice (Sound 2 - 0.27s Spectrogram Profile + Sampler Pitching)
            if (snareEnv > 1.0e-4f || snareNoiseEnv > 1.0e-4f || snareSnapEnv > 1.0e-3f)
            {
                // Low fundamental tone (pitched according to varispeed sampler)
                const float snareFreq = sBodyBase + (95.0f * sPitchRatio) * (snarePitchEnv * snarePitchEnv);
                const float fund = std::sin(snarePhase);
                const float filteredBody = snareBodyFilter.processSample(std::tanh(fund * snareEnv * 1.8f));

                // Sustained Noise Tail
                const float noise = nextRandomFloat() * 2.0f - 1.0f;
                const float sizzleNoise = snareSizzleFilter.processSample(noise) * snareNoiseEnv * 1.15f;

                // Initial attack crack
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

    // Kick State & Parameters
    std::atomic<float> kickPitchSemi { 0.0f }; // -24 to +12 semitones
    std::atomic<float> kickBaseFreq { 68.0f };
    std::atomic<float> kickSweepDepth { 77.0f };
    std::atomic<float> kickDecayMs { 72.0f };
    std::atomic<float> kickPunchLevel { 0.75f };
    std::atomic<float> kickDriveAmount { 0.5f };

    float kickPhase { 0.0f };
    float kickHarmonicPhase { 0.0f };
    float kickEnv { 0.0f };
    float kickPitchEnv { 0.0f };
    float kickBeaterEnv { 0.0f };

    juce::dsp::IIR::Filter<float> kickBeaterFilter;
    juce::dsp::IIR::Filter<float> kickBodyFilter;

    // Snare State & Parameters
    std::atomic<float> snarePitchSemi { 0.0f }; // -24 to +12 semitones
    std::atomic<float> snareDecayMs { 270.0f };
    std::atomic<float> snareNoiseLevel { 0.90f };
    std::atomic<float> snareSnapLevel { 0.85f };
    std::atomic<float> snareBodyLevel { 0.70f };
    std::atomic<float> snareBodyFreq { 150.0f };

    float lastSizzleHz { 9800.0f };

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
