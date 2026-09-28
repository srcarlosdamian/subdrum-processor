#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace underground::dsp
{

/**
 * @brief Empty Room / Bedroom Acoustic Ambience Processor:
 *        Emulates the physical acoustic early reflections and diffuse resonance of an empty room.
 */
class RoomAmbience
{
public:
    RoomAmbience() = default;
    ~RoomAmbience() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        reverb.prepare(spec);
        updateParameters();
    }

    void reset()
    {
        reverb.reset();
    }

    void setMix(float norm) noexcept
    {
        roomMix.store(norm, std::memory_order_relaxed);
        updateParameters();
    }

    void setRoomSize(float norm) noexcept
    {
        roomSizeNorm.store(norm, std::memory_order_relaxed);
        updateParameters();
    }

    void setDamping(float norm) noexcept
    {
        dampingNorm.store(norm, std::memory_order_relaxed);
        updateParameters();
    }

    void process(juce::dsp::ProcessContextReplacing<float>& context)
    {
        const float currentMix = roomMix.load(std::memory_order_relaxed);
        if (currentMix < 0.001f)
            return;

        reverb.process(context);
    }

private:
    void updateParameters()
    {
        const float mix = roomMix.load(std::memory_order_relaxed);
        const float size = roomSizeNorm.load(std::memory_order_relaxed);
        const float damp = dampingNorm.load(std::memory_order_relaxed);

        juce::dsp::Reverb::Parameters params;
        params.roomSize   = juce::jlimit(0.1f, 0.95f, size);
        params.damping    = juce::jlimit(0.05f, 0.9f, damp);
        params.wetLevel   = mix * 0.75f;
        params.dryLevel   = 1.0f - (mix * 0.25f);
        params.width      = 1.0f;
        params.freezeMode = 0.0f;

        reverb.setParameters(params);
    }

    double sampleRate { 44100.0 };
    juce::dsp::Reverb reverb;

    std::atomic<float> roomMix { 0.15f };      // 15% subtle bedroom ambience by default
    std::atomic<float> roomSizeNorm { 0.45f }; // Empty bedroom scale (medium-small room)
    std::atomic<float> dampingNorm { 0.35f };  // Hard plaster / drywall reflections
};

} // namespace underground::dsp
