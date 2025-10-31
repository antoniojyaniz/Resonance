#pragma once

#include <JuceHeader.h>

// Visualizers
#include "Oscilloscope.h"
#include "Waveform.h"
#include "StereoImage.h"
#include "SpectrumAnalyzer.h"

// Meters / analyzers
#include "dbMeter.h"
#include "TruePeakDetector.h"
#include "LufsMeter.h"

// UI / settings
#include "Settings.h"

//==============================================================================
// Main app component: hosts audio I/O, analyzers, and UI.
class MainComponent : public juce::Component,
    public juce::AudioIODeviceCallback
{
public:
    MainComponent();
    ~MainComponent() override;

    // Audio device callbacks
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceStopped() override;

    // JUCE component
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    // App state

    enum transportState { Stopped, Starting, Stopping };
    transportState state;

    //Meter mode (radio behavior)
    enum class MeterMode { DB, LUFS, TP };
    MeterMode currentMeterMode = MeterMode::DB;
    void setMeterMode(MeterMode mode);

    //Visualizer mode (radio behavior)
    enum class VisualizerMode { Oscilloscope, Spectrum, StereoImage };
    VisualizerMode currentVisualizerMode = VisualizerMode::Oscilloscope;
    void setVisualizerMode(VisualizerMode mode);

    bool isApplyingModes = false;

    //=============================================================================
    //Commands / actions
    void openButtonClicked();
    void stopButtonClicked();
    void playButtonClicked();
    void showSettings(bool show);
    void transportStateChange(transportState newState);

    //==============================================================================
    //Audio core
    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;

    std::unique_ptr<juce::FileChooser> myFileChooser;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    bool useMicInput = false;
    juce::AudioBuffer<float> micBuffer; // (optional scratch)

    // Utility
    juce::String currentFileName;
    juce::String cleanFileName(const juce::String& filePath);
    float calculateRMS(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples, int channel);
    float rmsToDb(float rms);
    float displayDb = 0.0f;

    //==============================================================================
    // Analyzers / meters
    TruePeakDetector tpDetector{ 2, 2 };   // stereo, 4x oversampling
    std::vector<float> tpPeaks;
    float tpLeftSmooth = -60.0f;
    float tpRightSmooth = -60.0f;

    LufsMeter lufsMeter;
    float lufsShortVal = -60.0f;

    // Meter widgets (three modes: DB / LUFS / TP)
    dbMeter leftMeterDisplay;
    dbMeter rightMeterDisplay;
    dbMeter lufsLeftMeterDisplay;
    dbMeter lufsRightMeterDisplay;
    dbMeter tpLeftMeterDisplay;
    dbMeter tpRightMeterDisplay;

    //==============================================================================
    //Visualizers
    Oscilloscope oscilloscopeDisplay;
    Waveform     waveformDisplay;
    StereoImage  stereoImageDisplay;
    SpectrumAnalyzer spectrumDisplay;

    //--- Centralized clear/reset (used by multiple places) --------------------
    void clearVisuals();                 
    void resetMetersAndAnalyzers();     

    bool isClearing = false;
    bool isResetting = false;

    //==============================================================================
    //UI controls
    juce::DrawableButton openButton{ "OpenButton",     juce::DrawableButton::ImageFitted };
    juce::DrawableButton playButton{ "PlayButton",     juce::DrawableButton::ImageFitted };
    juce::DrawableButton stopButton{ "StopButton",     juce::DrawableButton::ImageFitted };
    juce::DrawableButton settingsButton{ "SettingsButton", juce::DrawableButton::ImageFitted };
    juce::DrawableButton micButton{ "micButton",      juce::DrawableButton::ImageFitted };

    juce::Label appTitleLabel;

    juce::Slider positionSlider;
    juce::Label  timeLabel;
    bool userIsDraggingSlider = false;

    //Sidebars / groups
    juce::GroupComponent visualizerSidebar;
    juce::GroupComponent meterBox;

    //Visualizer mode buttons
    juce::TextButton oscilloscopeButton;
    juce::TextButton spectrumButton;
    juce::TextButton stereoImageButton;

    //Meter mode buttons
    juce::TextButton dbButton{ "dB" };
    juce::TextButton lufsButton{ "LUFS" };
    juce::TextButton tpButton{ "dBTP" };

    juce::Label meterValueLabel;

    float smoothedMeterValue = -60.0f;   // initial dB/LUFS/TP
    float meterSmoothingAlpha = 0.2f;    // 0.1–0.3 = slower, 0.6–0.8 = faster

    //Settings panel
    std::unique_ptr<Settings> settingsComponent;
    bool showingSettings = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
