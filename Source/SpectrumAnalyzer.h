#pragma once
#include <JuceHeader.h>

// Real-time spectrum analyzer (single trace) with overlap + smoothing
class SpectrumAnalyzer : public juce::Component
{
public:
    // fftOrder = 12 -> 4096 (nice resolution). You can try 13 (8192) if CPU allows.
    explicit SpectrumAnalyzer(int fftOrder = 12)
        : order(fftOrder),
        fftSize(1 << order),
        hopSize(fftSize / 4), // 4x overlap
        window(fftSize, juce::dsp::WindowingFunction<float>::hann, true /*normalise*/),
        fft(order),
        fifo(fftSize, 0.0f),
        magDb(fftSize / 2, -120.0f),
        magDbEma(fftSize / 2, -120.0f)
    {
        setOpaque(true);
    }

    // ---- Configuration API ----
    void setDbRange(float minDbIn, float maxDbIn) { minDb = minDbIn; maxDb = maxDbIn; repaint(); }
    void setFreqRange(float minHz, float maxHz) { minFreq = minHz; maxFreq = maxHz; repaint(); }
    void setSampleRate(double sr) { sampleRate = (sr > 0.0 ? sr : 44100.0); }
    // Smoothing: timeAlpha in [0..1], higher = snappier; freqSmoothRadius = 0..N (bins)
    void setSmoothing(float timeAlphaIn, int freqSmoothRadiusIn)
    {
        timeAlpha = juce::jlimit(0.0f, 1.0f, timeAlphaIn);
        freqSmoothRadius = juce::jmax(0, freqSmoothRadiusIn);
    }

    // ---- Data feed ----
    void pushSamples(const juce::AudioBuffer<float>& buffer)
    {
        const int numCh = juce::jmax(1, buffer.getNumChannels());
        const int numSmps = buffer.getNumSamples();

        for (int i = 0; i < numSmps; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) s += buffer.getSample(ch, i);
            s *= (1.0f / (float)numCh);

            ring.push_back(s);
            // When we have >= hopSize new samples beyond the last frame, do a new FFT frame
            if ((int)ring.size() >= (int)fifo.size() + hopSize)
            {
                // Copy the last fftSize samples into fifo (with 4x overlap)
                std::copy(ring.end() - fftSize, ring.end(), fifo.begin());
                computeSpectrum();
                // Keep only the tail we need for the next overlap frame
                ring.erase(ring.begin(), ring.end() - (int)fifo.size() + hopSize);
            }
        }
    }

    void clear()
    {
        std::fill(magDb.begin(), magDb.end(), minDb);
        std::fill(magDbEma.begin(), magDbEma.end(), minDb);
        magDbSmoothed.clear();                // ensure no leftover smoothed trace
        ring.clear();                         // drop any queued audio
        repaint();
    }

    // ---- Component ----
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::lightgrey);

        auto r = getLocalBounds().toFloat().reduced(1.0f, 2.0f); // avoid visual clipping at edges
        drawGrid(g, r);

        g.setColour(juce::Colours::lightslategrey);
        juce::Path p = makeSpectrumPath(r, magDbSmoothed.empty() ? magDbEma : magDbSmoothed);
        g.strokePath(p, juce::PathStrokeType(1.6f));

        g.setColour(juce::Colours::lightslategrey);
        g.drawRect(getLocalBounds());
    }

    void resized() override {}

