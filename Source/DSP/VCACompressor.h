#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace underground::dsp
{

/**
 * @brief Fast VCA-style compressor engineered for aggressive transient shaping,
 *        punch, and glue on lo-fi & underground breakbeats.
 */
class VCACompressor
{
public:
    VCACompressor() = default;
    ~VCACompressor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;

        envelopeState.assign(numChannels, 0.0f);

        thresholdSmoothed.reset(sampleRate, 0.02);
        ratioSmoothed.reset(sampleRate, 0.02);
        attackTimeSmoothed.reset(sampleRate, 0.02);
        releaseTimeSmoothed.reset(sampleRate, 0.02);
        makeupGainSmoothed.reset(sampleRate, 0.02);
        mixSmoothed.reset(sampleRate, 0.02);

        updateCoefficients();
    }

    void reset()
    {
        std::fill(envelopeState.begin(), envelopeState.end(), 0.0f);
        currentGainReductionDb.store(0.0f, std::memory_order_relaxed);
    }

    void setThreshold(float thresholdDb)
    {
        thresholdSmoothed.setTargetValue(thresholdDb);
    }

    void setRatio(float ratio)
    {
        ratioSmoothed.setTargetValue(juce::jmax(1.0f, ratio));
    }

    void setAttack(float attackMs)
    {
        attackTimeSmoothed.setTargetValue(juce::jmax(0.1f, attackMs));
    }

    void setRelease(float releaseMs)
    {
        releaseTimeSmoothed.setTargetValue(juce::jmax(5.0f, releaseMs));
    }

    void setMakeupGain(float gainDb)
    {
        makeupGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(gainDb));
    }

    void setMix(float mixPercentage)
    {
        mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, mixPercentage));
    }

    float getGainReductionDb() const noexcept
    {
        return currentGainReductionDb.load(std::memory_order_relaxed);
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
        const size_t channels = std::min(inputBlock.getNumChannels(), envelopeState.size());

        float maxGrThisBlock = 0.0f;

        for (size_t i = 0; i < numSamples; ++i)
        {
            const float thresh = thresholdSmoothed.getNextValue();
            const float rat = ratioSmoothed.getNextValue();
            const float attMs = attackTimeSmoothed.getNextValue();
            const float relMs = releaseTimeSmoothed.getNextValue();
            const float makeup = makeupGainSmoothed.getNextValue();
            const float mix = mixSmoothed.getNextValue();

            // Calculate instantaneous ballistic coefficients
            const float alphaAttack = std::exp(-1.0f / (0.001f * attMs * static_cast<float>(sampleRate)));
            const float alphaRelease = std::exp(-1.0f / (0.001f * relMs * static_cast<float>(sampleRate)));

            // Linked sidechain envelope across channels for stereo coherence
            float maxInputLevel = 0.0f;
            for (size_t channel = 0; channel < channels; ++channel)
            {
                maxInputLevel = std::max(maxInputLevel, std::abs(inputBlock.getSample(static_cast<int>(channel), static_cast<int>(i))));
            }

            const float inputDb = (maxInputLevel > 1.0e-5f) ? juce::Decibels::gainToDecibels(maxInputLevel) : -100.0f;

            // Static gain characteristic with soft knee
            const float kneeWidth = 4.0f;
            float targetGainReductionDb = 0.0f;

            if (inputDb > (thresh + kneeWidth / 2.0f))
            {
                // Above knee
                targetGainReductionDb = (thresh + (inputDb - thresh) / rat) - inputDb;
            }
            else if (inputDb > (thresh - kneeWidth / 2.0f))
            {
                // In knee
                const float x = inputDb - thresh + kneeWidth / 2.0f;
                const float compressed = inputDb + ((1.0f / rat - 1.0f) * (x * x)) / (2.0f * kneeWidth);
                targetGainReductionDb = compressed - inputDb;
            }
            else
            {
                // Below knee
                targetGainReductionDb = 0.0f;
            }

            // Target gain reduction is negative (e.g. -6 dB)
            const float targetGrMagnitude = -targetGainReductionDb;

            // Ballistics smoothing (attack when GR increases, release when GR relaxes)
            for (size_t channel = 0; channel < channels; ++channel)
            {
                auto& env = envelopeState[channel];
                if (targetGrMagnitude > env)
                    env = alphaAttack * env + (1.0f - alphaAttack) * targetGrMagnitude;
                else
                    env = alphaRelease * env + (1.0f - alphaRelease) * targetGrMagnitude;

                const float appliedGainReduction = juce::Decibels::decibelsToGain(-env);
                const float inSample = inputBlock.getSample(static_cast<int>(channel), static_cast<int>(i));
                const float wetSample = inSample * appliedGainReduction * makeup;

                outputBlock.setSample(static_cast<int>(channel), static_cast<int>(i), inSample * (1.0f - mix) + wetSample * mix);
                maxGrThisBlock = std::max(maxGrThisBlock, env);
            }
        }

        currentGainReductionDb.store(maxGrThisBlock, std::memory_order_relaxed);
    }

private:
    void updateCoefficients() noexcept {}

    double sampleRate { 44100.0 };
    size_t numChannels { 2 };

    std::vector<float> envelopeState;
    std::atomic<float> currentGainReductionDb { 0.0f };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> thresholdSmoothed { -12.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ratioSmoothed { 4.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> attackTimeSmoothed { 5.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> releaseTimeSmoothed { 50.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> makeupGainSmoothed { 1.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed { 1.0f };
};

} // namespace underground::dsp
