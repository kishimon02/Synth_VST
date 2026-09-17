#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include <array>
#include <atomic>

namespace ui
{

// Single-writer (audio thread) / single-reader (UI timer) sample ring for
// the output scope. No locks; a torn read only ever shows a slightly stale
// waveform, which does not matter for a display.
class ScopeBuffer
{
public:
    static constexpr int size = 8192;   // power of two

    void push (const float* l, const float* r, int n) noexcept
    {
        int w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            data[(size_t) w] = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
            w = (w + 1) & (size - 1);
        }
        writePos.store (w, std::memory_order_release);
    }

    // Copies the most recent `n` samples (oldest first).
    void copyLatest (float* out, int n) const noexcept
    {
        const int w = writePos.load (std::memory_order_acquire);
        int start = (w - n) & (size - 1);
        for (int i = 0; i < n; ++i)
            out[i] = data[(size_t) ((start + i) & (size - 1))];
    }

private:
    std::array<float, size> data {};
    std::atomic<int> writePos { 0 };
};

// Output oscilloscope with a simple rising-zero-crossing trigger.
class Scope final : public juce::Component, private juce::Timer
{
public:
    explicit Scope (const ScopeBuffer& source) : buffer (source)
    {
        setOpaque (true);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.fillAll (colours::widget);
        g.setColour (colours::panelEdge);
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        // trigger: first rising zero crossing in the first half of the capture
        int start = 0;
        for (int i = 1; i < captureSize / 2; ++i)
            if (samples[(size_t) i - 1] <= 0.0f && samples[(size_t) i] > 0.0f) { start = i; break; }

        const int shown = captureSize / 2;
        juce::Path p;
        for (int i = 0; i < shown; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (shown - 1);
            const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, samples[(size_t) (start + i)]) * r.getHeight() * 0.46f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (colours::accentFx);
        g.strokePath (p, juce::PathStrokeType (1.4f));

        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("OUT", getLocalBounds().reduced (6, 3), juce::Justification::topLeft);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

private:
    static constexpr int captureSize = 2048;

    void timerCallback() override
    {
        if (! isShowing())
            return;
        buffer.copyLatest (samples.data(), captureSize);
        repaint();
    }

    const ScopeBuffer& buffer;
    std::array<float, captureSize> samples {};
};

} // namespace ui
