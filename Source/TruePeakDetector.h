#pragma once
#include <JuceHeader.h>

class TruePeakDetector
{
public:
    explicit TruePeakDetector(int channels = 2, int osPow2 = 2)
        : numChannels(channels),
        oversampling(channels,
            osPow2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR) {
    }

    void prepare(double /*sampleRate*/, int maxBlockSize)
    {
        // Oversampling doesn't need sampleRate; it needs the max block size
        oversampling.reset();
        oversampling.initProcessing((size_t)maxBlockSize);

        // Work buffer only used to up-mix/duplicate channels before oversampling
        workBuffer.setSize(numChannels, maxBlockSize, false, false, true);
    }

    void processBlock(const juce::AudioBuffer<float>& in, std::vector<float>& outPeaks)
    {
        const int n = in.getNumSamples();
        if (n <= 0) { outPeaks.assign((size_t)numChannels, 0.0f); return; }

        outPeaks.assign((size_t)numChannels, 0.0f);

        // Ensure mutable, correctly sized buffer matching 'numChannels'
        workBuffer.setSize(numChannels, n, false, false, true);
        workBuffer.clear();

        // Up-mix/copy into workBuffer so channel count/pointers are mutable
        if (in.getNumChannels() == 1 && numChannels >= 2)
        {
            workBuffer.copyFrom(0, 0, in, 0, 0, n);
            workBuffer.copyFrom(1, 0, in, 0, 0, n);
        }
        else
        {
            const int srcChans = in.getNumChannels();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const int srcCh = juce::jmin(ch, srcChans - 1);
                workBuffer.copyFrom(ch, 0, in, srcCh, 0, n);
            }
        }

        // Now create a mutable AudioBlock
        juce::dsp::AudioBlock<float> inBlock(workBuffer);

        // Oversample up (initProcessing must have been called in prepare())
        auto upBlock = oversampling.processSamplesUp(inBlock);

        // Scan absolute max per channel on the oversampled data
        for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
        {
            const float* d = upBlock.getChannelPointer(ch);
            const int upN = (int)upBlock.getNumSamples();
            float m = 0.0f;
            for (int i = 0; i < upN; ++i)
                m = std::max(m, std::abs(d[i]));
            outPeaks[ch] = m;
        }
    }


    static float linearToDb(float x) { return x > 0.0f ? 20.0f * std::log10(x) : -100.0f; }

private:
    int numChannels;
    juce::dsp::Oversampling<float> oversampling;
    juce::AudioBuffer<float> workBuffer;
};
