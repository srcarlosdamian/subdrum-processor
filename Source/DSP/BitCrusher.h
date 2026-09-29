#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <atomic>
#include <algorithm>

namespace underground::dsp
{

/**
 * @brief Vintage Lo-Fi BitCrusher & Sample Rate Decimator.
 *        Emulates classic 12-bit / 8-bit grit (SP-1200 / Akai vintage samplers).
 */
class BitCrusher
{
public:
    BitCrusher() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        holdSampleL = 0.0f;
        holdSampleR = 0.0f;
        sampleCounter = 0.0f;
    }

    void setBitDepth(float bits) noexcept
    {
        bitDepth.store(std::clamp(bits, 2.0f, 16.0f), std::memory_order_relaxed);
    }

    void setDownsample(float factor) noexcept
    {
        downsampleFactor.store(std::clamp(factor, 1.0f, 32.0f), std::memory_order_relaxed);
    }

    void setMix(float mixNorm) noexcept
    {
        mix.store(std::clamp(mixNorm, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const float m = mix.load(std::memory_order_relaxed);
        if (m < 0.001f)
            return;

        const float bits = bitDepth.load(std::memory_order_relaxed);
        const float downsample = downsampleFactor.load(std::memory_order_relaxed);
        const float levels = std::pow(2.0f, bits - 1.0f);
        const float invLevels = 1.0f / levels;

        auto* channelL = buffer.getWritePointer(0);
        auto* channelR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : channelL;
        const int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            sampleCounter += 1.0f;
            if (sampleCounter >= downsample)
            {
                sampleCounter -= downsample;

                const float inL = std::clamp(channelL[i], -1.0f, 1.0f);
                holdSampleL = std::round(inL * levels) * invLevels;

                const float inR = std::clamp(channelR[i], -1.0f, 1.0f);
                holdSampleR = std::round(inR * levels) * invLevels;
            }

            channelL[i] = channelL[i] * (1.0f - m) + holdSampleL * m;
            if (channelR != channelL)
                channelR[i] = channelR[i] * (1.0f - m) + holdSampleR * m;
        }
    }

private:
    double sampleRate { 44100.0 };
    std::atomic<float> bitDepth { 16.0f };
    std::atomic<float> downsampleFactor { 1.0f };
    std::atomic<float> mix { 0.0f };

    float holdSampleL { 0.0f };
    float holdSampleR { 0.0f };
    float sampleCounter { 0.0f };
};

} // namespace underground::dsp
