#pragma once

#include <juce_dsp/juce_dsp.h>
#include <random>

namespace underground::dsp
{

/**
 * @brief Procedural Vinyl & Dust Noise Generator.
 *        Emulates groove hiss, low-frequency rumble, and random micro-crackle/dust pops.
 */
class VinylNoise
{
public:
    VinylNoise() = default;
    ~VinylNoise() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;

        hissFilters.resize(numChannels);
        rumbleFilters.resize(numChannels);

        for (size_t i = 0; i < numChannels; ++i)
        {
            // Hiss bandpass: 800Hz - 8000Hz
            hissFilters[i].state = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 3500.0f, 0.8f);
            hissFilters[i].reset();

            // Low frequency rumble: ~60Hz
            rumbleFilters[i].state = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 80.0f, 0.707f);
            rumbleFilters[i].reset();
        }

        dustFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 2500.0f, 1.5f);
        dustFilter.reset();

        noiseLevelSmoothed.reset(sampleRate, 0.05);
        dustDensitySmoothed.reset(sampleRate, 0.05);

        // Fast random generator (Xorshift32 for real-time safety)
        rngState = 0x12345678;
    }

    void reset()
    {
        for (auto& f : hissFilters)
            f.reset();
        for (auto& f : rumbleFilters)
            f.reset();
        dustFilter.reset();

        noiseLevelSmoothed.setCurrentAndTargetValue(noiseLevelSmoothed.getTargetValue());
        dustDensitySmoothed.setCurrentAndTargetValue(dustDensitySmoothed.getTargetValue());
    }

    void setAmount(float amountLinear)
    {
        noiseLevelSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, amountLinear));
    }

    void setDustDensity(float densityNormalized)
    {
        dustDensitySmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, densityNormalized));
    }

    template <typename ProcessContext>
    void process(const ProcessContext& context) noexcept
    {
        auto& outputBlock = context.getOutputBlock();

        if (context.isBypassed)
            return;

        const size_t numSamples = outputBlock.getNumSamples();
        const size_t channels = outputBlock.getNumChannels();

        for (size_t i = 0; i < numSamples; ++i)
        {
            const float noiseGain = noiseLevelSmoothed.getNextValue();
            const float dustDensity = dustDensitySmoothed.getNextValue();

            if (noiseGain <= 1.0e-5f)
                continue;

            // Generate White Noise sample
            const float whiteSample = nextRandomFloat() * 2.0f - 1.0f;

            // Dust / Crackle Pop trigger: low probability spike
            // Threshold scales with density
            const float popThreshold = 0.9997f - (dustDensity * 0.006f);
            float crackleSample = 0.0f;
            const float r = nextRandomFloat();
            if (r > popThreshold)
            {
                // Impulsive click
                const float polarity = (nextRandomFloat() > 0.5f) ? 1.0f : -1.0f;
                crackleSample = polarity * (0.3f + 0.7f * nextRandomFloat());
            }

            const float filteredCrackle = dustFilter.processSample(crackleSample);

            for (size_t ch = 0; ch < channels; ++ch)
            {
                const float hiss = hissFilters[ch].processSample(whiteSample) * 0.06f;
                const float rumble = rumbleFilters[ch].processSample(whiteSample) * 0.08f;

                const float totalVinyl = (hiss + rumble + filteredCrackle * 0.25f) * noiseGain * 0.5f;

                const float existing = outputBlock.getSample(static_cast<int>(ch), static_cast<int>(i));
                outputBlock.setSample(static_cast<int>(ch), static_cast<int>(i), existing + totalVinyl);
            }
        }
    }

private:
    // Fast lock-free random number generator (Xorshift32)
    inline float nextRandomFloat() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        // Map uint32_t to float [0.0, 1.0)
        return static_cast<float>(rngState & 0x00FFFFFF) / static_cast<float>(0x01000000);
    }

    double sampleRate { 44100.0 };
    size_t numChannels { 2 };
    uint32_t rngState { 0x12345678 };

    std::vector<juce::dsp::IIR::Filter<float>> hissFilters;
    std::vector<juce::dsp::IIR::Filter<float>> rumbleFilters;
    juce::dsp::IIR::Filter<float> dustFilter;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> noiseLevelSmoothed { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dustDensitySmoothed { 0.5f };
};

} // namespace underground::dsp
