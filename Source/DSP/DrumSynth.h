#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <atomic>
#include <array>
#include <algorithm>

namespace underground::dsp
{

/**
 * @brief Complete 8-Voice Underground Drum Synthesizer:
 *        - Pad 1 (KICK, Note 36): Deep analog sub, pitch sweep, punch click beater, saturation.
 *        - Pad 2 (ACOUSTIC CLAP, Note 38): Single cohesive strike, wood/skin formants, noise filter.
 *        - Pad 3 (CLOSED HI-HAT, Note 42): 6-oscillator metallic cluster, highpass 7kHz, crisp 45ms decay.
 *        - Pad 4 (OPEN HI-HAT, Note 46): Metallic wash, 320ms decay, choked by Closed Hat.
 *        - Pad 5 (VINYL DUST, Note 48): Needle contact hiss and authentic vinyl dust crackle impulse.
 *        - Pad 6 (RIMSHOT, Note 37): Resonant hollow wood rim tap (480Hz & 1680Hz double peak).
 *        - Pad 7 (808 SUB BASS, Note 39): Pure deep 42Hz sub sine with warm saturation and 450ms sustain.
 *        - Pad 8 (SHAKER, Note 40): Organic soft-attack percussion rattle with high bandpass filter.
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

        // Hi-Hat Filters
        hatHighPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 7200.0f, 1.0f);
        hatHighPass.reset();
        hatBandPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 9500.0f, 2.0f);
        hatBandPass.reset();

        // Rimshot Filters
        rimBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 480.0f, 4.0f);
        rimBodyFilter.reset();
        rimSnapFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1680.0f, 3.5f);
        rimSnapFilter.reset();

        // Shaker Filter
        shakerFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 6200.0f, 1.8f);
        shakerFilter.reset();

        reset();
    }

    void reset()
    {
        // Kick
        kickPhase = 0.0f;
        kickHarmonicPhase = 0.0f;
        kickEnv = 0.0f;
        kickPitchEnv = 0.0f;
        kickBeaterEnv = 0.0f;

        // Clap
        clapSampleCounter = 999999;
        clapBurst1Env = 0.0f;
        clapBurst2Env = 0.0f;
        clapBurst3Env = 0.0f;
        clapBodyPhase1 = 0.0f;
        clapBodyPhase2 = 0.0f;
        clapBodyEnv = 0.0f;
        currentVelocity = 0.0f;

        // Hats
        closedHatEnv = 0.0f;
        openHatEnv = 0.0f;
        hatPhases.fill(0.0f);

        // Vinyl Dust
        vinylEnv = 0.0f;

        // Rimshot
        rimEnv = 0.0f;
        rimPhase = 0.0f;

        // 808 Sub
        subEnv = 0.0f;
        subPhase = 0.0f;

        // Shaker
        shakerEnv = 0.0f;

        // Sidechain Ducking
        kickDuckEnv = 0.0f;

        kickBeaterFilter.reset();
        kickBodyFilter.reset();
        clapBodyLowFilter.reset();
        clapWoodFilter.reset();
        clapSlapFilter.reset();
        clapAirFilter.reset();
        clapToneLowPass.reset();
        hatHighPass.reset();
        hatBandPass.reset();
        rimBodyFilter.reset();
        rimSnapFilter.reset();
        shakerFilter.reset();

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

    float getKickDuckLevel() const noexcept
    {
        return kickDuckEnv;
    }

    void handleMidiEvent(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            // 1. KICK (Note 35, 36, 60 / Key 'A')
            if (note == 35 || note == 36 || note == 60)
            {
                kickEnv = vel * 1.35f;
                kickPitchEnv = 1.0f;
                kickBeaterEnv = vel * (0.4f + 0.9f * kickPunchLevel.load(std::memory_order_relaxed));
                kickPhase = 0.0f;
                kickHarmonicPhase = 0.0f;
                kickDuckEnv = 1.0f; // Trigger sidechain pump
            }
            // 2. SINGLE ACOUSTIC CLAP (Note 38 / Key 'S')
            else if (note == 38)
            {
                currentVelocity = vel;
                clapSampleCounter = 0;
                clapBurst1Env = vel * 1.4f;
                clapBurst2Env = 0.0f;
                clapBurst3Env = 0.0f;
                clapBodyEnv = vel * 1.25f;
                clapBodyPhase1 = 0.0f;
                clapBodyPhase2 = 0.0f;
            }
            // 3. CLOSED HI-HAT (Note 42, 44 / Key 'D')
            else if (note == 42 || note == 44)
            {
                closedHatEnv = vel * 1.2f;
                openHatEnv = 0.0f; // Choke open hat
            }
            // 4. OPEN HI-HAT (Note 46 / Key 'F')
            else if (note == 46)
            {
                openHatEnv = vel * 1.35f;
            }
            // 5. VINYL DUST / TEXTURE (Note 48 / Key 'G')
            else if (note == 48)
            {
                vinylEnv = vel * 1.1f;
            }
            // 6. RIMSHOT (Note 37 / Key 'H')
            else if (note == 37)
            {
                rimEnv = vel * 1.3f;
                rimPhase = 0.0f;
            }
            // 7. 808 SUB BASS (Note 39 / Key 'J')
            else if (note == 39)
            {
                subEnv = vel * 1.4f;
                subPhase = 0.0f;
            }
            // 8. SHAKER / PERC (Note 40 / Key 'K')
            else if (note == 40)
            {
                shakerEnv = vel * 1.15f;
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
        const float duckDecayCoef = std::exp(-1.0f / (0.160f * static_cast<float>(sampleRate))); // 160ms sidechain duck

        const float kBase = kickBaseFreq.load(std::memory_order_relaxed) * kPitchRatio;
        const float kSweep = kickSweepDepth.load(std::memory_order_relaxed) * kPitchRatio;
        const float kDriveFactor = 1.2f + 0.8f * kickDriveAmount.load(std::memory_order_relaxed);

        // Dynamic Acoustic Clap Coefficients
        const float sDecaySec = (snareDecayMs.load(std::memory_order_relaxed) * 0.001f) * sSpeedFactor;
        const float woodGain = snareWoodLevel.load(std::memory_order_relaxed);
        const float slapGain = snareSlapLevel.load(std::memory_order_relaxed);
        const float roomGain = snareRoomTail.load(std::memory_order_relaxed);

        const int tap2Sample = static_cast<int>(0.0028f * static_cast<float>(sampleRate));
        const int tap3Sample = static_cast<int>(0.0058f * static_cast<float>(sampleRate));

        const float microDecay = std::exp(-1.0f / ((0.009f * sSpeedFactor) * static_cast<float>(sampleRate)));
        const float mainDecay  = std::exp(-1.0f / (juce::jmax(0.01f, sDecaySec * 0.38f) * static_cast<float>(sampleRate)));
        const float bodyDecay  = std::exp(-1.0f / ((0.032f * sSpeedFactor) * static_cast<float>(sampleRate)));

        // Hat Decay Coefficients
        const float closedHatDecay = std::exp(-1.0f / (0.045f * static_cast<float>(sampleRate)));
        const float openHatDecay   = std::exp(-1.0f / (0.320f * static_cast<float>(sampleRate)));
        const float vinylDecay     = std::exp(-1.0f / (0.180f * static_cast<float>(sampleRate)));
        const float rimDecay       = std::exp(-1.0f / (0.028f * static_cast<float>(sampleRate)));
        const float subDecay       = std::exp(-1.0f / (0.460f * static_cast<float>(sampleRate)));
        const float shakerDecay    = std::exp(-1.0f / (0.075f * static_cast<float>(sampleRate)));

        // 6 Inharmonic Hat Oscillator Frequencies (TR-808 metallic cluster)
        constexpr std::array<float, 6> hatFreqs = { 245.0f, 306.0f, 384.0f, 523.0f, 659.0f, 831.0f };

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

            // Sidechain Ducking Decay
            kickDuckEnv *= duckDecayCoef;

            // 2. Acoustic Clap Voice
            if (clapSampleCounter < 999990)
            {
                if (clapSampleCounter == tap2Sample)
                    clapBurst2Env = currentVelocity * 1.25f;
                else if (clapSampleCounter == tap3Sample)
                    clapBurst3Env = currentVelocity * 1.6f;

                const float totalClapEnv = clapBurst1Env + clapBurst2Env + clapBurst3Env;
                if (totalClapEnv > 1.0e-4f || clapBodyEnv > 1.0e-4f)
                {
                    const float whiteNoise = nextRandomFloat() * 2.0f - 1.0f;
                    const float noiseBurst = whiteNoise * totalClapEnv;

                    const float woodFormant = clapWoodFilter.processSample(noiseBurst);
                    const float slapSmack   = clapSlapFilter.processSample(noiseBurst);
                    const float airFizz     = clapAirFilter.processSample(noiseBurst);

                    const float bodySine1 = std::sin(clapBodyPhase1);
                    const float bodySine2 = std::sin(clapBodyPhase2) * 0.5f;
                    const float physicalBody = (bodySine1 + bodySine2) * clapBodyEnv;

                    clapBodyPhase1 += twoPi * (240.0f * sPitchRatio) * samplePeriod;
                    if (clapBodyPhase1 >= twoPi) clapBodyPhase1 -= twoPi;

                    clapBodyPhase2 += twoPi * (580.0f * sPitchRatio) * samplePeriod;
                    if (clapBodyPhase2 >= twoPi) clapBodyPhase2 -= twoPi;

                    const float cavityResonance = clapBodyLowFilter.processSample(noiseBurst + physicalBody * 0.6f);

                    const float rawClapSum = physicalBody * (0.85f * woodGain)
                                           + cavityResonance * (1.1f * woodGain)
                                           + woodFormant * (1.3f * woodGain)
                                           + slapSmack * (1.5f * slapGain)
                                           + airFizz * (0.35f * roomGain);

                    const float filteredClap = clapToneLowPass.processSample(rawClapSum);
                    const float saturatedClap = std::tanh(filteredClap * 1.6f);

                    synthSample += saturatedClap * 1.25f;

                    clapBurst1Env *= microDecay;
                    clapBurst2Env *= microDecay;
                    clapBurst3Env *= mainDecay;
                    clapBodyEnv   *= bodyDecay;
                }

                ++clapSampleCounter;
            }

            // 3. Metallic Hi-Hat Oscillators (for Closed & Open Hats)
            if (closedHatEnv > 1.0e-4f || openHatEnv > 1.0e-4f)
            {
                float metallicSum = 0.0f;
                for (size_t osc = 0; osc < hatFreqs.size(); ++osc)
                {
                    hatPhases[osc] += twoPi * hatFreqs[osc] * samplePeriod;
                    if (hatPhases[osc] >= twoPi) hatPhases[osc] -= twoPi;
                    metallicSum += (hatPhases[osc] < juce::MathConstants<float>::pi ? 1.0f : -1.0f);
                }
                metallicSum *= (1.0f / 6.0f);

                const float hatNoise = (nextRandomFloat() * 2.0f - 1.0f) * 0.4f;
                const float combinedHat = metallicSum * 0.6f + hatNoise;
                const float hpFiltered = hatHighPass.processSample(combinedHat);
                const float bpFiltered = hatBandPass.processSample(hpFiltered);

                if (closedHatEnv > 1.0e-4f)
                {
                    synthSample += bpFiltered * closedHatEnv * 0.95f;
                    closedHatEnv *= closedHatDecay;
                }

                if (openHatEnv > 1.0e-4f)
                {
                    synthSample += bpFiltered * openHatEnv * 0.90f;
                    openHatEnv *= openHatDecay;
                }
            }

            // 5. Vinyl Dust / Texture
            if (vinylEnv > 1.0e-4f)
            {
                const float dustNoise = nextRandomFloat() * 2.0f - 1.0f;
                const bool cracklePop = (nextRandomFloat() > 0.985f);
                const float crackleImpulse = cracklePop ? (nextRandomFloat() * 1.8f - 0.9f) : 0.0f;
                synthSample += (dustNoise * 0.25f + crackleImpulse) * vinylEnv * 0.7f;
                vinylEnv *= vinylDecay;
            }

            // 6. Acoustic Rimshot
            if (rimEnv > 1.0e-4f)
            {
                const float rimImpulse = (nextRandomFloat() * 2.0f - 1.0f) * rimEnv;
                const float rim1 = rimBodyFilter.processSample(rimImpulse);
                const float rim2 = rimSnapFilter.processSample(rimImpulse);
                const float rimWoodSine = std::sin(rimPhase) * rimEnv * 0.8f;

                synthSample += std::tanh((rim1 * 1.5f + rim2 * 1.2f + rimWoodSine) * 1.4f) * 0.95f;

                rimPhase += twoPi * 480.0f * samplePeriod;
                if (rimPhase >= twoPi) rimPhase -= twoPi;

                rimEnv *= rimDecay;
            }

            // 7. 808 Sub Bass
            if (subEnv > 1.0e-4f)
            {
                const float subSine = std::sin(subPhase);
                const float subSat = std::tanh(subSine * 1.35f);
                synthSample += subSat * subEnv * 1.2f;

                subPhase += twoPi * 42.0f * samplePeriod;
                if (subPhase >= twoPi) subPhase -= twoPi;

                subEnv *= subDecay;
            }

            // 8. Shaker Perc
            if (shakerEnv > 1.0e-4f)
            {
                const float shakerNoise = nextRandomFloat() * 2.0f - 1.0f;
                const float filteredShaker = shakerFilter.processSample(shakerNoise) * shakerEnv;
                synthSample += filteredShaker * 0.85f;
                shakerEnv *= shakerDecay;
            }

            // Final Sum & Output to Buffer
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
        clapBodyLowFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 680.0f, 2.5f);
        clapBodyLowFilter.reset();

        clapWoodFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 1050.0f, 3.0f);
        clapWoodFilter.reset();

        clapSlapFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 2600.0f, 2.8f);
        clapSlapFilter.reset();

        clapAirFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 5800.0f, 1.6f);
        clapAirFilter.reset();

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
    std::atomic<float> kickPitchSemi { 0.0f };
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
    float kickDuckEnv { 0.0f };

    juce::dsp::IIR::Filter<float> kickBeaterFilter;
    juce::dsp::IIR::Filter<float> kickBodyFilter;

    // Single Solid Acoustic Clap State & Parameters
    std::atomic<float> snarePitchSemi { 0.0f };
    std::atomic<float> snareDecayMs { 180.0f };
    std::atomic<float> snareWoodLevel { 0.85f };
    std::atomic<float> snareSlapLevel { 0.90f };
    std::atomic<float> snareRoomTail { 0.40f };
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

    // Hi-Hat State & Filters
    float closedHatEnv { 0.0f };
    float openHatEnv { 0.0f };
    std::array<float, 6> hatPhases {};
    juce::dsp::IIR::Filter<float> hatHighPass;
    juce::dsp::IIR::Filter<float> hatBandPass;

    // Vinyl Dust
    float vinylEnv { 0.0f };

    // Rimshot State & Filters
    float rimEnv { 0.0f };
    float rimPhase { 0.0f };
    juce::dsp::IIR::Filter<float> rimBodyFilter;
    juce::dsp::IIR::Filter<float> rimSnapFilter;

    // 808 Sub Bass
    float subEnv { 0.0f };
    float subPhase { 0.0f };

    // Shaker Perc
    float shakerEnv { 0.0f };
    juce::dsp::IIR::Filter<float> shakerFilter;
};

} // namespace underground::dsp
