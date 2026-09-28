#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace underground::dsp
{

/**
 * @brief 16-Step Pattern Sequencer (TR-808 style step player).
 *        Sample-accurate timing, supports internal BPM & Host DAW sync.
 */
class StepSequencer
{
public:
    static constexpr int numSteps = 16;
    static constexpr int numTracks = 2; // Track 0: Kick (36), Track 1: Snare (38)

    StepSequencer()
    {
        loadDefaultPattern();
    }

    void prepare(double sampleRateHz)
    {
        sampleRate = sampleRateHz > 0.0 ? sampleRateHz : 44100.0;
        samplesPerStep = (60.0 / (bpm * 4.0)) * sampleRate; // 16th notes
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
        if (sampleRate > 0.0)
            samplesPerStep = (60.0 / (bpm * 4.0)) * sampleRate;
    }

    double getBpm() const noexcept
    {
        return bpm;
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

    void loadDefaultPattern()
    {
        clear();
        // UK 2-Step / Garage Default Pattern:
        // Kick on step 0 (1), step 6 (2.3), step 10 (3.3)
        pattern[0][0].store(true, std::memory_order_release);
        pattern[0][6].store(true, std::memory_order_release);
        pattern[0][10].store(true, std::memory_order_release);

        // Snare on step 4 (Beat 2) and step 12 (Beat 4)
        pattern[1][4].store(true, std::memory_order_release);
        pattern[1][12].store(true, std::memory_order_release);
    }

    int getCurrentStep() const noexcept
    {
        return currentStep.load(std::memory_order_relaxed);
    }

    /**
     * @brief Process sequencer clock in audio block and inject MIDI note-ons.
     */
    void process(juce::MidiBuffer& midiMessages, int numSamples, double hostBpm = 0.0)
    {
        if (hostBpm > 20.0)
            setBpm(hostBpm);

        if (!playing.load(std::memory_order_relaxed))
            return;

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            if (sampleCounter <= 0)
            {
                // Trigger current step notes
                const int step = currentStep.load(std::memory_order_relaxed);

                // Track 0: KICK (Note 36)
                if (pattern[0][step].load(std::memory_order_relaxed))
                {
                    midiMessages.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), sampleIdx);
                }

                // Track 1: SNARE (Note 38)
                if (pattern[1][step].load(std::memory_order_relaxed))
                {
                    midiMessages.addEvent(juce::MidiMessage::noteOn(1, 38, 1.0f), sampleIdx);
                }

                // Advance step
                const int nextStep = (step + 1) % numSteps;
                currentStep.store(nextStep, std::memory_order_relaxed);
                sampleCounter = static_cast<int>(samplesPerStep);
            }

            --sampleCounter;
        }
    }

private:
    double sampleRate { 44100.0 };
    double bpm { 132.0 };
    double samplesPerStep { 5512.5 };
    int sampleCounter { 0 };

    std::atomic<bool> playing { false };
    std::atomic<int> currentStep { 0 };

    // 2 Tracks x 16 Steps atomic grid
    std::array<std::array<std::atomic<bool>, numSteps>, numTracks> pattern {};
};

} // namespace underground::dsp
