#include "dbMeter.h"

dbMeter::dbMeter()
{
}

dbMeter::~dbMeter()
{
}

float dbMeter::mapDb(float db) const
{
    //Remaps a normalised value (between 0 and 1) to a target range
    return juce::jmap(db, minDb, maxDb, 0.0f, 1.0f);
}

void dbMeter::setLevel(float newLevel) 
{
    //only changes if the level has changed 
    if (levelDb != newLevel)
    {
        levelDb = newLevel;

        //Constrains a value to keep it within a given range.
        levelDb = juce::jlimit(minDb, maxDb, levelDb);
        repaint();
    }
}

void dbMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour(juce::Colours::lightgrey);
    g.fillRect(bounds);

    //calculate the normalized level
    float normalizedLevel = mapDb(levelDb);

    //Create a gradient for the meter
    juce::ColourGradient gradient;
    gradient.addColour(0.0, juce::Colours::lightgrey); // -60dB
    gradient.addColour(0.7, juce::Colours::lightslategrey); // -18dB
    gradient.addColour(1.0, juce::Colours::darkgrey); // 0dB

    //Apply the gradient across the entire width
    gradient.point1 = bounds.getBottomLeft().toFloat();
    gradient.point2 = bounds.getTopLeft().toFloat();
    g.setGradientFill(gradient);

    //here we multiply the normalized value by the width of our component to essentially get the height of the meter 
    //that is added to the left edge of the meter returned by bounds.getX()
    //we round because pixel positions must be ints

    int fillHeight = juce::roundToInt(normalizedLevel * bounds.getHeight());
    int yStart = bounds.getBottom() - fillHeight;
    g.fillRect(bounds.getX(), yStart, bounds.getWidth(), fillHeight);

    //Draw the outline
    g.setColour(juce::Colours::lightslategrey);
    g.drawRect(bounds, 1);
}


void dbMeter::resized()
{
}