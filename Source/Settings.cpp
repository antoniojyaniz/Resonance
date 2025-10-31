#include "Settings.h"

Settings::Settings(juce::AudioDeviceManager& deviceManager)
{
    // Create audio settings component
    audioSettings = std::make_unique<juce::AudioDeviceSelectorComponent>(
        deviceManager,
        1, 2, // Min I/O channels
        1, 2, // Max I/O channels
        false, // MIDI input
        false, // MIDI output
        true, // Stereo/mono combo
        false  // Advanced options
    );

    addAndMakeVisible(audioSettings.get());
}

void Settings::resized()
{
    audioSettings->setBounds(getLocalBounds());
}
