#include "MainComponent.h"

juce::String formatTime(double seconds);

// helpers 
void MainComponent::clearVisuals()
{
    if (isClearing) return;                  // guard against re-entry
    isClearing = true;

    oscilloscopeDisplay.clear();
    waveformDisplay.clear();
    stereoImageDisplay.clear();
    spectrumDisplay.clear();

    isClearing = false;
}

void MainComponent::resetMetersAndAnalyzers()
{
    if (isResetting) return;                 //guard 
    isResetting = true;

    //Reset UI meters
    leftMeterDisplay.setLevel(-60.0f);
    rightMeterDisplay.setLevel(-60.0f);
    lufsLeftMeterDisplay.setLevel(-60.0f);
    lufsRightMeterDisplay.setLevel(-60.0f);
    tpLeftMeterDisplay.setLevel(-60.0f);
    tpRightMeterDisplay.setLevel(-60.0f);

    //Reset analyzer state 
    lufsMeter.clear();
    tpLeftSmooth = -60.0f;
    tpRightSmooth = -60.0f;

    //Reset transport UI 
    positionSlider.setValue(0.0, juce::dontSendNotification);
    timeLabel.setText("00:00", juce::dontSendNotification);

    isResetting = false;
}

//==============================================================================

MainComponent::MainComponent()
    : state(Stopped),
    openButton("openButton", juce::DrawableButton::ImageFitted),
    playButton("playButton", juce::DrawableButton::ImageFitted),
    stopButton("stopButton", juce::DrawableButton::ImageFitted),
    settingsButton("SettingsButton", juce::DrawableButton::ImageFitted),
    micButton("micButton", juce::DrawableButton::ImageFitted)
{
    deviceManager.initialiseWithDefaultDevices(1, 2); // 1 input, 2 output
    deviceManager.addAudioCallback(this);

    //--- Meter widgets --------------------------------------------------------
    addAndMakeVisible(leftMeterDisplay);
    leftMeterDisplay.setMinDb(-60.0); leftMeterDisplay.setMaxDb(0.0);
    leftMeterDisplay.setLevel(-60.0f); leftMeterDisplay.setVisible(true);

    addAndMakeVisible(rightMeterDisplay);
    rightMeterDisplay.setMinDb(-60.0); rightMeterDisplay.setMaxDb(0.0);
    rightMeterDisplay.setLevel(-60.0f); rightMeterDisplay.setVisible(true);

    addAndMakeVisible(lufsLeftMeterDisplay);
    lufsLeftMeterDisplay.setMinDb(-60.0); lufsLeftMeterDisplay.setMaxDb(0.0);
    lufsLeftMeterDisplay.setLevel(-60.0f); lufsLeftMeterDisplay.setVisible(false);

    addAndMakeVisible(lufsRightMeterDisplay);
    lufsRightMeterDisplay.setMinDb(-60.0); lufsRightMeterDisplay.setMaxDb(0.0);
    lufsRightMeterDisplay.setLevel(-60.0f); lufsRightMeterDisplay.setVisible(false);

    addAndMakeVisible(tpLeftMeterDisplay);
    tpLeftMeterDisplay.setMinDb(-60.0); tpLeftMeterDisplay.setMaxDb(0.0);
    tpLeftMeterDisplay.setLevel(-60.0f); tpLeftMeterDisplay.setVisible(false);

    addAndMakeVisible(tpRightMeterDisplay);
    tpRightMeterDisplay.setMinDb(-60.0); tpRightMeterDisplay.setMaxDb(0.0);
    tpRightMeterDisplay.setLevel(-60.0f); tpRightMeterDisplay.setVisible(false);

    //--- Icons ----------------------------------------------------------------
    auto fileSvg = juce::Drawable::createFromImageData(BinaryData::file_svg, BinaryData::file_svgSize);
    auto playSvg = juce::Drawable::createFromImageData(BinaryData::play_svg, BinaryData::play_svgSize);
    auto pauseSvg = juce::Drawable::createFromImageData(BinaryData::pause_svg, BinaryData::pause_svgSize);
    auto cogSvg = juce::Drawable::createFromImageData(BinaryData::cog_svg, BinaryData::cog_svgSize);
    auto micSvg = juce::Drawable::createFromImageData(BinaryData::mic_svg, BinaryData::mic_svgSize);

    //--- Transport buttons ----------------------------------------------------
    openButton.setImages(fileSvg.get());
    openButton.setButtonText(""); openButton.setTooltip("Open File");
    openButton.onClick = [this] { openButtonClicked(); };
    addAndMakeVisible(openButton);

    playButton.setImages(playSvg.get());
    playButton.setButtonText(""); playButton.setTooltip("Play");
    playButton.setEnabled(true);
    playButton.onClick = [this] { playButtonClicked(); };
    addAndMakeVisible(playButton);

    stopButton.setImages(pauseSvg.get());
    stopButton.setButtonText(""); stopButton.setTooltip("Pause");
    stopButton.setEnabled(false);
    stopButton.onClick = [this] { stopButtonClicked(); };
    addAndMakeVisible(stopButton);

    //--- Settings button (always visible) ------------------------------------
    settingsButton.setImages(cogSvg.get());
    settingsButton.setButtonText(""); settingsButton.setTooltip("Settings");
    settingsButton.onClick = [this] { showSettings(!showingSettings); };
    addAndMakeVisible(settingsButton);

    //--- Mic toggle -----------------------------------------------------------
    micButton.setImages(micSvg.get());
    micButton.setButtonText(""); micButton.setTooltip("Mic");
    micButton.setClickingTogglesState(false);
    micButton.onClick = [this]
        {
            //Switching source context => clear visuals + reset analyzers/meters/UI
            useMicInput = !useMicInput;

            if (useMicInput && transportSource.isPlaying())
                stopButtonClicked();

            clearVisuals();
            resetMetersAndAnalyzers();

            //Show/hide playback UI portions when in mic mode
            const bool showPlayback = !useMicInput && !showingSettings;
            openButton.setVisible(showPlayback);
            playButton.setVisible(showPlayback);
            stopButton.setVisible(showPlayback);
            positionSlider.setVisible(showPlayback);
            timeLabel.setVisible(showPlayback);

            resized();
            repaint();
        };
    addAndMakeVisible(micButton);

    //--- Settings panel (hidden initially) -----------------------------------
    settingsComponent = std::make_unique<Settings>(deviceManager);
    addAndMakeVisible(settingsComponent.get());
    settingsComponent->setVisible(false);

    //--- Seekbar + time -------------------------------------------------------
    positionSlider.setRange(0.0, 1.0);
    positionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    positionSlider.setColour(juce::Slider::thumbColourId, juce::Colours::lightgrey);
    positionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.onValueChange = [this]
        {
            const double duration = transportSource.getLengthInSeconds();
            if (duration > 0.0)
            {
                const double newPosition = positionSlider.getValue() * duration;
                if (userIsDraggingSlider)
                    transportSource.setPosition(newPosition);
                timeLabel.setText(formatTime(newPosition), juce::dontSendNotification);
            }
        };
    positionSlider.onDragStart = [this] { userIsDraggingSlider = true;  };
    positionSlider.onDragEnd = [this] { userIsDraggingSlider = false; };
    addAndMakeVisible(positionSlider);

    timeLabel.setText("00:00", juce::dontSendNotification);
    timeLabel.setJustificationType(juce::Justification::centredRight);
    timeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(timeLabel);

    //--- Sidebar + meter group ------------------------------------------------
    visualizerSidebar.setText("Visualizers");
    addAndMakeVisible(visualizerSidebar);

    meterBox.setText("");
    addAndMakeVisible(meterBox);

    //--- Meter mode buttons (radio via code; all enabled, styled the same) ---
    auto styleButton = [](juce::TextButton& b)
        {
            b.setColour(juce::TextButton::buttonColourId, juce::Colours::black);      // OFF
            b.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkgrey);   // ON
            b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            b.setClickingTogglesState(false);
        };

    styleButton(dbButton);
    styleButton(lufsButton);
    styleButton(tpButton);

    dbButton.onClick = [this] { setMeterMode(MeterMode::DB);   };
    lufsButton.onClick = [this] { setMeterMode(MeterMode::LUFS); };
    tpButton.onClick = [this] { setMeterMode(MeterMode::TP);   };

    addAndMakeVisible(dbButton);
    addAndMakeVisible(lufsButton);
    addAndMakeVisible(tpButton);
    setMeterMode(MeterMode::DB); //default meter mode

    meterValueLabel.setText("--.-", juce::dontSendNotification);
    meterValueLabel.setJustificationType(juce::Justification::centred);
    meterValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    meterValueLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    addAndMakeVisible(meterValueLabel);

    //--- Visualizer toggles ---------------------------------------------------
    styleButton(oscilloscopeButton);
    styleButton(spectrumButton);
    styleButton(stereoImageButton);

    oscilloscopeButton.setButtonText("Oscilloscope");
    spectrumButton.setButtonText("Spectrum");
    stereoImageButton.setButtonText("Stereoimage");

    oscilloscopeButton.onClick = [this] { setVisualizerMode(VisualizerMode::Oscilloscope); };
    spectrumButton.onClick = [this] { setVisualizerMode(VisualizerMode::Spectrum);     };
    stereoImageButton.onClick = [this] { setVisualizerMode(VisualizerMode::StereoImage);  };

    addAndMakeVisible(oscilloscopeButton);
    addAndMakeVisible(spectrumButton);
    addAndMakeVisible(stereoImageButton);

    //--- Visualizer components ------------------------------------------------
    addAndMakeVisible(oscilloscopeDisplay);
    addAndMakeVisible(spectrumDisplay);
    addAndMakeVisible(stereoImageDisplay);

    spectrumDisplay.setDbRange(-90.0f, 0.0f);
    spectrumDisplay.setFreqRange(20.0f, 20000.0f);
    spectrumDisplay.setSmoothing(/*timeAlpha*/ 0.25f, /*freqSmoothRadius*/ 1);

    setVisualizerMode(VisualizerMode::Spectrum); // default visualizer

    //--- Title ----------------------------------------------------------------
    appTitleLabel.setText("Resonance", juce::dontSendNotification);
    appTitleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    appTitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    appTitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(appTitleLabel);

    addAndMakeVisible(waveformDisplay);

    formatManager.registerBasicFormats();
    setSize(620, 350);
}

