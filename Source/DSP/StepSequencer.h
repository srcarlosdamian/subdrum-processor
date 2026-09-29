#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <algorithm>

namespace underground::dsp
{

/**
 * @brief 4-Track 16-Step Pattern Sequencer with 16th-Note Swing,
 *        DAW Host Transport Sync, and authentic Underground rhythm presets.
 *        - Track 0: Kick (Note 36)
 *        - Track 1: Clap (Note 38)
 *        - Track 2: Closed Hat (Note 42)
 *        - Track 3: Open Hat / Perc (Note 46)
 */
class StepSequencer
{
public:
    static constexpr int numSteps = 16;
    static constexpr int numTracks = 4;

    enum class RhythmPreset
    {
        TwoStepClassic = 0,
        SyncopatedGarage,
        HalfStepDub,
        BrokenBeat,
        StraightFour,
        GhostClap,
        ClearAll
    };

    StepSequencer()
    {
        loadRhythmPreset(RhythmPreset::TwoStepClassic);
    }

    void prepare(double sampleRateHz)
    {
        sampleRate = sampleRateHz > 0.0 ? sampleRateHz : 44100.0;
        updateStepTiming();
        sampleCounter = 0;
    }

    void reset()
    {
        sampleCounter = 0;
        currentStep.store(0, std::memory_order_relaxed);
    }

    void setPlaying(bool play)
    {
        playing.store(play, std::memory_order_release);
        if (!play)
        {
            sampleCounter = 0;
            currentStep.store(0, std::memory_order_relaxed);
        }
    }

    bool isPlaying() const noexcept
    {
        return playing.load(std::memory_order_acquire);
    }

    void setBpm(double newBpm)
    {
        bpm = std::clamp(newBpm, 40.0, 240.0);
        updateStepTiming();
    }

    double getBpm() const noexcept
    {
        return bpm;
    }

    void setSwing(float swingPercent) noexcept
    {
        // swingPercent: 50.0% (straight) to 75.0% (heavy shuffle)
        swingRatio.store(std::clamp(swingPercent * 0.01f, 0.50f, 0.75f), std::memory_order_relaxed);
        updateStepTiming();
    }

    float getSwing() const noexcept
    {
        return swingRatio.load(std::memory_order_relaxed) * 100.0f;
    }

    void setHostSync(bool enabled) noexcept
    {
        hostSync.store(enabled, std::memory_order_relaxed);
    }

    bool isHostSyncEnabled() const noexcept
    {
        return hostSync.load(std::memory_order_relaxed);
    }

    void setStep(int track, int step, bool active)
    {
        if (track >= 0 && track < numTracks && step >= 0 && step < numSteps)
        {
            pattern[track][step].store(active, std::memory_order_release);
        }
    }

    bool getStep(int track, int step) const noexcept
    {
        if (track >= 0 && track < numTracks && step >= 0 && step < numSteps)
        {
            return pattern[track][step].load(std::memory_order_acquire);
        }
        return false;
    }

    void toggleStep(int track, int step)
    {
        if (track >= 0 && track < numTracks && step >= 0 && step < numSteps)
        {
            const bool current = pattern[track][step].load(std::memory_order_relaxed);
            pattern[track][step].store(!current, std::memory_order_release);
        }
    }

    void clear()
    {
        for (int t = 0; t < numTracks; ++t)
            for (int s = 0; s < numSteps; ++s)
                pattern[t][s].store(false, std::memory_order_release);
    }

