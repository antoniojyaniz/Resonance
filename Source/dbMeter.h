#pragma once
#include <JuceHeader.h>

class dbMeter : public juce::Component
{
public:
	dbMeter();
	~dbMeter() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

	void setLevel(float newLevel);
	float getLevel() const { return levelDb; }
	void setMinDb(float newMinDb) { minDb = newMinDb; repaint(); }
	void setMaxDb(float newMaxDb) { maxDb = newMaxDb; repaint(); }

private:

	float levelDb = -60.0f;  
	float minDb = -60.0f;    
	float maxDb = 0.0f;      

	float mapDb(float db) const;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(dbMeter)
};