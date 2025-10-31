// LufsMeter.h
#pragma once
#include <JuceHeader.h>

class LufsMeter
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;

        // Set coefficients the portable way (use .coefficients, not .state)
        auto hpL = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 60.0, 0.5f);
        auto hpR = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 60.0, 0.5f);
        auto shL = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 4000.0, 0.707f,
            juce::Decibels::decibelsToGain(4.0f));
        auto shR = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 4000.0, 0.707f,
            juce::Decibels::decibelsToGain(4.0f));

        hpfL.coefficients = hpL;
        hpfR.coefficients = hpR;
        shelfL.coefficients = shL;
        shelfR.coefficients = shR;

        hpfL.reset(); hpfR.reset();
        shelfL.reset(); shelfR.reset();

        mWinSamples = juce::jmax(1, (int)std::round(0.400 * sr)); // 400 ms
        sWinSamples = juce::jmax(1, (int)std::round(3.000 * sr)); // 3 s

        mRing.assign(mWinSamples, 0.0f);
        sRing.assign(sWinSamples, 0.0f);
        mIdx = sIdx = 0;
        mSum = sSum = 0.0;

        work.setSize(2, 0);
    }

    void clear()
    {
        std::fill(mRing.begin(), mRing.end(), 0.0f);
        std::fill(sRing.begin(), sRing.end(), 0.0f);
        mIdx = sIdx = 0;
        mSum = sSum = 0.0;
        hpfL.reset(); hpfR.reset(); shelfL.reset(); shelfR.reset();
    }

    // Feed one block (mic or playback). Mono input is duplicated to stereo.
    void processBlock(const juce::AudioBuffer<float>& in)
    {
        const int n = in.getNumSamples();
        if (n <= 0) return;

        work.setSize(2, n, false, false, true);
        work.clear();

        if (in.getNumChannels() >= 2)
        {
            work.copyFrom(0, 0, in, 0, 0, n);
            work.copyFrom(1, 0, in, 1, 0, n);
        }
        else
        {
            work.copyFrom(0, 0, in, 0, 0, n);
            work.copyFrom(1, 0, in, 0, 0, n);
        }

        for (int i = 0; i < n; ++i)
        {
            float L = work.getSample(0, i);
            float R = work.getSample(1, i);

            // K-weighting: HPF then High-shelf (+4 dB @ 4 kHz)
            L = shelfL.processSample(hpfL.processSample(L));
            R = shelfR.processSample(hpfR.processSample(R));

            // BS.1770 mean channel power
            const float p = 0.5f * (L * L + R * R);

            // Momentary (400 ms)
            mSum -= mRing[mIdx];
            mRing[mIdx] = p;
            mSum += p;
            mIdx = (mIdx + 1) % mWinSamples;

            // Short-term (3 s)
            sSum -= sRing[sIdx];
            sRing[sIdx] = p;
            sSum += p;
            sIdx = (sIdx + 1) % sWinSamples;
        }
    }

    float getMomentaryLUFS() const { return powerToLufs(avgPower(mSum, mWinSamples)); }
    float getShortTermLUFS() const { return powerToLufs(avgPower(sSum, sWinSamples)); }

private:
    double sampleRate = 48000.0;

    // Per-channel filters for K-weighting
    juce::dsp::IIR::Filter<float> hpfL, hpfR;
    juce::dsp::IIR::Filter<float> shelfL, shelfR;

    // Running windows
    int mWinSamples = 1, sWinSamples = 1;
    std::vector<float> mRing, sRing;
    int mIdx = 0, sIdx = 0;
    double mSum = 0.0, sSum = 0.0;

    // Workspace
    juce::AudioBuffer<float> work;

    static float powerToLufs(double meanPower)
    {
        if (meanPower <= 0.0) return -100.0f;           // floor
        const double dbfs = 10.0 * std::log10(meanPower);
        return (float)(dbfs - 0.691);                  // BS.1770 calibration
    }

    static double avgPower(double sum, int n) { return (n > 0) ? sum / (double)n : 0.0; }
};