    void loadRhythmPreset(RhythmPreset preset)
    {
        clear();
        switch (preset)
        {
            case RhythmPreset::TwoStepClassic:
                // Kick: 1 (0), 2.3 (6), 3.3 (10)
                pattern[0][0].store(true); pattern[0][6].store(true); pattern[0][10].store(true);
                // Clap: Beat 2 (4), Beat 4 (12)
                pattern[1][4].store(true); pattern[1][12].store(true);
                // Closed Hat: shuffling 16ths
                pattern[2][2].store(true); pattern[2][4].store(true); pattern[2][6].store(true);
                pattern[2][8].store(true); pattern[2][10].store(true); pattern[2][12].store(true); pattern[2][14].store(true);
                // Open Hat / Perc: syncopated off-beats
                pattern[3][7].store(true); pattern[3][15].store(true);
                break;

            case RhythmPreset::SyncopatedGarage:
                pattern[0][0].store(true); pattern[0][7].store(true); pattern[0][10].store(true);
                pattern[1][4].store(true); pattern[1][12].store(true); pattern[1][15].store(true);
                for (int s : { 0, 2, 4, 6, 8, 10, 11, 12, 14 }) pattern[2][s].store(true);
                pattern[3][3].store(true); pattern[3][9].store(true);
                break;

            case RhythmPreset::HalfStepDub:
                pattern[0][0].store(true); pattern[0][10].store(true);
                pattern[1][8].store(true); // Half-step snare on Beat 3 (step 8)
                for (int s = 0; s < 16; s += 2) pattern[2][s].store(true);
                pattern[3][6].store(true); pattern[3][14].store(true);
                break;

            case RhythmPreset::BrokenBeat:
                pattern[0][0].store(true); pattern[0][3].store(true); pattern[0][8].store(true); pattern[0][11].store(true);
                pattern[1][4].store(true); pattern[1][12].store(true); pattern[1][14].store(true);
                for (int s : { 2, 5, 8, 10, 13 }) pattern[2][s].store(true);
                pattern[3][7].store(true); pattern[3][15].store(true);
                break;

            case RhythmPreset::StraightFour:
                for (int s = 0; s < 16; s += 4) pattern[0][s].store(true); // 4-on-floor kick
                pattern[1][4].store(true); pattern[1][12].store(true);
                for (int s = 2; s < 16; s += 4) pattern[2][s].store(true); // off-beat hats
                pattern[3][6].store(true); pattern[3][14].store(true);
                break;

            case RhythmPreset::GhostClap:
                pattern[0][0].store(true); pattern[0][8].store(true);
                pattern[1][4].store(true); pattern[1][11].store(true); pattern[1][12].store(true);
                for (int s = 0; s < 16; s += 2) pattern[2][s].store(true);
                pattern[3][1].store(true); pattern[3][9].store(true);
                break;

            case RhythmPreset::ClearAll:
                break;
        }
    }

    void loadDefaultPattern()
    {
        loadRhythmPreset(RhythmPreset::TwoStepClassic);
    }

    int getCurrentStep() const noexcept
    {
        return currentStep.load(std::memory_order_relaxed);
    }

    /**
     * @brief Process sequencer clock in audio block and inject MIDI note-ons.
     */
    void process(juce::MidiBuffer& midiMessages, int numSamples, double hostBpm = 0.0,
                 bool hostIsPlaying = false, double hostPpqPosition = -1.0)
    {
        if (hostBpm > 20.0)
            setBpm(hostBpm);

        const bool sync = hostSync.load(std::memory_order_relaxed);
        if (sync)
        {
            if (hostIsPlaying && !playing.load(std::memory_order_relaxed))
                setPlaying(true);
            else if (!hostIsPlaying && playing.load(std::memory_order_relaxed))
                setPlaying(false);
        }

        if (!playing.load(std::memory_order_relaxed))
            return;

        const float sw = swingRatio.load(std::memory_order_relaxed);
        const double baseStepSamples = (60.0 / (bpm * 4.0)) * sampleRate;

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            if (sampleCounter <= 0)
            {
                const int step = currentStep.load(std::memory_order_relaxed);

                // Track 0: KICK (Note 36)
                if (pattern[0][step].load(std::memory_order_relaxed))
                    midiMessages.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), sampleIdx);

                // Track 1: CLAP (Note 38)
                if (pattern[1][step].load(std::memory_order_relaxed))
                    midiMessages.addEvent(juce::MidiMessage::noteOn(1, 38, 1.0f), sampleIdx);

                // Track 2: CLOSED HAT (Note 42)
                if (pattern[2][step].load(std::memory_order_relaxed))
                    midiMessages.addEvent(juce::MidiMessage::noteOn(1, 42, 0.85f), sampleIdx);

                // Track 3: OPEN HAT / PERC (Note 46)
                if (pattern[3][step].load(std::memory_order_relaxed))
                    midiMessages.addEvent(juce::MidiMessage::noteOn(1, 46, 0.90f), sampleIdx);

                // Advance step
                const int nextStep = (step + 1) % numSteps;
                currentStep.store(nextStep, std::memory_order_relaxed);

                // Apply Swing: Even steps last (2 * sw) * base, odd steps last (2 * (1 - sw)) * base
                const double durationRatio = (step % 2 == 0) ? (2.0 * sw) : (2.0 * (1.0 - sw));
                sampleCounter = static_cast<int>(baseStepSamples * durationRatio);
            }

            --sampleCounter;
        }
    }

private:
    void updateStepTiming()
    {
        if (sampleRate > 0.0)
            samplesPerStep = (60.0 / (bpm * 4.0)) * sampleRate;
    }

    double sampleRate { 44100.0 };
    double bpm { 132.0 };
    double samplesPerStep { 5512.5 };
    int sampleCounter { 0 };

    std::atomic<float> swingRatio { 0.58f }; // 58% Default UK Garage Swing
    std::atomic<bool> hostSync { true };     // Auto-sync with DAW host
    std::atomic<bool> playing { false };
    std::atomic<int> currentStep { 0 };

    // 4 Tracks x 16 Steps atomic grid
    std::array<std::array<std::atomic<bool>, numSteps>, numTracks> pattern {};
};

} // namespace underground::dsp
