#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>

namespace underground::dsp
{

/**
 * @brief Thread-safe, real-time sample playback engine with sub-sample interpolation.
 *        Allows Drag & Drop of any WAV/AIFF audio file directly into the DSP chain.
 */
class SamplePlayer
{
public:
    SamplePlayer()
    {
        loadedSampleBuffer.setSize(2, 44100 * 10); // Pre-allocate up to 10 seconds
        loadedSampleBuffer.clear();
    }

    ~SamplePlayer() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        hostSampleRate = spec.sampleRate;
    }

    void reset()
    {
        playbackPosition.store(0.0, std::memory_order_relaxed);
        isPlaying.store(false, std::memory_order_relaxed);
    }

    void loadSample(const juce::AudioBuffer<float>& newBuffer, double sampleSourceRate)
    {
        const juce::SpinLock::ScopedLockType sl(bufferLock);

        sampleLength = newBuffer.getNumSamples();
        fileSampleRate = sampleSourceRate > 0.0 ? sampleSourceRate : hostSampleRate;

        loadedSampleBuffer.setSize(newBuffer.getNumChannels(), sampleLength, false, false, true);
        for (int ch = 0; ch < newBuffer.getNumChannels(); ++ch)
        {
            loadedSampleBuffer.copyFrom(ch, 0, newBuffer, ch, 0, sampleLength);
        }

        hasSampleLoaded.store(true, std::memory_order_release);
        trigger();
    }

    void trigger()
    {
        if (hasSampleLoaded.load(std::memory_order_acquire))
        {
            playbackPosition.store(0.0, std::memory_order_release);
            isPlaying.store(true, std::memory_order_release);
        }
    }

    void stop()
    {
        isPlaying.store(false, std::memory_order_relaxed);
    }

    bool hasSample() const noexcept
    {
        return hasSampleLoaded.load(std::memory_order_acquire);
    }

    int getLoadedSampleLength() const noexcept
    {
        return sampleLength;
    }

    const juce::AudioBuffer<float>& getBuffer() const noexcept
    {
        return loadedSampleBuffer;
    }

    void handleMidiEvent(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            if (note == 35 || note == 36 || note == 60)
            {
                trigger();
            }
        }
    }

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        auto midiIterator = midiMessages.findNextSamplePosition(0);
        const int numSamples = buffer.getNumSamples();
        const int outChannels = buffer.getNumChannels();

        if (!hasSampleLoaded.load(std::memory_order_acquire))
            return;

        const juce::SpinLock::ScopedTryLockType sl(bufferLock);
        if (!sl.isLocked())
            return;

        const int numSampleChannels = loadedSampleBuffer.getNumChannels();
        if (sampleLength <= 0 || numSampleChannels == 0)
            return;

        const double speedRatio = fileSampleRate / (hostSampleRate > 0.0 ? hostSampleRate : 44100.0);

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            while (midiIterator != midiMessages.cend() && (*midiIterator).samplePosition == sampleIdx)
            {
                handleMidiEvent((*midiIterator).getMessage());
                ++midiIterator;
            }

            if (!isPlaying.load(std::memory_order_relaxed))
                continue;

            double currentPos = playbackPosition.load(std::memory_order_relaxed);
            const int indexA = static_cast<int>(currentPos);
            const int indexB = indexA + 1;
            const float frac = static_cast<float>(currentPos - indexA);

            if (indexA >= sampleLength)
            {
                isPlaying.store(false, std::memory_order_relaxed);
                continue;
            }

            for (int ch = 0; ch < outChannels; ++ch)
            {
                const int srcCh = std::min(ch, numSampleChannels - 1);
                const float sA = loadedSampleBuffer.getSample(srcCh, indexA);
                const float sB = (indexB < sampleLength) ? loadedSampleBuffer.getSample(srcCh, indexB) : 0.0f;
                const float interpolatedSample = sA + frac * (sB - sA);

                buffer.addSample(ch, sampleIdx, interpolatedSample);
            }

            currentPos += speedRatio;
            playbackPosition.store(currentPos, std::memory_order_relaxed);
        }
    }

private:
    double hostSampleRate { 44100.0 };
    double fileSampleRate { 44100.0 };
    int sampleLength { 0 };

    juce::SpinLock bufferLock;
    juce::AudioBuffer<float> loadedSampleBuffer;

    std::atomic<bool> hasSampleLoaded { false };
    std::atomic<bool> isPlaying { false };
    std::atomic<double> playbackPosition { 0.0 };
};

} // namespace underground::dsp
