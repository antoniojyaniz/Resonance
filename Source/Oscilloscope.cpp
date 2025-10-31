#include "Oscilloscope.h"

Oscilloscope::Oscilloscope()
    : samplePointer(0)
{
    audioHistory.resize(maxHistorySize, 0.0f); //size our buffer and fill with 0s

    setOpaque(true); //skip the redraw of any components beneath this one
}

Oscilloscope::~Oscilloscope()
{
}

void Oscilloscope::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::lightgrey);  //background

    g.setColour(juce::Colours::lightslategrey);  //border
    g.drawRect(getLocalBounds(), 1);    //draw the border

    g.setColour(juce::Colours::lightslategrey); //waveform

    const float centerY = getHeight() / 2.0f;
    const float gainFactor = getHeight() / 2.0f * 0.9f; //determines how much the waveform will scale 

    juce::Path waveformPath; // used to store the points of the audio waveform as a series of connected lines.
                             //The waveform is constructed by iterating through the audioHistory buffer and adding points to the waveformPath

    bool pathStarted = false; //ensures that the first point of the waveform is added using startNewSubPath while subsequent points are connected using lineTo

    const juce::ScopedReadLock readLock(lock);

    //iterate through all the samples in our buffer 
    for (int i = 0; i < maxHistorySize; ++i)
    {
        float x = (float)i / maxHistorySize * getWidth(); //we normalize the sample index to a value between 0.0 and 1.0 and scale it to the width of the oscilloscope
        float y = centerY - audioHistory[(i + samplePointer) % maxHistorySize] * gainFactor; 
        //Adding i and samplePointer ensures the loop starts at the oldest sample, the modulo ensures we wrap around the buffer
        //we subract to center and multiply to scale 

        if (!pathStarted)
        {
            waveformPath.startNewSubPath(x, y);
            pathStarted = true;
        }
        else
        {
            waveformPath.lineTo(x, y);
        }
    }

    g.strokePath(waveformPath, juce::PathStrokeType(1.5f)); //draw the waveform

    //Draw center line
    g.setColour(juce::Colours::lightslategrey);
    g.drawHorizontalLine(getHeight() / 2, 0.0f, (float)getWidth());
}

void Oscilloscope::resized()
{
}

void Oscilloscope::pushSamples(const juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedWriteLock writeLock(lock);

    //get num of channels and samples for the input 
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    //For each sample calculate average across channels
    for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
    {
        float sum = 0.0f;

        // Sum all channels
        for (int channel = 0; channel < numChannels; ++channel)
        {
            sum += buffer.getSample(channel, sampleIdx);
        }

        //calculate  and store in the buffer
        float average = numChannels > 0 ? sum / numChannels : 0.0f;

        audioHistory[samplePointer] = average;
        samplePointer = (samplePointer + 1) % maxHistorySize;
    }

    //Request a repaint to show the new data
    //triggers the paint method 
    juce::MessageManager::callAsync([this]() { repaint(); }); 
}

void Oscilloscope::clear()
{
    const juce::ScopedWriteLock writeLock(lock);
    std::fill(audioHistory.begin(), audioHistory.end(), 0.0f);
    samplePointer = 0;
    repaint();
}