MainComponent::~MainComponent()
{
    deviceManager.removeAudioCallback(this);
}

//==============================================================================

void MainComponent::setMeterMode(MeterMode mode)
{
    if (isApplyingModes) return;
    isApplyingModes = true;

    currentMeterMode = mode;

    const bool showUI = !showingSettings;

    dbButton.setToggleState(mode == MeterMode::DB, juce::dontSendNotification);
    lufsButton.setToggleState(mode == MeterMode::LUFS, juce::dontSendNotification);
    tpButton.setToggleState(mode == MeterMode::TP, juce::dontSendNotification);

    dbButton.setEnabled(true);
    lufsButton.setEnabled(true);
    tpButton.setEnabled(true);

    const bool showDB = showUI && (mode == MeterMode::DB);
    const bool showLUFS = showUI && (mode == MeterMode::LUFS);
    const bool showTP = showUI && (mode == MeterMode::TP);

    leftMeterDisplay.setVisible(showDB);
    rightMeterDisplay.setVisible(showDB);
    lufsLeftMeterDisplay.setVisible(showLUFS);
    lufsRightMeterDisplay.setVisible(showLUFS);
    tpLeftMeterDisplay.setVisible(showTP);
    tpRightMeterDisplay.setVisible(showTP);

    resized();   //safe now that resized() doesn't call back into setters
    repaint();

    isApplyingModes = false;
    meterValueLabel.setText("--.-", juce::dontSendNotification);

}