private:
    // ====== FFT & data ======
    const int order;
    const int fftSize;
    const int hopSize;

    juce::dsp::WindowingFunction<float> window;
    juce::dsp::FFT fft;

    std::vector<float> fifo;          // time domain window (fftSize)
    std::vector<float> ring;          // rolling buffer for overlap
    std::vector<float> fftBuffer;     // 2 * fftSize (complex)
    std::vector<float> magDb;         // per-bin dB (instant)
    std::vector<float> magDbEma;      // per-bin dB (time-smoothed)
    std::vector<float> magDbSmoothed; // after freq smoothing (optional)

    // ====== Display params ======
    float  minDb = -90.0f;
    float  maxDb = 6.0f;           // allow headroom above 0 dB to avoid top flattening
    float  minFreq = 20.0f;
    float  maxFreq = 20000.0f;
    double sampleRate = 44100.0;

    // Smoothing
    float timeAlpha = 0.25f;         // 0..1 (higher = faster response)
    int   freqSmoothRadius = 1;      // bins to each side (0 disables)

    void computeSpectrum()
    {
        if ((int)fftBuffer.size() < 2 * fftSize) fftBuffer.resize(2 * fftSize, 0.0f);
        std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);

        // Copy + window
        std::vector<float> temp(fifo.begin(), fifo.end());
        window.multiplyWithWindowingTable(temp.data(), fftSize);
        std::copy(temp.begin(), temp.end(), fftBuffer.begin());

        // FFT
        fft.performRealOnlyForwardTransform(fftBuffer.data());

        // Magnitude (single-sided) + normalization
        const float singleSided = 2.0f / (float)fftSize;
        constexpr float eps = 1.0e-12f;

        for (int bin = 0; bin < fftSize / 2; ++bin)
        {
            const float re = fftBuffer[bin];
            const float im = fftBuffer[fftSize + bin];
            float lin = std::sqrt(re * re + im * im) * singleSided;

            float dB = 20.0f * std::log10(lin + eps);

            // Keep a tiny headroom so the line doesn't hit the very top pixel
            constexpr float headroom = 0.8f; // dB
            dB = juce::jmin(dB, maxDb - headroom);
            dB = juce::jlimit(minDb, maxDb, dB);

            magDb[bin] = dB;
        }

        // Temporal smoothing (EMA per bin)
        for (int bin = 0; bin < fftSize / 2; ++bin)
            magDbEma[bin] = timeAlpha * magDb[bin] + (1.0f - timeAlpha) * magDbEma[bin];

        // Optional frequency smoothing (simple weighted moving average)
        if (freqSmoothRadius > 0)
        {
            if ((int)magDbSmoothed.size() != fftSize / 2)
                magDbSmoothed.assign(fftSize / 2, minDb);

            const int N = (int)magDbEma.size();
            for (int i = 0; i < N; ++i)
            {
                float wsum = 0.0f, vsum = 0.0f;
                for (int k = -freqSmoothRadius; k <= freqSmoothRadius; ++k)
                {
                    const int j = juce::jlimit(0, N - 1, i + k);
                    // triangular weights (1,2,3,2,1) when radius=2, etc.
                    const float w = (float)(freqSmoothRadius + 1 - std::abs(k));
                    wsum += w;
                    vsum += w * magDbEma[j];
                }
                magDbSmoothed[i] = vsum / juce::jmax(1.0f, wsum);
            }
        }
        else
        {
            magDbSmoothed.clear();
        }

        juce::MessageManager::callAsync([this]() { repaint(); });
    }

    // ====== Rendering helpers ======
    float xForFreq(float f, juce::Rectangle<float> r) const
    {
        f = juce::jlimit(minFreq, maxFreq, f);
        const float norm = (std::log10(f) - std::log10(minFreq))
            / (std::log10(maxFreq) - std::log10(minFreq));
        return r.getX() + norm * r.getWidth();
    }

    float yForDb(float dB, juce::Rectangle<float> r) const
    {
        // Top = maxDb
        const float t = (dB - maxDb) / (minDb - maxDb);
        const float y = r.getY() + juce::jlimit(0.0f, 1.0f, t) * r.getHeight();
        return juce::jlimit(r.getY(), r.getBottom() - 1.0f, y);
    }

    juce::Path makeSpectrumPath(juce::Rectangle<float> r, const std::vector<float>& dBvals) const
    {
        juce::Path p;
        if (dBvals.empty()) return p;

        // bin frequency resolution
        const float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

        // pick first/last bins that fall inside your chosen freq range, skip DC
        const int firstBin = juce::jmax(1, (int)std::ceil(minFreq / binHz));
        const int lastBin = juce::jmin((int)std::floor(maxFreq / binHz), fftSize / 2 - 1);
        if (firstBin >= lastBin) return p;

        // --- Start EXACTLY at the left edge (removes the tiny gap) ---
        // Use the Y from the first bin so there’s no vertical jump.
        const float xLeft = r.getX();                      // left pixel of the plot rect
        const float yFirst = yForDb(dBvals[firstBin], r);
        p.startNewSubPath(xLeft, yFirst);

        // Draw the rest of the spectrum using bin *center* frequencies
        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            const float fCenter = (bin + 0.5f) * binHz;     // center of this FFT bin
            const float x = xForFreq(fCenter, r);
            const float y = yForDb(dBvals[bin], r);
            p.lineTo(x, y);
        }

        return p;
    }

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> r) const
    {
        g.setColour(juce::Colours::darkgrey.withAlpha(0.25f));

        // Horizontal dB lines every 12 dB
        for (float d = maxDb; d >= minDb; d -= 12.0f)
        {
            const float y = yForDb(d, r);
            g.drawHorizontalLine((int)std::round(y), r.getX(), r.getRight());
        }

        // Vertical freq lines
        const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (float f : freqs)
        {
            if (f < minFreq || f > maxFreq) continue;
            const float x = xForFreq(f, r);
            g.drawVerticalLine((int)std::round(x), r.getY(), r.getBottom());
        }
    }
};
