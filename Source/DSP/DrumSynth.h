#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <atomic>

namespace underground::dsp
{

/**
 * @brief Authentic Underground Acoustic Clap & Bass Kick Drum Synthesizer:
 *        - Sound 1 (KICK): Deep analog sub-bass, pitch drop, punch click beater, saturation, varispeed pitch.
 *        - Sound 2 (ACOUSTIC CLAP): Single solid impact with organic wood/skin physical resonance,
 *          low-noise multi-formant shaping (680Hz, 1050Hz, 2600Hz), and tight acoustic snap.
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

        // Acoustic Clap Filters
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
        clapBurst1Env = 0.0f;
        clapBurst2Env = 0.0f;
        clapBurst3Env = 0.0f;
        clapBodyPhase1 = 0.0f;
        clapBodyPhase2 = 0.0f;
        clapBodyEnv = 0.0f;
        currentVelocity = 0.0f;

        kickBeaterFilter.reset();
        kickBodyFilter.reset();
        clapBodyLowFilter.reset();
        clapWoodFilter.reset();
        clapSlapFilter.reset();
        clapAirFilter.reset();
        clapToneLowPass.reset();

        rngState = 0x98765432;
    }

    // --- Kick Parameter Setters ---
    void setKickPitchSemi(float semi) noexcept { kickPitchSemi.store(semi, std::memory_order_relaxed); }
    void setKickTune(float hz) noexcept { kickBaseFreq.store(hz, std::memory_order_relaxed); }
    void setKickPitchSweep(float hz) noexcept { kickSweepDepth.store(hz, std::memory_order_relaxed); }
    void setKickDecay(float ms) noexcept { kickDecayMs.store(ms, std::memory_order_relaxed); }
    void setKickPunch(float norm) noexcept { kickPunchLevel.store(norm, std::memory_order_relaxed); }
    void setKickDrive(float driveNorm) noexcept { kickDriveAmount.store(driveNorm, std::memory_order_relaxed); }

    // --- Acoustic Clap Parameter Setters ---
    void setSnarePitchSemi(float semi) noexcept { snarePitchSemi.store(semi, std::memory_order_relaxed); }
    void setSnareDecay(float ms) noexcept { snareDecayMs.store(ms, std::memory_order_relaxed); }
    void setSnareWoodLevel(float norm) noexcept { snareWoodLevel.store(norm, std::memory_order_relaxed); }
    void setSnareSlapLevel(float norm) noexcept { snareSlapLevel.store(norm, std::memory_order_relaxed); }
    void setSnareToneCutoff(float hz) noexcept
    {
        if (std::abs(lastToneHz - hz) > 20.0f)
        {
            lastToneHz = hz;
            clapToneLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, juce::jlimit(800.0f, 18000.0f, hz), 0.707f);
        }
    }
    void setSnareRoomTail(float norm) noexcept { snareRoomTail.store(norm, std::memory_order_relaxed); }

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
            // Sound 2: SINGLE ACOUSTIC CLAP (Note 38 / Key 'S')
            else if (note == 38)
            {
                currentVelocity = vel;
                clapSampleCounter = 0;
                // Single coherent strike: 3 ultra-fast micro-taps within 0-6ms for natural palm rattle
                clapBurst1Env = vel * 1.4f;
                clapBurst2Env = 0.0f;
                clapBurst3Env = 0.0f;

                // Organic acoustic wood/body resonant impulse
                clapBodyEnv = vel * 1.25f;
                clapBodyPhase1 = 0.0f;
                clapBodyPhase2 = 0.0f;
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

        // Dynamic Acoustic Clap Coefficients (Instant Single Strike)
        const float sDecaySec = (snareDecayMs.load(std::memory_order_relaxed) * 0.001f) * sSpeedFactor;
        const float woodGain = snareWoodLevel.load(std::memory_order_relaxed);
        const float slapGain = snareSlapLevel.load(std::memory_order_relaxed);
        const float roomGain = snareRoomTail.load(std::memory_order_relaxed);

        // Micro-flam spacing tightly packed inside first 6ms for organic hand texture
        const int tap2Sample = static_cast<int>(0.0028f * static_cast<float>(sampleRate));
        const int tap3Sample = static_cast<int>(0.0058f * static_cast<float>(sampleRate));

        const float microDecay = std::exp(-1.0f / ((0.009f * sSpeedFactor) * static_cast<float>(sampleRate)));
        const float mainDecay  = std::exp(-1.0f / (juce::jmax(0.01f, sDecaySec * 0.38f) * static_cast<float>(sampleRate)));
        const float bodyDecay  = std::exp(-1.0f / ((0.032f * sSpeedFactor) * static_cast<float>(sampleRate)));

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

            // 2. Single Solid Acoustic Clap Engine (Sound 2)
            if (clapSampleCounter < static_cast<int>(0.50f * static_cast<float>(sampleRate)))
            {
                if (clapSampleCounter == tap2Sample)
                    clapBurst2Env = currentVelocity * 1.15f;
                if (clapSampleCounter == tap3Sample)
                    clapBurst3Env = currentVelocity * 1.85f;

                const float totalBurst = (clapBurst1Env + clapBurst2Env + clapBurst3Env);

                if (totalBurst > 1.0e-4f || clapBodyEnv > 1.0e-4f)
                {
                    const float rawNoise = (nextRandomFloat() * 2.0f - 1.0f) * totalBurst;

                    // Organic Acoustic Formants (Wood + Skin Palm + Hard Slap)
                    const float woodPart  = clapWoodFilter.processSample(rawNoise)      * (1.55f * woodGain); // 1050Hz wood body
                    const float lowWood   = clapBodyLowFilter.processSample(rawNoise)   * (1.20f * woodGain); // 680Hz warm hollow box
                    const float slapPart  = clapSlapFilter.processSample(rawNoise)      * (1.65f * slapGain); // 2600Hz palm slap
                    const float airPart   = clapAirFilter.processSample(rawNoise)       * (0.85f * roomGain); // 5800Hz air tail

                    // Dual Acoustic Tonal Resonators for Organic Physical Impact (240Hz & 580Hz)
                    const float bodySine1 = std::sin(clapBodyPhase1) * 0.65f;
                    const float bodySine2 = std::sin(clapBodyPhase2) * 0.35f;
                    const float acousticTonal = (bodySine1 + bodySine2) * (clapBodyEnv * woodGain * 1.1f);

                    clapBodyPhase1 += twoPi * (240.0f * sPitchRatio) * samplePeriod;
                    if (clapBodyPhase1 >= twoPi) clapBodyPhase1 -= twoPi;

                    clapBodyPhase2 += twoPi * (580.0f * sPitchRatio) * samplePeriod;
                    if (clapBodyPhase2 >= twoPi) clapBodyPhase2 -= twoPi;

                    const float rawClapSum = woodPart + lowWood + slapPart + airPart + acousticTonal;

                    // Low-Pass Tone Control to filter out unwanted digital noise
                    const float filteredClap = clapToneLowPass.processSample(rawClapSum);

                    // Solid non-linear saturation for hard, punchy acoustic snap
                    const float saturatedClap = std::tanh(filteredClap * 1.6f);

                    synthSample += saturatedClap * 1.25f;

                    clapBurst1Env *= microDecay;
                    clapBurst2Env *= microDecay;
                    clapBurst3Env *= mainDecay;
                    clapBodyEnv   *= bodyDecay;
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
        // 1. Warm Hollow Wood Resonance (680 Hz, Q=2.5)
        clapBodyLowFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 680.0f, 2.5f);
        clapBodyLowFilter.reset();

        // 2. Organic Palm / Acoustic Wood Formant (1050 Hz, Q=3.0)
        clapWoodFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1050.0f, 3.0f);
        clapWoodFilter.reset();

        // 3. Hard Acoustic Palm Slap Smack (2600 Hz, Q=2.8)
        clapSlapFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 2600.0f, 2.8f);
        clapSlapFilter.reset();

        // 4. Subtle Air Resonance (5800 Hz, Q=1.6)
        clapAirFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 5800.0f, 1.6f);
        clapAirFilter.reset();

        // 5. Low-Pass Tone Filter (rolls off harsh high noise above 7.5 kHz)
        clapToneLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 7500.0f, 0.707f);
        clapToneLowPass.reset();
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

    // Single Solid Acoustic Clap State & Parameters
    std::atomic<float> snarePitchSemi { 0.0f };     // -24 to +12 semitones
    std::atomic<float> snareDecayMs { 180.0f };     // 180ms punchy tight acoustic decay
    std::atomic<float> snareWoodLevel { 0.85f };    // 680Hz + 1050Hz organic wood formants
    std::atomic<float> snareSlapLevel { 0.90f };    // 2600Hz palm slap bite
    std::atomic<float> snareRoomTail { 0.40f };     // 5800Hz air
    float lastToneHz { 7500.0f };

    int clapSampleCounter { 999999 };
    float currentVelocity { 1.0f };
    float clapBurst1Env { 0.0f };
    float clapBurst2Env { 0.0f };
    float clapBurst3Env { 0.0f };

    float clapBodyPhase1 { 0.0f };
    float clapBodyPhase2 { 0.0f };
    float clapBodyEnv { 0.0f };

    juce::dsp::IIR::Filter<float> clapBodyLowFilter;
    juce::dsp::IIR::Filter<float> clapWoodFilter;
    juce::dsp::IIR::Filter<float> clapSlapFilter;
    juce::dsp::IIR::Filter<float> clapAirFilter;
    juce::dsp::IIR::Filter<float> clapToneLowPass;
};

} // namespace underground::dsp
