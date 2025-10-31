#include "StereoImage.h"

StereoImage::StereoImage()
    : writeIndex(0)
{
    sampleHistory.resize(maxHistorySize);
    setOpaque(true);
}

StereoImage::~StereoImage() {}

void StereoImage::clear()
{
    const juce::ScopedWriteLock writeLock(lock);
    std::fill(sampleHistory.begin(), sampleHistory.end(), StereoSample{ 0.0f, 0.0f });
    writeIndex.store(0);
    repaint();
}

void StereoImage::pushSamples(const juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedWriteLock writeLock(lock);

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (numChannels < 2)
        return; // Need stereo

    for (int i = 0; i < numSamples; ++i)
    {
        float left = buffer.getSample(0, i);
        float right = buffer.getSample(1, i);

        sampleHistory[writeIndex] = { left, right };
        writeIndex = (writeIndex + 1) % maxHistorySize;
    }

    juce::MessageManager::callAsync([this]() { repaint(); });
}

void StereoImage::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::lightgrey);

    juce::Point<float> center(bounds.getCentreX(), bounds.getBottom());

    //Draw arc 
    constexpr float arcInsetX = 1.0f;

    juce::Path arc;
    arc.startNewSubPath(bounds.getX() + arcInsetX, bounds.getBottom());
    arc.quadraticTo(
        bounds.getCentreX(), bounds.getY() - 199,                    
        bounds.getRight() - arcInsetX, bounds.getBottom()      
    );

    g.setColour(juce::Colours::lightslategrey);
    g.strokePath(arc, juce::PathStrokeType(1.5f));


    // Axes (vertical + horizontal)
    g.setColour(juce::Colours::lightslategrey);
    juce::Point<float> leftEdge(bounds.getX() + arcInsetX, bounds.getBottom());
    juce::Point<float> rightEdge(bounds.getRight() - arcInsetX, bounds.getBottom());
    juce::Point<float> arcPeak(bounds.getCentreX(), bounds.getY() - 199); // same as control point

    // Diagonal left line
    g.drawLine(center.x, center.y, arcPeak.x - (arcPeak.y - center.y), arcPeak.y + 60, 1.5f);

    // Diagonal right line
    g.drawLine(center.x, center.y, arcPeak.x + (arcPeak.y - center.y), arcPeak.y + 60, 1.5f);

    // Stereo path
    juce::Path path;
    bool started = false;

    const juce::ScopedReadLock readLock(lock);
    int index = writeIndex.load();

    const float gainX = bounds.getWidth() * 0.5f * 0.95f;  // 95% of width from center
    const float gainY = bounds.getHeight() * 0.95f;        // 95% of height

    for (int i = 0; i < maxHistorySize; ++i)
    {
        const auto& s = sampleHistory[(index + i) % maxHistorySize];

        // Rotate 45° CCW (in-phase = vertical)
        float rotatedX = (s.right - s.left) * 0.7071f;
        float rotatedY = (s.right + s.left) * 0.7071f;

        // Scale + position
        float x = center.x + rotatedX * gainX;
        float y = center.y - rotatedY * gainY;

        // Skip points outside top
        if (y > bounds.getBottom())
            continue;

        if (!started)
        {
            path.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            path.lineTo(x, y);
        }
    }

    g.setColour(juce::Colours::lightslategrey);
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

void StereoImage::resized()
{
}
