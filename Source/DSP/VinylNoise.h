#pragma once

#include <juce_dsp/juce_dsp.h>
#include <random>

namespace underground::dsp
{

/**
 * @brief Procedural Vinyl & Dust Noise Generator with dynamic auto-gating.
 *        Completely silent when idle (0% default) and dynamically tracks audio
 *        presence so there is zero persistent background hiss when stopped.
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

        auto hissCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 3500.0f, 0.8f);
        auto rumbleCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 80.0f, 0.707f);

        for (size_t i = 0; i < numChannels; ++i)
        {
            hissFilters[i].coefficients = hissCoeffs;
            hissFilters[i].reset();

            rumbleFilters[i].coefficients = rumbleCoeffs;
            rumbleFilters[i].reset();
        }

        dustFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 2500.0f, 1.5f);
        dustFilter.reset();

        noiseLevelSmoothed.reset(sampleRate, 0.05);
        dustDensitySmoothed.reset(sampleRate, 0.05);
        noiseLevelSmoothed.setCurrentAndTargetValue(0.0f);
        dustDensitySmoothed.setCurrentAndTargetValue(0.0f);

        rngState = 0x12345678;
        signalEnvelope = 0.0f;
    }

    void reset()
    {
        for (auto& f : hissFilters)
            f.reset();
        for (auto& f : rumbleFilters)
            f.reset();
        dustFilter.reset();

        noiseLevelSmoothed.setCurrentAndTargetValue(0.0f);
        dustDensitySmoothed.setCurrentAndTargetValue(0.0f);
        signalEnvelope = 0.0f;
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

        const float targetGain = noiseLevelSmoothed.getTargetValue();
        const float currentGain = noiseLevelSmoothed.getCurrentValue();

        // If noise amount is 0, completely bypass to ensure 100% silence
        if (targetGain <= 0.0001f && currentGain <= 0.0001f)
            return;

        for (size_t i = 0; i < numSamples; ++i)
        {
            const float noiseGain = noiseLevelSmoothed.getNextValue();
            const float dustDensity = dustDensitySmoothed.getNextValue();

            if (noiseGain <= 0.0001f)
                continue;

            // Track input signal presence for dynamic auto-gating
            float sampleMag = 0.0f;
            for (size_t ch = 0; ch < channels; ++ch)
                sampleMag = std::max(sampleMag, std::abs(outputBlock.getSample(static_cast<int>(ch), static_cast<int>(i))));

            // Envelope follower (fast attack, smooth release)
            if (sampleMag > signalEnvelope)
                signalEnvelope = 0.1f * sampleMag + 0.9f * signalEnvelope;
            else
                signalEnvelope *= 0.9995f;

            // Dynamic ducking/gating factor: if signal is completely silent, fade noise to 0
            const float gateFactor = juce::jlimit(0.0f, 1.0f, signalEnvelope * 20.0f);

            // Generate noise
            const float whiteSample = nextRandomFloat() * 2.0f - 1.0f;

            // Dust clicks / pops
            const float popThreshold = 0.9997f - (dustDensity * 0.006f);
            float crackleSample = 0.0f;
            if (nextRandomFloat() > popThreshold)
            {
                const float polarity = (nextRandomFloat() > 0.5f) ? 1.0f : -1.0f;
                crackleSample = polarity * (0.3f + 0.7f * nextRandomFloat());
            }

            const float filteredCrackle = dustFilter.processSample(crackleSample);

            for (size_t ch = 0; ch < channels; ++ch)
            {
                const float hiss = hissFilters[ch].processSample(whiteSample) * 0.05f;
                const float rumble = rumbleFilters[ch].processSample(whiteSample) * 0.06f;

                const float totalVinyl = (hiss + rumble + filteredCrackle * 0.2f) * noiseGain * gateFactor * 0.4f;

                const float existing = outputBlock.getSample(static_cast<int>(ch), static_cast<int>(i));
                outputBlock.setSample(static_cast<int>(ch), static_cast<int>(i), existing + totalVinyl);
            }
        }
    }

private:
    inline float nextRandomFloat() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return static_cast<float>(rngState & 0x00FFFFFF) / static_cast<float>(0x01000000);
    }

    double sampleRate { 44100.0 };
    size_t numChannels { 2 };
    uint32_t rngState { 0x12345678 };
    float signalEnvelope { 0.0f };

    std::vector<juce::dsp::IIR::Filter<float>> hissFilters;
    std::vector<juce::dsp::IIR::Filter<float>> rumbleFilters;
    juce::dsp::IIR::Filter<float> dustFilter;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> noiseLevelSmoothed { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dustDensitySmoothed { 0.0f };
};

} // namespace underground::dsp
