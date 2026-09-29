#pragma once

#include <juce_dsp/juce_dsp.h>

namespace underground::dsp
{

/**
 * @brief Analog Tape Saturation module featuring 4x oversampling,
 *        asymmetric non-linear transfer function, and DC offset removal.
 */
class TapeSaturation
{
public:
    TapeSaturation() = default;
    ~TapeSaturation() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        numChannels = juce::jmax((size_t)1, (size_t)spec.numChannels);
        maxBlockSize = juce::jmax((size_t)spec.maximumBlockSize, (size_t)4096);

        // 4x Oversampling (factor 2^2 = 4)
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<juce::uint32>(numChannels),
            2, // 2 stages = 4x
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, // isMaxLatency
            false // useIntegerDelay
        );
        oversampling->initProcessing(maxBlockSize);

        // Prepare DC Blockers for each channel (high-pass at ~15 Hz)
        dcBlockers.resize(numChannels);
        auto dcCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 15.0f);
        for (auto& dc : dcBlockers)
        {
            dc.coefficients = dcCoeffs;
            dc.reset();
        }

        driveSmoothed.reset(sampleRate, 0.05); // 50ms smoothing
        mixSmoothed.reset(sampleRate, 0.05);
    }

    void reset()
    {
        if (oversampling != nullptr)
            oversampling->reset();

        for (auto& dc : dcBlockers)
            dc.reset();

        driveSmoothed.setCurrentAndTargetValue(driveSmoothed.getTargetValue());
        mixSmoothed.setCurrentAndTargetValue(mixSmoothed.getTargetValue());
    }

    void setDrive(float driveAmountDb)
    {
        // Drive in dB: 0 dB to +30 dB
        const float linearDrive = juce::Decibels::decibelsToGain(driveAmountDb);
        driveSmoothed.setTargetValue(linearDrive);
    }

    void setMix(float mixPercentage)
    {
        mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, mixPercentage));
    }

    template <typename ProcessContext>
    void process(const ProcessContext& context) noexcept
    {
        auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();

        jassert(inputBlock.getNumChannels() == outputBlock.getNumChannels());
        jassert(inputBlock.getNumSamples() == outputBlock.getNumSamples());

        const size_t numSamples = inputBlock.getNumSamples();
        const size_t channels = inputBlock.getNumChannels();

        if (channels == 0 || numSamples == 0)
            return;

        if (context.isBypassed)
        {
            outputBlock.copyFrom(inputBlock);
            return;
        }

        // Dynamically reallocate oversampling if audio device switch changes channel count or block size
        if (oversampling == nullptr || channels != numChannels || numSamples > maxBlockSize)
        {
            numChannels = channels;
            maxBlockSize = juce::jmax(numSamples, (size_t)4096);
            oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
                static_cast<juce::uint32>(numChannels),
                2,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                true,
                false
            );
            oversampling->initProcessing(maxBlockSize);

            dcBlockers.resize(numChannels);
            auto dcCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate > 0.0 ? sampleRate : 44100.0, 15.0f);
            for (auto& dc : dcBlockers)
            {
                dc.coefficients = dcCoeffs;
                dc.reset();
            }
        }

        // 1. Oversample Input Block (4x)
        juce::dsp::AudioBlock<float> oversampledBlock = oversampling->processSamplesUp(inputBlock);

        const size_t oversampledNumSamples = oversampledBlock.getNumSamples();
        const float currentDrive = driveSmoothed.getCurrentValue();
        const float makeupCompensation = 1.0f / (1.0f + 0.35f * std::log10(1.0f + currentDrive));

        // 2. Process non-linear tape saturation on oversampled signal
        for (size_t channel = 0; channel < channels; ++channel)
        {
            float* channelData = oversampledBlock.getChannelPointer(channel);

            for (size_t i = 0; i < oversampledNumSamples; ++i)
            {
                const float x = channelData[i];

                // Apply drive
                const float driven = x * currentDrive;

                // Tape non-linear transfer function:
                // Soft clipping + mild asymmetry generating 2nd harmonic (warmth) and 3rd/5th (magnetic compression)
                const float asymmetricInput = driven + 0.12f * (driven * driven) * (driven > 0.0f ? 1.0f : -1.0f);
                const float saturated = std::tanh(asymmetricInput);

                channelData[i] = saturated * makeupCompensation;
            }
        }

        // Advance smoothing parameter across the original block size
        driveSmoothed.skip(static_cast<int>(numSamples));

        // 3. Downsample back to original rate with anti-aliasing reconstruction
        oversampling->processSamplesDown(outputBlock);

        // 4. DC Blocker and Dry/Wet Mix
        for (size_t channel = 0; channel < channels; ++channel)
        {
            const float* inData = inputBlock.getChannelPointer(channel);
            float* outData = outputBlock.getChannelPointer(channel);

            // Process DC blocker sample by sample
            if (channel < dcBlockers.size())
            {
                for (size_t i = 0; i < numSamples; ++i)
                {
                    outData[i] = dcBlockers[channel].processSample(outData[i]);
                }
            }

            // Mix dry and wet
            for (size_t i = 0; i < numSamples; ++i)
            {
                const float currentMix = mixSmoothed.getNextValue();
                outData[i] = inData[i] * (1.0f - currentMix) + outData[i] * currentMix;
            }
        }
    }

private:
    double sampleRate { 44100.0 };
    size_t numChannels { 2 };
    size_t maxBlockSize { 4096 };

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::vector<juce::dsp::IIR::Filter<float>> dcBlockers;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoothed { 1.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed { 1.0f };
};

} // namespace underground::dsp
