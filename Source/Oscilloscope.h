#pragma once
#include <JuceHeader.h>

class Oscilloscope : public juce::Component
{
public:
    Oscilloscope();
    ~Oscilloscope() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void pushSamples(const juce::AudioBuffer<float>& buffer); //for pushing samples to the circular buffer 

    void clear();

private:
    static constexpr int maxHistorySize = 2048; //How many samples the oscilloscope can store and display at once.
    std::vector<float> audioHistory; //circular buffer for the audio samples
    int samplePointer; //curent pos of the circular buffer, points to the oldest sample 
    juce::ReadWriteLock lock; //Thread safety for accessing the buffer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Oscilloscope)
};