void MainComponent::setVisualizerMode(VisualizerMode mode)
{
    if (isApplyingModes) return;
    isApplyingModes = true;

    currentVisualizerMode = mode;

    oscilloscopeButton.setToggleState(mode == VisualizerMode::Oscilloscope, juce::dontSendNotification);
    spectrumButton.setToggleState(mode == VisualizerMode::Spectrum, juce::dontSendNotification);
    stereoImageButton.setToggleState(mode == VisualizerMode::StereoImage, juce::dontSendNotification);

    oscilloscopeButton.setEnabled(true);
    spectrumButton.setEnabled(true);
    stereoImageButton.setEnabled(true);

    const bool showUI = !showingSettings;
    const bool showScope = showUI && (mode == VisualizerMode::Oscilloscope);
    const bool showSpec = showUI && (mode == VisualizerMode::Spectrum);
    const bool showStereo = showUI && (mode == VisualizerMode::StereoImage);

    oscilloscopeDisplay.setVisible(showScope);
    spectrumDisplay.setVisible(showSpec);
    stereoImageDisplay.setVisible(showStereo);

    resized();   // safe now
    repaint();

    isApplyingModes = false;
}


void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const auto sampleRate = device->getCurrentSampleRate();
    const auto bufferSize = device->getCurrentBufferSizeSamples();

    transportSource.prepareToPlay(bufferSize, sampleRate);

    // analyzers
    spectrumDisplay.setSampleRate(sampleRate);
    tpDetector.prepare(sampleRate, bufferSize);
    lufsMeter.prepare(sampleRate);

    tpLeftSmooth = -60.0f;
    tpRightSmooth = -60.0f;
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& ctx)
{
    juce::AudioBuffer<float> outputBuffer(outputChannelData, numOutputChannels, numSamples);
    outputBuffer.clear();

    const bool tpVisible = tpLeftMeterDisplay.isVisible() || tpRightMeterDisplay.isVisible();
    const bool dbVisible = leftMeterDisplay.isVisible() || rightMeterDisplay.isVisible();
    const bool lufsVisible = lufsLeftMeterDisplay.isVisible() || lufsRightMeterDisplay.isVisible();

    float leftDb = -100.0f, rightDb = -100.0f;
    float tpLdb = -100.0f, tpRdb = -100.0f;
    float lufsShort = -100.0f;
    bool  haveTP = false, haveDB = false, haveLUFS = false;

    if (useMicInput)
    {
        juce::AudioBuffer<float> inputBuffer(const_cast<float**>(inputChannelData), numInputChannels, numSamples);

        //Feed whichever visualizer is currently visible (cheap enough to feed all too)
        oscilloscopeDisplay.pushSamples(inputBuffer);
        waveformDisplay.pushSamples(inputBuffer);
        spectrumDisplay.pushSamples(inputBuffer);

        if (numInputChannels >= 2)
        {
            stereoImageDisplay.pushSamples(inputBuffer);
        }
        else
        {
            //Duplicate mono to L+R so the stereo image has two channels
            juce::AudioBuffer<float> tempStereo(2, numSamples);
            tempStereo.copyFrom(0, 0, inputBuffer, 0, 0, numSamples);
            tempStereo.copyFrom(1, 0, inputBuffer, 0, 0, numSamples);
            stereoImageDisplay.pushSamples(tempStereo);
        }

        if (dbVisible)
        {
            const float leftRMS = numInputChannels > 0 ? calculateRMS(inputBuffer, 0, numSamples, 0) : 0.0f;
            const float rightRMS = numInputChannels > 1 ? calculateRMS(inputBuffer, 0, numSamples, 1) : leftRMS;
            leftDb = rmsToDb(leftRMS);
            rightDb = rmsToDb(rightRMS);
            haveDB = true;
        }

        if (tpVisible)
        {
            tpDetector.processBlock(inputBuffer, tpPeaks);
            const float tpL = TruePeakDetector::linearToDb(tpPeaks.size() > 0 ? tpPeaks[0] : 0.0f);
            const float tpR = TruePeakDetector::linearToDb(tpPeaks.size() > 1 ? tpPeaks[1] : tpL);

            const float attack = 0.6f, release = 0.2f;
            tpLeftSmooth = (tpL > tpLeftSmooth) ? attack * tpL + (1.0f - attack) * tpLeftSmooth
                : release * tpL + (1.0f - release) * tpLeftSmooth;
            tpRightSmooth = (tpR > tpRightSmooth) ? attack * tpR + (1.0f - attack) * tpRightSmooth
                : release * tpR + (1.0f - release) * tpRightSmooth;

            tpLdb = tpLeftSmooth; tpRdb = tpRightSmooth;
            haveTP = true;
        }

        if (lufsVisible)
        {
            lufsMeter.processBlock(inputBuffer);
            lufsShort = lufsMeter.getShortTermLUFS();
            haveLUFS = true;
        }
    }
    else if (transportSource.isPlaying())
    {
        juce::AudioSourceChannelInfo track(&outputBuffer, 0, numSamples);
        transportSource.getNextAudioBlock(track);

        oscilloscopeDisplay.pushSamples(outputBuffer);
        waveformDisplay.pushSamples(outputBuffer);
        stereoImageDisplay.pushSamples(outputBuffer);
        spectrumDisplay.pushSamples(outputBuffer);

        if (dbVisible)
        {
            const float leftRMS = numOutputChannels > 0 ? calculateRMS(outputBuffer, 0, numSamples, 0) : 0.0f;
            const float rightRMS = numOutputChannels > 1 ? calculateRMS(outputBuffer, 0, numSamples, 1) : leftRMS;
            leftDb = rmsToDb(leftRMS);
            rightDb = rmsToDb(rightRMS);
            haveDB = true;
        }

        if (tpVisible)
        {
            tpDetector.processBlock(outputBuffer, tpPeaks);
            const float tpL = TruePeakDetector::linearToDb(tpPeaks.size() > 0 ? tpPeaks[0] : 0.0f);
            const float tpR = TruePeakDetector::linearToDb(tpPeaks.size() > 1 ? tpPeaks[1] : tpL);

            const float attack = 0.6f, release = 0.2f;
            tpLeftSmooth = (tpL > tpLeftSmooth) ? attack * tpL + (1.0f - attack) * tpLeftSmooth
                : release * tpL + (1.0f - release) * tpLeftSmooth;
            tpRightSmooth = (tpR > tpRightSmooth) ? attack * tpR + (1.0f - attack) * tpRightSmooth
                : release * tpR + (1.0f - release) * tpRightSmooth;

            tpLdb = tpLeftSmooth; tpRdb = tpRightSmooth;
            haveTP = true;
        }

        if (lufsVisible)
        {
            lufsMeter.processBlock(outputBuffer);
            lufsShort = lufsMeter.getShortTermLUFS();
            haveLUFS = true;
        }

        if (!userIsDraggingSlider)
        {
            const double position = transportSource.getCurrentPosition();
            const double duration = transportSource.getLengthInSeconds();
            if (duration > 0.0)
            {
                const double relative = position / duration;
                juce::MessageManager::callAsync([this, relative, position]
                    {
                        positionSlider.setValue(relative, juce::dontSendNotification);
                        timeLabel.setText(formatTime(position), juce::dontSendNotification);
                    });
            }
        }
    }

    if (haveDB || haveTP || haveLUFS)
    {
        //compute a target for the readout on the AUDIO thread
        float targetValForReadout = smoothedMeterValue;
        switch (currentMeterMode)
        {
        case MeterMode::DB:
            if (haveDB)   targetValForReadout = juce::jmax(leftDb, rightDb);
            break;
        case MeterMode::LUFS:
            if (haveLUFS) targetValForReadout = lufsShort;
            break;
        case MeterMode::TP:
            if (haveTP)   targetValForReadout = juce::jmax(tpLdb, tpRdb);
            break;
        }

        //apply exponential smoothing on the AUDIO thread
        const float a = juce::jlimit(0.01f, 0.99f, meterSmoothingAlpha);  // safety clamp
        smoothedMeterValue = a * targetValForReadout + (1.0f - a) * smoothedMeterValue;

        //snap tiny near-zero negatives to exactly 0.0 so you don't see "-0.0"
        float displayVal = smoothedMeterValue;
        if (std::abs(displayVal) < 0.05f) displayVal = 0.0f;

        juce::MessageManager::callAsync([this, haveDB, haveTP, haveLUFS,
            leftDb, rightDb, tpLdb, tpRdb, lufsShort,
            displayVal]  //pass the final value into the lambda
            {
                if (haveDB)
                {
                    leftMeterDisplay.setLevel(leftDb);
                    rightMeterDisplay.setLevel(rightDb);
                }
                if (haveTP)
                {
                    tpLeftMeterDisplay.setLevel(tpLdb);
                    tpRightMeterDisplay.setLevel(tpRdb);
                }
                if (haveLUFS)
                {
                    lufsLeftMeterDisplay.setLevel(lufsShort);
                    lufsRightMeterDisplay.setLevel(lufsShort);
                }

                //update the label with the smoothed, snapped value
                switch (currentMeterMode)
                {
                case MeterMode::DB:
                    meterValueLabel.setText(juce::String(displayVal, 1) + "", juce::dontSendNotification);
                    break;
                case MeterMode::LUFS:
                    meterValueLabel.setText(juce::String(displayVal, 1) + "", juce::dontSendNotification);
                    break;
                case MeterMode::TP:
                    meterValueLabel.setText(juce::String(displayVal, 1) + "", juce::dontSendNotification);
                    break;
                }
            });
    }
}

