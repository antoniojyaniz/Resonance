#pragma once
#include <JuceHeader.h>

class Settings : public juce::Component
{
public:
    Settings(juce::AudioDeviceManager& deviceManager);
    ~Settings() override = default;

    void resized() override;

private:
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSettings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Settings)
};
