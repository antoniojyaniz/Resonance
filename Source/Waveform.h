#pragma once
#include <JuceHeader.h>
#include <vector>
#include <atomic>

class Waveform : public juce::Component
{
public:
    Waveform();
    ~Waveform() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void pushSamples(const juce::AudioBuffer<float>& buffer);
    void clear();

private:
    juce::ReadWriteLock lock;

    static constexpr int historySeconds = 10;
    static constexpr int sampleRate = 44100; 
    static constexpr int maxHistorySize = historySeconds * sampleRate;

    std::vector<float> audioHistory;
    std::atomic<int> writeIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Waveform)
};