void MainComponent::audioDeviceStopped()
{
    transportSource.releaseResources();
}

juce::String MainComponent::cleanFileName(const juce::String& filePath)
{
    return juce::File(filePath).getFileNameWithoutExtension();
}

void MainComponent::showSettings(bool show)
{
    showingSettings = show;

    settingsComponent->setVisible(showingSettings);
    //Keep Settings button ALWAYS visible
    settingsButton.setVisible(true);
    settingsComponent->toFront(true);
    settingsButton.toFront(true);

    //Playback UI toggles based on mic + settings
    const bool showPlayback = !useMicInput && !showingSettings;
    openButton.setVisible(showPlayback);
    playButton.setVisible(showPlayback);
    stopButton.setVisible(showPlayback);
    positionSlider.setVisible(showPlayback);
    timeLabel.setVisible(showPlayback);

    appTitleLabel.setVisible(!showingSettings);
    waveformDisplay.setVisible(!showingSettings);
    meterBox.setVisible(!showingSettings);
    visualizerSidebar.setVisible(!showingSettings);
    meterValueLabel.setVisible(!showingSettings);

    //Re-apply mode visibility respecting the settings state
    setMeterMode(currentMeterMode);
    setVisualizerMode(currentVisualizerMode);

    resized();
    repaint();
}

