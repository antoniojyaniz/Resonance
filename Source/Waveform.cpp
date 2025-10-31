#include "Waveform.h"

Waveform::Waveform()
    : writeIndex(0)
{
    audioHistory.resize(maxHistorySize, 0.0f);
    setOpaque(true);
}

Waveform::~Waveform() {}

void Waveform::clear()
{
    const juce::ScopedWriteLock writeLock(lock);
    std::fill(audioHistory.begin(), audioHistory.end(), 0.0f);
    writeIndex.store(0);
    repaint();
}

void Waveform::pushSamples(const juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedWriteLock writeLock(lock);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    //loop through the audio information sample by sample
    for (int i = 0; i < numSamples; ++i)
    {
        //sum channels then divide by number of channels to get average amplitude 
        //works with mono and stereo 
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            sum += buffer.getSample(ch, i);

        float mono = sum / static_cast<float>(numChannels);

        //writeIndex points to the next free slot
        audioHistory[writeIndex] = mono;

        //As soon as writeIndex = maxHistorySize we're reset back to zero which means we start overwriting the oldest samples
        writeIndex = (writeIndex + 1) % maxHistorySize;
    }

    juce::MessageManager::callAsync([this]() { repaint(); });
}


//loop across each pixel on the screen on the waveform and calculate how many samples ago it represents, fetch and average those samples, 
//convert that average to height, draw a mirrored bar 
void Waveform::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::lightgrey);

    //get the center, width, and gain scale for drawing
    const float gain = 0.9f * getHeight() * 0.5f;
    const float centerY = getHeight() * 0.5f;
    const int width = getWidth();

    const float zoomFactor = 0.25f; //for changing, maybe add as a slider
    const float samplesPerPixel = (float)maxHistorySize / (float)width * zoomFactor;

    const juce::ScopedReadLock readLock(lock);

    //get the current index of our audio buffer where the next sample is about to be written
    int readHead = writeIndex.load();

    g.setColour(juce::Colours::lightslategrey);

    for (int x = 0; x < width; ++x)
    {
        float rms = 0.0f;
        int count = 0;

        //downsample to meet our space restrictions 
        for (int i = 0; i < samplesPerPixel; ++i)
        {
            //Calculate pixel distance from right edge
            //Convert pixel distance to sample distance 
            //Add i to access individual samples within that pixel's chunk
            //subtract from readHead to walkback
            //Add and modulo buffer size to wrap around safely
            int pixelsBack = (getWidth() - x - 1);
            int samplesBack = (pixelsBack * samplesPerPixel) + i;
            int index = (readHead - samplesBack + maxHistorySize) % maxHistorySize;

            float sample = audioHistory[index];
            rms += sample * sample;
            ++count; //count how many samples we've proccessed for this pixel
        }

        rms = std::sqrt(rms / (float)count); //finsih the rms calc
        float barHeight = rms * gain; //how much we want to draw

        //mirrored top and bottom bars
        g.fillRect((float)x, centerY - barHeight, 1.0f, barHeight);
        g.fillRect((float)x, centerY, 1.0f, barHeight);
    }
}

void Waveform::resized()
{
    
}
