#pragma once

#include <juce_dsp/juce_dsp.h>

namespace underground::dsp
{

/**
 * @brief Resonant Lo-Fi Sampler Low-Pass Filter.
 *        Emulates the steep reconstruction roll-off and warm resonance
 *        typical of vintage 12-bit/16-bit drum samplers.
 */
class SamplerFilter
{
public:
    SamplerFilter() = default;
    ~SamplerFilter() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;

        filters.resize(numChannels);
        for (auto& filter : filters)
        {
            filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            filter.reset();
        }

        cutoffSmoothed.reset(sampleRate, 0.03); // 30ms smoothing
        resonanceSmoothed.reset(sampleRate, 0.03);
    }

    void reset()
    {
        for (auto& filter : filters)
            filter.reset();

        cutoffSmoothed.setCurrentAndTargetValue(cutoffSmoothed.getTargetValue());
        resonanceSmoothed.setCurrentAndTargetValue(resonanceSmoothed.getTargetValue());
    }

    void setCutoff(float cutoffHz)
    {
        cutoffSmoothed.setTargetValue(juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.495), cutoffHz));
    }

    void setResonance(float resonanceQ)
    {
        resonanceSmoothed.setTargetValue(juce::jlimit(0.1f, 10.0f, resonanceQ));
    }

    template <typename ProcessContext>
    void process(const ProcessContext& context) noexcept
    {
        auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();

        if (context.isBypassed)
        {
            outputBlock.copyFrom(inputBlock);
            return;
        }

        const size_t numSamples = inputBlock.getNumSamples();
        const size_t channels = std::min(inputBlock.getNumChannels(), filters.size());

        for (size_t channel = 0; channel < channels; ++channel)
        {
            const float* inData = inputBlock.getChannelPointer(channel);
            float* outData = outputBlock.getChannelPointer(channel);
            auto& filter = filters[channel];

            for (size_t i = 0; i < numSamples; ++i)
            {
                filter.setCutoffFrequency(cutoffSmoothed.getNextValue());
                filter.setResonance(resonanceSmoothed.getNextValue());
                outData[i] = filter.processSample(static_cast<int>(channel), inData[i]);
            }
        }
    }

private:
    double sampleRate { 44100.0 };
    size_t numChannels { 2 };

    std::vector<juce::dsp::StateVariableTPTFilter<float>> filters;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> cutoffSmoothed { 12000.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> resonanceSmoothed { 0.707f };
};

} // namespace underground::dsp