void MainComponent::openButtonClicked()
{
    myFileChooser = std::make_unique<juce::FileChooser>(
        "Choose a WAV or AIFF file",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.wav;*.aiff;*.mp3",
        true, false, this);

    myFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            juce::File file = fc.getResult();
            if (file == juce::File{}) { myFileChooser.reset(); return; }

            if (auto* reader = formatManager.createReaderFor(file))
            {
                currentFileName = cleanFileName(file.getFullPathName());

                transportSource.stop();
                transportSource.setSource(nullptr);
                readerSource.reset();

                readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
                transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);

                // New track => clear visuals and reset analyzers/meters/UI
                clearVisuals();
                resetMetersAndAnalyzers();

                // Don’t auto-start playback; user decides via Play
                DBG("You selected: " + currentFileName);
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon, "Error", "Could not open file.");
            }

            myFileChooser.reset();
        },
        nullptr);
}

void MainComponent::playButtonClicked()
{
    //Same track, new playback pass => clear visuals so traces start fresh.
    clearVisuals();
    transportStateChange(Starting);
}

void MainComponent::stopButtonClicked()
{
    //Don’t clear on stop; let the user see the last frame if they want.
    transportStateChange(Stopping);
}

float MainComponent::calculateRMS(const juce::AudioBuffer<float>& buffer,
    int startSample, int numSamples, int channel)
{
    float sum = 0.0f;
    if (channel < buffer.getNumChannels())
    {
        const float* p = buffer.getReadPointer(channel, startSample);
        for (int i = 0; i < numSamples; ++i) sum += p[i] * p[i];
    }
    return std::sqrt(sum / juce::jmax(1, numSamples));
}

