#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <atomic>

namespace underground::dsp
{

/**
 * @brief Authentic UK 2-Step & Burial Acoustic Clap / Hard Snare Drum Synthesizer:
 *        - Sound 1 (KICK): Deep analog sub-bass, pitch drop, punch click beater, saturation, varispeed pitch drop.
 *        - Sound 2 (BURIAL CLAP / HARD SNARE):
 *          * Multi-transient acoustic flam (wooden pre-tap at t=0 + hard delayed slap at t=flamDelay).
 *          * Resonant acoustic wood/skin formants (1100Hz + 2900Hz + 5400Hz + 8500Hz).
 *          * Hard saturation clipper for aggressive, punchy impact.
 *          * Smooth diffuse acoustic room tail.
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

        // Burial Acoustic Clap Filter Array
        updateClapFilters();

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
        preTapEnv = 0.0f;
        clapMicroBurst1 = 0.0f;
        clapMicroBurst2 = 0.0f;
        clapMainEnv = 0.0f;
        currentVelocity = 0.0f;

        kickBeaterFilter.reset();
        kickBodyFilter.reset();
        clapPreWoodFilter.reset();
        clapWoodFilter.reset();
        clapSlapFilter.reset();
        clapCrackFilter.reset();
        clapSizzleFilter.reset();

        rngState = 0x98765432;
    }

    // --- Kick Parameter Setters ---
    void setKickPitchSemi(float semi) noexcept { kickPitchSemi.store(semi, std::memory_order_relaxed); }
    void setKickTune(float hz) noexcept { kickBaseFreq.store(hz, std::memory_order_relaxed); }
    void setKickPitchSweep(float hz) noexcept { kickSweepDepth.store(hz, std::memory_order_relaxed); }
    void setKickDecay(float ms) noexcept { kickDecayMs.store(ms, std::memory_order_relaxed); }
    void setKickPunch(float norm) noexcept { kickPunchLevel.store(norm, std::memory_order_relaxed); }
    void setKickDrive(float driveNorm) noexcept { kickDriveAmount.store(driveNorm, std::memory_order_relaxed); }

    // --- Burial Clap / Snare Parameter Setters ---
    void setSnarePitchSemi(float semi) noexcept { snarePitchSemi.store(semi, std::memory_order_relaxed); }
    void setSnareDecay(float ms) noexcept { snareDecayMs.store(ms, std::memory_order_relaxed); }
    void setSnareWoodLevel(float norm) noexcept { snareWoodLevel.store(norm, std::memory_order_relaxed); }
    void setSnareSlapLevel(float norm) noexcept { snareSlapLevel.store(norm, std::memory_order_relaxed); }
    void setSnareSizzleLevel(float norm) noexcept { snareSizzleLevel.store(norm, std::memory_order_relaxed); }
    void setSnareFlamMs(float ms) noexcept { snareFlamMs.store(ms, std::memory_order_relaxed); }

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
            // Sound 2: BURIAL ACOUSTIC CLAP / HARD SNARE (Note 38 / Key 'S')
            else if (note == 38)
            {
                currentVelocity = vel;
                clapSampleCounter = 0;
                preTapEnv = vel * 0.95f; // Initial pre-tap wood tick at t=0
                clapMicroBurst1 = 0.0f;
                clapMicroBurst2 = 0.0f;
                clapMainEnv = 0.0f;
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

        // Dynamic Burial Clap Coefficients
        const float sDecaySec = (snareDecayMs.load(std::memory_order_relaxed) * 0.001f) * sSpeedFactor;
        const float flamSec = (snareFlamMs.load(std::memory_order_relaxed) * 0.001f) * sSpeedFactor;

        const int flamSample = static_cast<int>(flamSec * static_cast<float>(sampleRate));
        const int micro1Sample = juce::jmax(0, flamSample - static_cast<int>(0.012f * static_cast<float>(sampleRate)));
        const int micro2Sample = juce::jmax(0, flamSample - static_cast<int>(0.006f * static_cast<float>(sampleRate)));

        const float preTapDecay = std::exp(-1.0f / ((0.038f * sSpeedFactor) * static_cast<float>(sampleRate)));
        const float microDecay  = std::exp(-1.0f / ((0.008f * sSpeedFactor) * static_cast<float>(sampleRate)));
        const float mainSlapDecay = std::exp(-1.0f / (juce::jmax(0.01f, sDecaySec * 0.42f) * static_cast<float>(sampleRate)));

        const float woodGain   = snareWoodLevel.load(std::memory_order_relaxed);
        const float slapGain   = snareSlapLevel.load(std::memory_order_relaxed);
        const float sizzleGain = snareSizzleLevel.load(std::memory_order_relaxed);

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

            // 2. Burial Acoustic Hard Clap Engine (Sound 2)
            if (clapSampleCounter < static_cast<int>(0.60f * static_cast<float>(sampleRate)))
            {
                // Trigger micro-flams and delayed main slap strike
                if (flamSample > 0)
                {
                    if (clapSampleCounter == micro1Sample)
                        clapMicroBurst1 = currentVelocity * 0.85f;
                    if (clapSampleCounter == micro2Sample)
                        clapMicroBurst2 = currentVelocity * 1.10f;
                    if (clapSampleCounter == flamSample)
                        clapMainEnv = currentVelocity * 2.20f; // Heavy hard slap impact
                }
                else
                {
                    if (clapSampleCounter == 0)
                        clapMainEnv = currentVelocity * 2.20f;
                }

                float clapOutput = 0.0f;

                // A. Wooden Pre-tap at t=0 (as seen in spectrogram 0.00s-0.08s)
                if (preTapEnv > 1.0e-4f)
                {
                    const float preNoise = (nextRandomFloat() * 2.0f - 1.0f) * preTapEnv;
                    const float woodPre = clapPreWoodFilter.processSample(preNoise) * (1.1f * woodGain);
                    clapOutput += woodPre;
                    preTapEnv *= preTapDecay;
                }

                // B. Hard Main Acoustic Slap Strike + Room Tail (as seen at t=0.145s)
                const float slapNoiseBurst = (clapMicroBurst1 + clapMicroBurst2 + clapMainEnv);
                if (slapNoiseBurst > 1.0e-4f)
                {
                    const float rawNoise = (nextRandomFloat() * 2.0f - 1.0f) * slapNoiseBurst;

                    // Formant Filter Bank for Organic Handclap & Acoustic Wood
                    const float woodPart   = clapWoodFilter.processSample(rawNoise)   * (1.45f * woodGain);   // 1100Hz skin/wood
                    const float slapPart   = clapSlapFilter.processSample(rawNoise)   * (1.75f * slapGain);   // 2900Hz hard bite smack
                    const float crackPart  = clapCrackFilter.processSample(rawNoise)  * (1.30f * slapGain);   // 5400Hz snap
                    const float sizzlePart = clapSizzleFilter.processSample(rawNoise) * (1.10f * sizzleGain); // 8500Hz air

                    const float combinedSlap = (woodPart + slapPart + crackPart + sizzlePart);

                    // Hard non-linear acoustic clipper for punchy "duro" impact
                    clapOutput += std::tanh(combinedSlap * 1.65f);

                    clapMicroBurst1 *= microDecay;
                    clapMicroBurst2 *= microDecay;
                    clapMainEnv     *= mainSlapDecay;
                }

                if (clapOutput != 0.0f)
                {
                    synthSample += clapOutput * 1.15f;
                }

                ++clapSampleCounter;
            }

            if (synthSample != 0.0f)
            {
                const float outSample = std::tanh(synthSample * 0.95f);
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    buffer.addSample(ch, sampleIdx, outSample);
                }
            }
        }
    }

private:
    void updateClapFilters()
    {
        // Acoustic Pre-Tap Filter (850Hz hollow wood tick)
        clapPreWoodFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 850.0f, 1.8f);
        clapPreWoodFilter.reset();

        // 1. Acoustic Wood / Handclap Skin Formant (1100 Hz, Q=2.2)
        clapWoodFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1100.0f, 2.2f);
        clapWoodFilter.reset();

        // 2. Hard Slap Smack Formant (2900 Hz, Q=2.4)
        clapSlapFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 2900.0f, 2.4f);
        clapSlapFilter.reset();

        // 3. Crisp Snap Transient (5400 Hz, Q=1.8)
        clapCrackFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 5400.0f, 1.8f);
        clapCrackFilter.reset();

        // 4. High Air Sizzle (8500 Hz High-Shelf / Bandpass)
        clapSizzleFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 7500.0f, 0.707f);
        clapSizzleFilter.reset();
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

    // Burial Acoustic Clap State & Parameters
    std::atomic<float> snarePitchSemi { 0.0f };     // -24 to +12 semitones
    std::atomic<float> snareDecayMs { 320.0f };     // 320ms matching 0.38s spectrogram tail
    std::atomic<float> snareWoodLevel { 0.85f };    // 1100Hz organic acoustic wood
    std::atomic<float> snareSlapLevel { 0.90f };    // 2900Hz hard bite smack
    std::atomic<float> snareSizzleLevel { 0.75f };  // 7500Hz air sizzle
    std::atomic<float> snareFlamMs { 135.0f };      // 135ms matching the exact delayed slap peak in spectrogram!

    int clapSampleCounter { 999999 };
    float currentVelocity { 1.0f };
    float preTapEnv { 0.0f };
    float clapMicroBurst1 { 0.0f };
    float clapMicroBurst2 { 0.0f };
    float clapMainEnv { 0.0f };

    juce::dsp::IIR::Filter<float> clapPreWoodFilter;
    juce::dsp::IIR::Filter<float> clapWoodFilter;
    juce::dsp::IIR::Filter<float> clapSlapFilter;
    juce::dsp::IIR::Filter<float> clapCrackFilter;
    juce::dsp::IIR::Filter<float> clapSizzleFilter;
};

} // namespace underground::dsp
