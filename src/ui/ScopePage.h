#pragma once

#include "LineStack3DView.h"
#include "Scope.h"
#include <juce_dsp/juce_dsp.h>

// Output analysis page: 3D spectrogram waterfall, 2D spectrum with peak
// hold, and a large waveform scope. One timer (30 fps) does a single FFT per
// tick and feeds all views; nothing runs while the page is hidden.
namespace ui
{

class Spectrogram3DView final : public LineStack3DView
{
public:
    static constexpr int numRows = 64, pointsPerRow = 256;

    Spectrogram3DView() : LineStack3DView (colours::accentFx)
    {
        rows.assign ((size_t) numRows * pointsPerRow, 0.0f);
        resetCamera (0.7f, -0.35f, 1.0f);
    }

    // Message thread: newest row (0..1 magnitudes, pointsPerRow values).
    void pushRow (const float* row)
    {
        const juce::ScopedLock sl (lock);
        writeIndex = (writeIndex + 1) % numRows;
        std::copy_n (row, pointsPerRow, rows.begin() + (size_t) writeIndex * pointsPerRow);
    }

private:
    bool refillEveryFrame() const override { return true; }

    bool buildVertices (std::vector<float>& verts, int& outLines, int& outPoints) override
    {
        outLines = numRows;
        outPoints = pointsPerRow;
        verts.resize ((size_t) numRows * pointsPerRow * 3);
        const juce::ScopedLock sl (lock);
        size_t k = 0;
        for (int line = 0; line < numRows; ++line)          // line 0 = newest, at the front
        {
            const int r = (writeIndex - line + numRows) % numRows;
            const float* row = rows.data() + (size_t) r * pointsPerRow;
            const float z = -1.0f + 2.4f * (float) line / (float) (numRows - 1);
            for (int i = 0; i < pointsPerRow; ++i)
            {
                verts[k++] = -1.2f + 2.4f * (float) i / (float) (pointsPerRow - 1);
                verts[k++] = -0.5f + row[i] * 1.1f;
                verts[k++] = z;
            }
        }
        return true;
    }

    juce::Colour lineColour (int line, int lines) const override
    {
        const float t = (float) line / (float) juce::jmax (1, lines - 1);   // 0 newest
        return accent.interpolatedWith (colours::accentMod, t).withAlpha (0.9f - 0.6f * t);
    }

    float lineWidthFor (int line) const override { return line == 0 ? 2.0f : 1.0f; }

    void paintOverlay (juce::Graphics& g) override
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("SPECTROGRAM   20 Hz - 20 kHz (log)   newest in front", getLocalBounds().reduced (8, 5),
                    juce::Justification::topLeft);
    }

    juce::CriticalSection lock;
    std::vector<float> rows;
    int writeIndex = 0;
};

class SpectrumView final : public juce::Component
{
public:
    static constexpr int points = Spectrogram3DView::pointsPerRow;

    SpectrumView() { setOpaque (true); }

    void setRow (const float* row)
    {
        for (int i = 0; i < points; ++i)
        {
            current[(size_t) i] = row[i];
            peak[(size_t) i] = juce::jmax (row[i], peak[(size_t) i] * 0.97f);   // slow peak decay
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (6.0f, 18.0f);
        g.fillAll (colours::widget);

        g.setColour (colours::panelEdge);
        for (float db = -20.0f; db > -90.0f; db -= 20.0f)
        {
            const float y = r.getBottom() - r.getHeight() * (db + 90.0f) / 90.0f;
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }
        const float labels[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
        g.setFont (juce::FontOptions (10.0f));
        for (float f : labels)
        {
            const float x = r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
            g.setColour (colours::panelEdge);
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            g.setColour (colours::textDim);
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        (int) x - 16, (int) r.getBottom() + 2, 32, 12, juce::Justification::centred);
        }

        auto curve = [&] (const std::array<float, points>& data)
        {
            juce::Path p;
            for (int i = 0; i < points; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (float) (points - 1);
                const float y = r.getBottom() - juce::jlimit (0.0f, 1.0f, data[(size_t) i]) * r.getHeight();
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };
        g.setColour (colours::accentMod.withAlpha (0.5f));
        g.strokePath (curve (peak), juce::PathStrokeType (1.0f));
        auto fill = curve (current);
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        g.setColour (colours::accentFx.withAlpha (0.25f));
        g.fillPath (fill);
        g.setColour (colours::accentFx);
        g.strokePath (curve (current), juce::PathStrokeType (1.5f));

        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("SPECTRUM   0 .. -90 dB", getLocalBounds().reduced (8, 5), juce::Justification::topLeft);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    }

private:
    std::array<float, points> current {}, peak {};
};

class ScopePage final : public juce::Component, private juce::Timer
{
public:
    ScopePage (const ScopeBuffer& source, std::function<double()> sampleRateFn)
        : buffer (source), getSampleRate (std::move (sampleRateFn)), scope (source)
    {
        addAndMakeVisible (spectrogram);
        addAndMakeVisible (spectrum);
        addAndMakeVisible (scope);
        window.resize ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)   // Hann
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
        startTimerHz (30);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto bottom = r.removeFromBottom (r.getHeight() * 2 / 5);
        r.removeFromBottom (6);
        spectrogram.setBounds (r);
        scope.setBounds (bottom.removeFromRight (bottom.getWidth() / 3));
        bottom.removeFromRight (6);
        spectrum.setBounds (bottom);
    }

private:
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;

    void timerCallback() override
    {
        if (! isShowing())
            return;

        buffer.copyLatest (samples.data(), fftSize);
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = samples[(size_t) i] * window[(size_t) i];
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        // log-spaced 20 Hz .. 20 kHz -> 0..1 from -90..0 dBFS (Hann + full-scale sine ~ N/4)
        const double sr = juce::jmax (8000.0, getSampleRate());
        const float fullScale = (float) fftSize * 0.25f;
        for (int i = 0; i < Spectrogram3DView::pointsPerRow; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / (double) (Spectrogram3DView::pointsPerRow - 1));
            const double bin = f * fftSize / sr;
            const int b0 = juce::jlimit (0, fftSize / 2 - 1, (int) bin);
            const int b1 = juce::jmin (fftSize / 2 - 1, b0 + 1);
            const float t = (float) (bin - b0);
            const float mag = fftData[(size_t) b0] + (fftData[(size_t) b1] - fftData[(size_t) b0]) * t;
            const float db = juce::Decibels::gainToDecibels (mag / fullScale, -100.0f);
            row[(size_t) i] = juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f);
        }
        spectrogram.pushRow (row.data());
        spectrum.setRow (row.data());
    }

    const ScopeBuffer& buffer;
    std::function<double()> getSampleRate;
    Spectrogram3DView spectrogram;
    SpectrumView spectrum;
    Scope scope;

    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window;
    std::array<float, fftSize> samples {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, Spectrogram3DView::pointsPerRow> row {};
};

} // namespace ui