float MainComponent::rmsToDb(float rms)
{
    return (rms > 0.0f) ? 20.0f * std::log10(rms) : -100.0f;
}

void MainComponent::transportStateChange(transportState newstate)
{
    if (newstate == state) return;

    state = newstate;
    if (state == Stopped)
    {
        playButton.setEnabled(true);
        stopButton.setEnabled(false);
        transportSource.setPosition(0.0);
        waveformDisplay.clear(); //keep oscilloscope/spectrum as-is on Stop
    }
    else if (state == Stopping)
    {
        stopButton.setEnabled(false);
        playButton.setEnabled(true);
        transportSource.stop();
    }
    else if (state == Starting)
    {
        stopButton.setEnabled(true);
        playButton.setEnabled(false);
        transportSource.start();
    }
}

juce::String formatTime(double seconds)
{
    int totalSeconds = (int)seconds;
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    return juce::String::formatted("%02d:%02d", minutes, secs);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::lightslategrey);
}

void MainComponent::resized()
{
    const int sidebarWidth = 120;
    const int padding = 10;
    const int buttonSize = 40;
    const int micButtonWidth = 50;
    const int topBarHeight = 40;
    const int bottomY = getHeight() - buttonSize - padding;

    //mic toggle
    micButton.setBounds(getWidth() - 95, bottomY + 8, micButtonWidth, 24);

    //Transport buttons
    openButton.setBounds(padding, bottomY, buttonSize, buttonSize);
    playButton.setBounds(padding + buttonSize + 8, bottomY, buttonSize, buttonSize);
    stopButton.setBounds(padding + (buttonSize + 8) * 2, bottomY, buttonSize, buttonSize);

    //Always keep settings button visible and reachable
    settingsButton.setBounds(getWidth() - 55, bottomY, 50, 40);

    //Sidebar
    visualizerSidebar.setBounds(getWidth() - sidebarWidth, 10, sidebarWidth - 10, 115);
    spectrumButton.setBounds(getWidth() - sidebarWidth + 10, 30, sidebarWidth - 30, 20);
    oscilloscopeButton.setBounds(getWidth() - sidebarWidth + 10, 60, sidebarWidth - 30, 20);
    stereoImageButton.setBounds(getWidth() - sidebarWidth + 10, 90, sidebarWidth - 30, 20);

    const int meterBoxTop = visualizerSidebar.getBottom() + 10;
    meterBox.setBounds(getWidth() - sidebarWidth, meterBoxTop - 10, sidebarWidth - 10, 170);

    //Main content
    const int contentX = padding;
    const int contentRight = getWidth() - sidebarWidth - padding;
    const int contentWidth = contentRight - contentX;

    //Seekbar + timestamp
    const int sliderX = padding + (buttonSize + 8) * 3 + 8;
    const int sliderWid = contentWidth - (sliderX - contentX);
    positionSlider.setBounds(sliderX, bottomY + 10, sliderWid, 20);
    timeLabel.setBounds(sliderX + sliderWid - 5, bottomY + 10, 50, 20);

    //Visualizers (stacked; we show one at a time)
    oscilloscopeDisplay.setBounds(contentX + 5, topBarHeight - 20, contentWidth - 10, 200);
    spectrumDisplay.setBounds(contentX + 5, topBarHeight - 20, contentWidth - 10, 200);
    stereoImageDisplay.setBounds(contentX + 5, topBarHeight - 20, contentWidth - 10, 200);

    //Waveform
    const int waveformHeight = 40;
    const int waveformTop = oscilloscopeDisplay.getBottom() + 10;
    const int waveformRight = meterBox.getX() - 10;
    const int waveformWidth = waveformRight - padding;
    waveformDisplay.setBounds(padding + 5, waveformTop + 2, waveformWidth - 10, waveformHeight + 20);

    //Meter widgets
    const auto meterBoxBounds = meterBox.getBounds();
    const int meterWidth = 20;
    const int meterHeight = meterBoxBounds.getHeight() - 40;
    int meterX = meterBoxBounds.getCentreX() - 42;
    int meterY = meterBoxBounds.getY() + 23;

    leftMeterDisplay.setBounds(meterX, meterY, meterWidth, meterHeight);
    rightMeterDisplay.setBounds(meterX + 25, meterY, meterWidth, meterHeight);

    lufsLeftMeterDisplay.setBounds(meterX, meterY, meterWidth, meterHeight);
    lufsRightMeterDisplay.setBounds(meterX + 25, meterY, meterWidth, meterHeight);

    tpLeftMeterDisplay.setBounds(meterX, meterY, meterWidth, meterHeight);
    tpRightMeterDisplay.setBounds(meterX + 25, meterY, meterWidth, meterHeight);

    //Meter mode buttons
    const int buttonWidth = 40;
    const int buttonHeight = 16;
    const int buttonCount = 3;
    const int spacingBetweenButtons = 10;
    const int totalButtonsHeight = (buttonCount * buttonHeight) + (spacingBetweenButtons * (buttonCount - 1));
    const int meterBoxMidX = meterBoxBounds.getCentreX() + 7;
    const int startY = meterBoxBounds.getCentreY() - (totalButtonsHeight / 2);

    dbButton.setBounds(meterBoxMidX, startY, buttonWidth, buttonHeight);
    lufsButton.setBounds(meterBoxMidX, startY + (buttonHeight + spacingBetweenButtons), buttonWidth, buttonHeight);
    tpButton.setBounds(meterBoxMidX, startY + 2 * (buttonHeight + spacingBetweenButtons), buttonWidth, buttonHeight);

    const int valueLabelTop = tpButton.getBottom() + 6;
    meterValueLabel.setBounds(meterBox.getX() + 36,
        valueLabelTop,
        meterBox.getWidth() - 20,
        18);
}

