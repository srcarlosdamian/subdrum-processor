#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <atomic>

namespace underground::dsp
{

/**
 * @brief UK 2-Step / Garage Vintage Tape Dub Echo:
 *        - High-resolution circular delay line with tape-saturation feedback loop.
 *        - Analog high-cut damping (warm analog tape decay on repeats).
 *        - Stereo cross-feed spread for deep spatial immersion.
 */
class TapeEcho
{
public:
    TapeEcho() = default;
    ~TapeEcho() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        const int maxDelaySamples = static_cast<int>(sampleRate * 2.0); // 2.0 seconds max delay
        delayBuffer.setSize(2, maxDelaySamples);
        delayBuffer.clear();
        writePos = 0;

        dampFilterL.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 4200.0f, 0.707f);
        dampFilterR.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 4200.0f, 0.707f);
        dampFilterL.reset();
        dampFilterR.reset();

        reset();
    }

    void reset()
    {
        delayBuffer.clear();
        writePos = 0;
        dampFilterL.reset();
        dampFilterR.reset();
    }

    void setTime(float ms) noexcept { delayTimeMs.store(ms, std::memory_order_relaxed); }
    void setFeedback(float norm) noexcept { feedback.store(norm, std::memory_order_relaxed); }
    void setDamping(float cutoffHz) noexcept
    {
        if (std::abs(lastCutoffHz - cutoffHz) > 20.0f)
        {
            lastCutoffHz = cutoffHz;
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, juce::jlimit(800.0f, 18000.0f, cutoffHz), 0.707f);
            dampFilterL.coefficients = coeffs;
            dampFilterR.coefficients = coeffs;
        }
    }
    void setMix(float norm) noexcept { mix.store(norm, std::memory_order_relaxed); }

    void process(juce::dsp::ProcessContextReplacing<float>& context)
    {
        const auto& inBlock = context.getInputBlock();
        auto& outBlock = context.getOutputBlock();

        const int numSamples = static_cast<int>(inBlock.getNumSamples());
        const int numChannels = static_cast<int>(inBlock.getNumChannels());
        const float currentMix = mix.load(std::memory_order_relaxed);

        if (currentMix < 0.001f)
            return;

        const float targetDelaySamples = (delayTimeMs.load(std::memory_order_relaxed) * 0.001f) * static_cast<float>(sampleRate);
        const float currentFeedback = juce::jlimit(0.0f, 0.88f, feedback.load(std::memory_order_relaxed));
        const int bufferLength = delayBuffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            float readIndex = static_cast<float>(writePos) - targetDelaySamples;
            if (readIndex < 0.0f)
                readIndex += static_cast<float>(bufferLength);

            const int readIndexInt = static_cast<int>(readIndex);
            const float frac = readIndex - static_cast<float>(readIndexInt);
            const int readIndexNext = (readIndexInt + 1) % bufferLength;

            // Interpolated delay read for L and R
            const float delayOutL = delayBuffer.getSample(0, readIndexInt) * (1.0f - frac) + delayBuffer.getSample(0, readIndexNext) * frac;
            const float delayOutR = (numChannels > 1)
                ? (delayBuffer.getSample(1, readIndexInt) * (1.0f - frac) + delayBuffer.getSample(1, readIndexNext) * frac)
                : delayOutL;

            // Damped & saturated feedback signal with subtle stereo cross-feed
            const float dampedL = dampFilterL.processSample(delayOutL);
            const float dampedR = dampFilterR.processSample(delayOutR);

            const float satFeedbackL = std::tanh((dampedL * 0.85f + dampedR * 0.15f) * currentFeedback * 1.15f);
            const float satFeedbackR = std::tanh((dampedR * 0.85f + dampedL * 0.15f) * currentFeedback * 1.15f);

            // Fetch dry input
            const float inL = inBlock.getSample(0, i);
            const float inR = (numChannels > 1) ? inBlock.getSample(1, i) : inL;

            // Write into delay buffer (Dry In + Saturated Feedback)
            delayBuffer.setSample(0, writePos, inL + satFeedbackL);
            if (numChannels > 1)
                delayBuffer.setSample(1, writePos, inR + satFeedbackR);

            // Mix dry and wet output
            outBlock.setSample(0, i, inL * (1.0f - currentMix * 0.35f) + delayOutL * currentMix);
            if (numChannels > 1)
                outBlock.setSample(1, i, inR * (1.0f - currentMix * 0.35f) + delayOutR * currentMix);

            writePos = (writePos + 1) % bufferLength;
        }
    }

private:
    double sampleRate { 44100.0 };
    juce::AudioBuffer<float> delayBuffer;
    int writePos { 0 };

    std::atomic<float> delayTimeMs { 260.0f }; // Classic 2-step dotted groove delay time (approx 135 bpm dotted 8th)
    std::atomic<float> feedback { 0.42f };
    std::atomic<float> mix { 0.0f }; // 0% by default, ready to turn up
    float lastCutoffHz { 4200.0f };

    juce::dsp::IIR::Filter<float> dampFilterL;
    juce::dsp::IIR::Filter<float> dampFilterR;
};

} // namespace underground::dsp
