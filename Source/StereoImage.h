#pragma once
#include <JuceHeader.h>
#include <vector>
#include <atomic>

class StereoImage : public juce::Component
{
public:
    StereoImage();
    ~StereoImage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void pushSamples(const juce::AudioBuffer<float>& buffer);
    void clear();

private:
    struct StereoSample
    {
        float left;
        float right;
    };

    static constexpr int maxHistorySize = 2048;
    std::vector<StereoSample> sampleHistory;
    std::atomic<int> writeIndex;

    juce::ReadWriteLock lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoImage)
};
