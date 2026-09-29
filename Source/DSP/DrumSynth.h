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
 *        - Sound 1: KICK (Deep sub, pitch sweep, click punch, overdrive)
 *        - Sound 2: SNARE (Acoustic wire buzz, resonant tone body, rim crack, drive)
 *        - Sound 3: CLAP (Acoustic wood & palm physical formants, noise filter, room tail)
 *        - Sound 4: CLOSED HI-HAT (6 inharmonic metallic square cluster, highpass 7kHz)
 *        - Sound 5: OPEN HI-HAT (Metallic shimmer wash, choke group with closed hat)
 *        - Sound 6: RIMSHOT (Resonant hollow wood shell tap, snappy beater crack)
 *        - Sound 7: SUB 808 (Deep 42Hz saturated sine with pitch glide drop)
 *        - Sound 8: SHAKER / VINYL (Soft attack rattle & vinyl crackle pop bursts)
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

        // Snare Filters
        snareBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 195.0f, 3.0f);
        snareBodyFilter.reset();
        snareWiresFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 2800.0f, 0.707f);
        snareWiresFilter.reset();
        snareCrackFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 4500.0f, 1.8f);
        snareCrackFilter.reset();

        // Clap Filters
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

        // Sub 808 Lowpass Filter
        subLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 320.0f, 0.707f);
        subLowPass.reset();

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
        kickDuckEnv = 0.0f;

        // Snare
        snareBodyEnv = 0.0f;
        snareWiresEnv = 0.0f;
        snarePhase = 0.0f;

        // Clap
        clapSampleCounter = 999999;
        clapBurst1Env = 0.0f;
        clapBurst2Env = 0.0f;
        clapBurst3Env = 0.0f;
        clapBodyPhase1 = 0.0f;
        clapBodyPhase2 = 0.0f;
        clapBodyEnv = 0.0f;
        clapVelocity = 0.0f;

        // Hats
        closedHatEnv = 0.0f;
        openHatEnv = 0.0f;
        hatPhases.fill(0.0f);

        // Rimshot
        rimEnv = 0.0f;
        rimPhase = 0.0f;

        // 808 Sub
        subEnv = 0.0f;
        subPhase = 0.0f;
        subPitchEnv = 0.0f;

        // Shaker / Vinyl
        shakerEnv = 0.0f;
        shakerAttacking = false;

        kickBeaterFilter.reset();
        kickBodyFilter.reset();
        snareBodyFilter.reset();
        snareWiresFilter.reset();
        snareCrackFilter.reset();
        clapBodyLowFilter.reset();
        clapWoodFilter.reset();
        clapSlapFilter.reset();
        clapAirFilter.reset();
        clapToneLowPass.reset();
        hatHighPass.reset();
        hatBandPass.reset();
        rimBodyFilter.reset();
        rimSnapFilter.reset();
        subLowPass.reset();
        shakerFilter.reset();

        rngState = 0x98765432;
    }

    // --- 1. Kick Setters ---
    void setKickPitchSemi(float semi) noexcept { kickPitchSemi.store(semi, std::memory_order_relaxed); }
    void setKickTune(float hz) noexcept { kickBaseFreq.store(hz, std::memory_order_relaxed); }
    void setKickPitchSweep(float hz) noexcept { kickSweepDepth.store(hz, std::memory_order_relaxed); }
    void setKickDecay(float ms) noexcept { kickDecayMs.store(ms, std::memory_order_relaxed); }
    void setKickPunch(float norm) noexcept { kickPunchLevel.store(norm, std::memory_order_relaxed); }
    void setKickDrive(float driveNorm) noexcept { kickDriveAmount.store(driveNorm, std::memory_order_relaxed); }

    // --- 2. Snare Setters ---
    void setSnarePitchSemi(float semi) noexcept { snarePitchSemi.store(semi, std::memory_order_relaxed); }
    void setSnareDecay(float ms) noexcept { snareDecayMs.store(ms, std::memory_order_relaxed); }
    void setSnareSnap(float norm) noexcept { snareSnapLevel.store(norm, std::memory_order_relaxed); }
    void setSnareTone(float hz) noexcept
    {
        snareToneFreq.store(hz, std::memory_order_relaxed);
        snareBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, juce::jlimit(80.0f, 600.0f, hz), 3.0f);
    }
    void setSnareCrack(float norm) noexcept { snareCrackLevel.store(norm, std::memory_order_relaxed); }
    void setSnareDrive(float norm) noexcept { snareDriveAmount.store(norm, std::memory_order_relaxed); }

    // --- 3. Acoustic Clap Setters ---
    void setClapPitchSemi(float semi) noexcept { clapPitchSemi.store(semi, std::memory_order_relaxed); }
    void setClapDecay(float ms) noexcept { clapDecayMs.store(ms, std::memory_order_relaxed); }
    void setClapWood(float norm) noexcept { clapWoodLevel.store(norm, std::memory_order_relaxed); }
    void setClapSlap(float norm) noexcept { clapSlapLevel.store(norm, std::memory_order_relaxed); }
    void setClapTone(float hz) noexcept
    {
        if (std::abs(lastToneHz - hz) > 20.0f)
        {
            lastToneHz = hz;
            clapToneLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, juce::jlimit(800.0f, 18000.0f, hz), 0.707f);
        }
    }
    void setClapTail(float norm) noexcept { clapRoomTail.store(norm, std::memory_order_relaxed); }

    // --- 4. Closed Hat Setters ---
    void setChatPitchSemi(float semi) noexcept { chatPitchSemi.store(semi, std::memory_order_relaxed); }
    void setChatDecay(float ms) noexcept { chatDecayMs.store(ms, std::memory_order_relaxed); }
    void setChatTone(float hz) noexcept
    {
        chatToneCutoff.store(hz, std::memory_order_relaxed);
        hatBandPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, juce::jlimit(3000.0f, 15000.0f, hz), 2.0f);
    }
    void setChatSizzle(float norm) noexcept { chatSizzleLevel.store(norm, std::memory_order_relaxed); }
    void setChatRes(float q) noexcept { chatResAmount.store(q, std::memory_order_relaxed); }
    void setChatDrive(float norm) noexcept { chatDriveAmount.store(norm, std::memory_order_relaxed); }

    // --- 5. Open Hat Setters ---
    void setOhatPitchSemi(float semi) noexcept { ohatPitchSemi.store(semi, std::memory_order_relaxed); }
    void setOhatDecay(float ms) noexcept { ohatDecayMs.store(ms, std::memory_order_relaxed); }
    void setOhatTone(float hz) noexcept { ohatToneCutoff.store(hz, std::memory_order_relaxed); }
    void setOhatSizzle(float norm) noexcept { ohatSizzleLevel.store(norm, std::memory_order_relaxed); }
    void setOhatChoke(float norm) noexcept { ohatChokeAmount.store(norm, std::memory_order_relaxed); }
    void setOhatDrive(float norm) noexcept { ohatDriveAmount.store(norm, std::memory_order_relaxed); }

    // --- 6. Rimshot Setters ---
    void setRimPitchSemi(float semi) noexcept { rimPitchSemi.store(semi, std::memory_order_relaxed); }
    void setRimDecay(float ms) noexcept { rimDecayMs.store(ms, std::memory_order_relaxed); }
    void setRimTune(float hz) noexcept
    {
        rimTuneFreq.store(hz, std::memory_order_relaxed);
        rimBodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, juce::jlimit(200.0f, 1200.0f, hz), 4.0f);
    }
    void setRimSnap(float norm) noexcept { rimSnapLevel.store(norm, std::memory_order_relaxed); }
    void setRimTone(float hz) noexcept { rimToneCutoff.store(hz, std::memory_order_relaxed); }
    void setRimDrive(float norm) noexcept { rimDriveAmount.store(norm, std::memory_order_relaxed); }

    // --- 7. Sub 808 Setters ---
    void setSubTune(float hz) noexcept { subTuneFreq.store(hz, std::memory_order_relaxed); }
    void setSubDecay(float ms) noexcept { subDecayMs.store(ms, std::memory_order_relaxed); }
    void setSubSweep(float hz) noexcept { subSweepDepth.store(hz, std::memory_order_relaxed); }
    void setSubDrive(float norm) noexcept { subDriveAmount.store(norm, std::memory_order_relaxed); }
    void setSubCutoff(float hz) noexcept
    {
        subCutoffFreq.store(hz, std::memory_order_relaxed);
        subLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, juce::jlimit(60.0f, 1200.0f, hz), 0.707f);
    }
    void setSubLevel(float db) noexcept { subLevelGain.store(juce::Decibels::decibelsToGain(db), std::memory_order_relaxed); }

    // --- 8. Shaker / Vinyl Setters ---
    void setShakerAttack(float ms) noexcept { shakerAttackMs.store(ms, std::memory_order_relaxed); }
    void setShakerDecay(float ms) noexcept { shakerDecayMs.store(ms, std::memory_order_relaxed); }
    void setShakerTone(float hz) noexcept
    {
        shakerToneFreq.store(hz, std::memory_order_relaxed);
        shakerFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, juce::jlimit(1500.0f, 12000.0f, hz), 1.8f);
    }
    void setVinylCrackle(float norm) noexcept { vinylCrackleLevel.store(norm, std::memory_order_relaxed); }
    void setVinylHiss(float norm) noexcept { vinylHissLevel.store(norm, std::memory_order_relaxed); }
    void setShakerDrive(float norm) noexcept { shakerDriveAmount.store(norm, std::memory_order_relaxed); }

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
                kickDuckEnv = 1.0f;
            }
            // 2. SNARE (Note 38 / Key 'S')
            else if (note == 38)
            {
                snareBodyEnv = vel * 1.35f;
                snareWiresEnv = vel * 1.25f;
                snarePhase = 0.0f;
            }
            // 3. CLAP (Note 39 / Key 'D')
            else if (note == 39)
            {
                clapVelocity = vel;
                clapSampleCounter = 0;
                clapBurst1Env = vel * 1.4f;
                clapBurst2Env = 0.0f;
                clapBurst3Env = 0.0f;
                clapBodyEnv = vel * 1.25f;
                clapBodyPhase1 = 0.0f;
                clapBodyPhase2 = 0.0f;
            }
            // 4. CLOSED HI-HAT (Note 42 / Key 'F')
            else if (note == 42)
            {
                closedHatEnv = vel * 1.2f;
                const float choke = ohatChokeAmount.load(std::memory_order_relaxed);
                openHatEnv *= (1.0f - choke);
            }
            // 5. OPEN HI-HAT (Note 46 / Key 'G')
            else if (note == 46)
            {
                openHatEnv = vel * 1.35f;
            }
            // 6. RIMSHOT (Note 37 / Key 'H')
            else if (note == 37)
            {
                rimEnv = vel * 1.3f;
                rimPhase = 0.0f;
            }
            // 7. SUB 808 (Note 48 / Key 'J')
            else if (note == 48)
            {
                subEnv = vel * 1.4f;
                subPitchEnv = 1.0f;
                subPhase = 0.0f;
            }
            // 8. SHAKER / VINYL (Note 40 / Key 'K')
            else if (note == 40)
            {
                shakerEnv = 0.05f;
                shakerAttacking = true;
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

        // Varispeed Sampler Pitch Ratios
        const float kPitchRatio = std::pow(2.0f, kickPitchSemi.load(std::memory_order_relaxed) / 12.0f);
        const float kSpeedFactor = 1.0f / juce::jmax(0.2f, kPitchRatio);

        const float snPitchRatio = std::pow(2.0f, snarePitchSemi.load(std::memory_order_relaxed) / 12.0f);
        const float clPitchRatio = std::pow(2.0f, clapPitchSemi.load(std::memory_order_relaxed) / 12.0f);

        // Kick Coefficients
        const float kDecaySec = (kickDecayMs.load(std::memory_order_relaxed) * 0.001f) * 0.45f * kSpeedFactor;
        const float kickDecayCoef = std::exp(-1.0f / (juce::jmax(0.005f, kDecaySec) * static_cast<float>(sampleRate)));
        const float kickPitchDecayCoef = std::exp(-1.0f / ((0.016f * kSpeedFactor) * static_cast<float>(sampleRate)));
        const float kickBeaterDecayCoef = std::exp(-1.0f / ((0.006f * kSpeedFactor) * static_cast<float>(sampleRate)));
        const float duckDecayCoef = std::exp(-1.0f / (0.160f * static_cast<float>(sampleRate)));

        const float kBase = kickBaseFreq.load(std::memory_order_relaxed) * kPitchRatio;
        const float kSweep = kickSweepDepth.load(std::memory_order_relaxed) * kPitchRatio;
        const float kDrive = 1.2f + 0.8f * kickDriveAmount.load(std::memory_order_relaxed);

        // Snare Coefficients
        const float snDecaySec = (snareDecayMs.load(std::memory_order_relaxed) * 0.001f);
        const float snareBodyDecay = std::exp(-1.0f / (juce::jmax(0.01f, snDecaySec * 0.45f) * static_cast<float>(sampleRate)));
        const float snareWiresDecay = std::exp(-1.0f / (juce::jmax(0.01f, snDecaySec * 0.85f) * static_cast<float>(sampleRate)));
        const float snareSnap = snareSnapLevel.load(std::memory_order_relaxed);
        const float snareCrack = snareCrackLevel.load(std::memory_order_relaxed);
        const float snareDrive = 1.0f + 1.2f * snareDriveAmount.load(std::memory_order_relaxed);
        const float snareFreq = snareToneFreq.load(std::memory_order_relaxed) * snPitchRatio;

        // Clap Coefficients
        const float clDecaySec = (clapDecayMs.load(std::memory_order_relaxed) * 0.001f);
        const float woodGain = clapWoodLevel.load(std::memory_order_relaxed);
        const float slapGain = clapSlapLevel.load(std::memory_order_relaxed);
        const float roomGain = clapRoomTail.load(std::memory_order_relaxed);

        const int tap2Sample = static_cast<int>(0.0028f * static_cast<float>(sampleRate));
        const int tap3Sample = static_cast<int>(0.0058f * static_cast<float>(sampleRate));
        const float clMicroDecay = std::exp(-1.0f / (0.009f * static_cast<float>(sampleRate)));
        const float clMainDecay  = std::exp(-1.0f / (juce::jmax(0.01f, clDecaySec * 0.38f) * static_cast<float>(sampleRate)));
        const float clBodyDecay  = std::exp(-1.0f / (0.032f * static_cast<float>(sampleRate)));

        // Hats Decay Coefficients
        const float chatDecay = std::exp(-1.0f / ((chatDecayMs.load(std::memory_order_relaxed) * 0.001f) * static_cast<float>(sampleRate)));
        const float ohatDecay = std::exp(-1.0f / ((ohatDecayMs.load(std::memory_order_relaxed) * 0.001f) * static_cast<float>(sampleRate)));
        const float rimDecay  = std::exp(-1.0f / ((rimDecayMs.load(std::memory_order_relaxed) * 0.001f) * static_cast<float>(sampleRate)));
        const float subDecay  = std::exp(-1.0f / ((subDecayMs.load(std::memory_order_relaxed) * 0.001f) * static_cast<float>(sampleRate)));
        const float subGlide  = std::exp(-1.0f / (0.045f * static_cast<float>(sampleRate)));

        // Shaker Attack & Decay
        const float shAttackStep = 1.0f / (juce::jmax(0.001f, shakerAttackMs.load(std::memory_order_relaxed) * 0.001f) * static_cast<float>(sampleRate));
        const float shDecayCoef = std::exp(-1.0f / ((shakerDecayMs.load(std::memory_order_relaxed) * 0.001f) * static_cast<float>(sampleRate)));

        constexpr std::array<float, 6> hatFreqs = { 245.0f, 306.0f, 384.0f, 523.0f, 659.0f, 831.0f };

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            while (midiIterator != midiMessages.cend() && (*midiIterator).samplePosition == sampleIdx)
            {
                handleMidiEvent((*midiIterator).getMessage());
                ++midiIterator;
            }

            float synthSample = 0.0f;

            // 1. KICK VOICE
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
                    beaterClick = kickBeaterFilter.processSample(noise) * kickBeaterEnv * 0.75f;
                    kickBeaterEnv *= kickBeaterDecayCoef;
                }

                synthSample += filteredBody * 1.35f + beaterClick;

                kickPhase += twoPi * kickFreq * samplePeriod;
                if (kickPhase >= twoPi) kickPhase -= twoPi;

                kickHarmonicPhase += twoPi * (kickFreq * 2.0f) * samplePeriod;
                if (kickHarmonicPhase >= twoPi) kickHarmonicPhase -= twoPi;

                kickEnv *= kickDecayCoef;
                kickPitchEnv *= kickPitchDecayCoef;
            }

            kickDuckEnv *= duckDecayCoef;

            // 2. SNARE VOICE (Punchy Acoustic & Underground Wire Snare)
            if (snareBodyEnv > 1.0e-4f || snareWiresEnv > 1.0e-4f)
            {
                const float snareNoise = nextRandomFloat() * 2.0f - 1.0f;
                const float wireSound = snareWiresFilter.processSample(snareNoise) * snareWiresEnv * snareSnap;
                const float crackSound = snareCrackFilter.processSample(snareNoise) * (snareWiresEnv * 0.8f) * snareCrack;

                const float bodyOsc = std::sin(snarePhase) * snareBodyEnv;
                const float bodyFiltered = snareBodyFilter.processSample(bodyOsc + snareNoise * 0.2f);

                const float rawSnare = (bodyFiltered * 1.1f + wireSound * 1.4f + crackSound * 1.2f) * snareDrive;
                synthSample += std::tanh(rawSnare) * 1.2f;

                snarePhase += twoPi * snareFreq * samplePeriod;
                if (snarePhase >= twoPi) snarePhase -= twoPi;

                snareBodyEnv *= snareBodyDecay;
                snareWiresEnv *= snareWiresDecay;
            }

            // 3. ACOUSTIC CLAP VOICE
            if (clapSampleCounter < 999990)
            {
                if (clapSampleCounter == tap2Sample)
                    clapBurst2Env = clapVelocity * 1.25f;
                else if (clapSampleCounter == tap3Sample)
                    clapBurst3Env = clapVelocity * 1.6f;

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

                    clapBodyPhase1 += twoPi * (240.0f * clPitchRatio) * samplePeriod;
                    if (clapBodyPhase1 >= twoPi) clapBodyPhase1 -= twoPi;

                    clapBodyPhase2 += twoPi * (580.0f * clPitchRatio) * samplePeriod;
                    if (clapBodyPhase2 >= twoPi) clapBodyPhase2 -= twoPi;

                    const float cavityResonance = clapBodyLowFilter.processSample(noiseBurst + physicalBody * 0.6f);

                    const float rawClapSum = physicalBody * (0.85f * woodGain)
                                           + cavityResonance * (1.1f * woodGain)
                                           + woodFormant * (1.3f * woodGain)
                                           + slapSmack * (1.5f * slapGain)
                                           + airFizz * (0.35f * roomGain);

                    const float filteredClap = clapToneLowPass.processSample(rawClapSum);
                    synthSample += std::tanh(filteredClap * 1.6f) * 1.25f;

                    clapBurst1Env *= clMicroDecay;
                    clapBurst2Env *= clMicroDecay;
                    clapBurst3Env *= clMainDecay;
                    clapBodyEnv   *= clBodyDecay;
                }

                ++clapSampleCounter;
            }

            // 4. CLOSED & OPEN HI-HATS
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
                    const float sizzle = chatSizzleLevel.load(std::memory_order_relaxed);
                    const float drive = 1.0f + 0.8f * chatDriveAmount.load(std::memory_order_relaxed);
                    synthSample += std::tanh((bpFiltered + hatNoise * sizzle * 0.3f) * drive) * closedHatEnv * 0.95f;
                    closedHatEnv *= chatDecay;
                }

                if (openHatEnv > 1.0e-4f)
                {
                    const float sizzle = ohatSizzleLevel.load(std::memory_order_relaxed);
                    const float drive = 1.0f + 0.8f * ohatDriveAmount.load(std::memory_order_relaxed);
                    synthSample += std::tanh((bpFiltered + hatNoise * sizzle * 0.4f) * drive) * openHatEnv * 0.90f;
                    openHatEnv *= ohatDecay;
                }
            }

            // 6. RIMSHOT
            if (rimEnv > 1.0e-4f)
            {
                const float rimImpulse = (nextRandomFloat() * 2.0f - 1.0f) * rimEnv;
                const float rim1 = rimBodyFilter.processSample(rimImpulse);
                const float rim2 = rimSnapFilter.processSample(rimImpulse) * rimSnapLevel.load(std::memory_order_relaxed);
                const float rimWoodSine = std::sin(rimPhase) * rimEnv * 0.8f;
                const float rDrive = 1.0f + 0.9f * rimDriveAmount.load(std::memory_order_relaxed);

                synthSample += std::tanh((rim1 * 1.5f + rim2 * 1.2f + rimWoodSine) * rDrive) * 0.95f;

                rimPhase += twoPi * rimTuneFreq.load(std::memory_order_relaxed) * samplePeriod;
                if (rimPhase >= twoPi) rimPhase -= twoPi;

                rimEnv *= rimDecay;
            }

            // 7. SUB 808 BASS
            if (subEnv > 1.0e-4f)
            {
                const float sBase = subTuneFreq.load(std::memory_order_relaxed);
                const float sSweep = subSweepDepth.load(std::memory_order_relaxed);
                const float sFreq = sBase + sSweep * subPitchEnv;
                const float sDrive = 1.0f + 1.5f * subDriveAmount.load(std::memory_order_relaxed);

                const float subSine = std::sin(subPhase);
                const float subFiltered = subLowPass.processSample(subSine * sDrive);
                const float subSat = std::tanh(subFiltered);

                synthSample += subSat * subEnv * subLevelGain.load(std::memory_order_relaxed);

                subPhase += twoPi * sFreq * samplePeriod;
                if (subPhase >= twoPi) subPhase -= twoPi;

                subEnv *= subDecay;
                subPitchEnv *= subGlide;
            }

            // 8. SHAKER & VINYL CRACKLE
            if (shakerEnv > 1.0e-4f)
            {
                if (shakerAttacking)
                {
                    shakerEnv += shAttackStep;
                    if (shakerEnv >= 1.0f)
                    {
                        shakerEnv = 1.0f;
                        shakerAttacking = false;
                    }
                }
                else
                {
                    shakerEnv *= shDecayCoef;
                }

                const float shakerNoise = nextRandomFloat() * 2.0f - 1.0f;
                const float filteredShaker = shakerFilter.processSample(shakerNoise) * shakerEnv;

                // Vinyl Crackle Burst
                const float crackleDens = vinylCrackleLevel.load(std::memory_order_relaxed);
                const bool cracklePop = (nextRandomFloat() > (1.0f - crackleDens * 0.04f));
                const float crackleImpulse = cracklePop ? (nextRandomFloat() * 1.8f - 0.9f) : 0.0f;
                const float hiss = (nextRandomFloat() * 2.0f - 1.0f) * vinylHissLevel.load(std::memory_order_relaxed) * 0.15f;

                synthSample += (filteredShaker * 0.85f + crackleImpulse + hiss);
            }

            // Output to Audio Buffer
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

    // 1. Kick State & Parameters
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

    // 2. Snare State & Parameters
    std::atomic<float> snarePitchSemi { 0.0f };
    std::atomic<float> snareDecayMs { 180.0f };
    std::atomic<float> snareSnapLevel { 0.85f };
    std::atomic<float> snareToneFreq { 195.0f };
    std::atomic<float> snareCrackLevel { 0.80f };
    std::atomic<float> snareDriveAmount { 0.40f };

    float snareBodyEnv { 0.0f };
    float snareWiresEnv { 0.0f };
    float snarePhase { 0.0f };

    juce::dsp::IIR::Filter<float> snareBodyFilter;
    juce::dsp::IIR::Filter<float> snareWiresFilter;
    juce::dsp::IIR::Filter<float> snareCrackFilter;

    // 3. Acoustic Clap State & Parameters
    std::atomic<float> clapPitchSemi { 0.0f };
    std::atomic<float> clapDecayMs { 180.0f };
    std::atomic<float> clapWoodLevel { 0.85f };
    std::atomic<float> clapSlapLevel { 0.90f };
    std::atomic<float> clapRoomTail { 0.40f };
    float lastToneHz { 7500.0f };

    int clapSampleCounter { 999999 };
    float clapVelocity { 1.0f };
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

    // 4. Closed Hat State & Parameters
    std::atomic<float> chatPitchSemi { 0.0f };
    std::atomic<float> chatDecayMs { 45.0f };
    std::atomic<float> chatToneCutoff { 9500.0f };
    std::atomic<float> chatSizzleLevel { 0.60f };
    std::atomic<float> chatResAmount { 2.0f };
    std::atomic<float> chatDriveAmount { 0.30f };

    float closedHatEnv { 0.0f };

    // 5. Open Hat State & Parameters
    std::atomic<float> ohatPitchSemi { 0.0f };
    std::atomic<float> ohatDecayMs { 320.0f };
    std::atomic<float> ohatToneCutoff { 8500.0f };
    std::atomic<float> ohatSizzleLevel { 0.70f };
    std::atomic<float> ohatChokeAmount { 1.0f };
    std::atomic<float> ohatDriveAmount { 0.35f };

    float openHatEnv { 0.0f };
    std::array<float, 6> hatPhases {};
    juce::dsp::IIR::Filter<float> hatHighPass;
    juce::dsp::IIR::Filter<float> hatBandPass;

    // 6. Rimshot State & Parameters
    std::atomic<float> rimPitchSemi { 0.0f };
    std::atomic<float> rimDecayMs { 28.0f };
    std::atomic<float> rimTuneFreq { 480.0f };
    std::atomic<float> rimSnapLevel { 0.85f };
    std::atomic<float> rimToneCutoff { 6000.0f };
    std::atomic<float> rimDriveAmount { 0.40f };

    float rimEnv { 0.0f };
    float rimPhase { 0.0f };
    juce::dsp::IIR::Filter<float> rimBodyFilter;
    juce::dsp::IIR::Filter<float> rimSnapFilter;

    // 7. Sub 808 State & Parameters
    std::atomic<float> subTuneFreq { 42.0f };
    std::atomic<float> subDecayMs { 500.0f };
    std::atomic<float> subSweepDepth { 40.0f };
    std::atomic<float> subDriveAmount { 0.45f };
    std::atomic<float> subCutoffFreq { 320.0f };
    std::atomic<float> subLevelGain { 1.2f };

    float subEnv { 0.0f };
    float subPhase { 0.0f };
    float subPitchEnv { 0.0f };
    juce::dsp::IIR::Filter<float> subLowPass;

    // 8. Shaker / Vinyl State & Parameters
    std::atomic<float> shakerAttackMs { 12.0f };
    std::atomic<float> shakerDecayMs { 75.0f };
    std::atomic<float> shakerToneFreq { 6200.0f };
    std::atomic<float> vinylCrackleLevel { 0.30f };
    std::atomic<float> vinylHissLevel { 0.20f };
    std::atomic<float> shakerDriveAmount { 0.25f };

    float shakerEnv { 0.0f };
    bool shakerAttacking { false };
    juce::dsp::IIR::Filter<float> shakerFilter;
};

} // namespace underground::dsp
