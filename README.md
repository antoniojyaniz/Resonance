# Resonance 

A real-time audio visualization and metering app built with JUCE. It lets you analyze live or recorded audio through multiple synchronized visualizers — including a spectrum analyzer, oscilloscope, stereo image display, waveform viewer, and LUFS/dB/True Peak meters.

## Demo 

## How It’s Made

This project was built using the JUCE audio application framework. The app captures either microphone input or audio playback, processes it through FFT and RMS algorithms, and drives five different real-time visualizers.

Each visualizer is modular and self-contained:

  - Spectrum Analyzer: performs FFT analysis to display frequency amplitude in real time.

  - Oscilloscope: renders the time-domain waveform of the current audio buffer.

  - Stereo Image Display: shows phase and correlation between left and right channels.

  - Waveform Display: draws the full audio file for playback navigation.

  - Meters: dB, LUFS, and True Peak readings with a smoothed numeric value beneath the selected mode.

The layout logic, button controls, and visualization toggles are all handled in the MainComponent, keeping everything flexible and reactive. The color scheme matches the app’s sleek, light-to-slate grey theme for a professional audio-engineering look.

## Optimizations 

While building, I focused heavily on performance and responsiveness:

  - Implemented lightweight smoothing for meter values to reduce flicker without lag.

  - Reused FFT buffers and minimized allocations per frame.

  - Used juce::MessageManager::callAsync to ensure smooth UI updates without blocking the audio thread.

  - Optimized layout calculations to scale cleanly with window size and resolution.

These optimizations let the app render multiple meters and visualizers simultaneously while maintaining a solid frame rate and minimal CPU load.

## Lessons Learned

This project deepened my understanding of real-time audio processing and multithreaded UI design.
I learned how to:

  - Balance audio-thread safety with graphical performance.

  - Implement smoothing filters for more natural data display.

  - Manage multiple custom JUCE components cohesively within one application.

Building this helped me appreciate how low-level signal handling translates into intuitive, visual feedback — something every audio developer or musician loves seeing in action.
