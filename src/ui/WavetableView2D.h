#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/Wavetable.h"

// Simple 2D preview of the frame currently selected by the WT position.
// The 3D OpenGL view (Phase 4) replaces this on the main panel; this stays
// as the lightweight fallback.
class WavetableView2D final : public juce::Component
{
public:
    void setTable (const wf::Wavetable* t)
    {
        table = t;
        repaint();
    }

    void setPosition (float pos01)
    {
        if (std::abs (pos01 - position) > 1.0e-4f)
        {
            position = pos01;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff15161b));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (0xff2a2c36));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        if (table == nullptr)
            return;

        const int frames = table->getNumFrames();
        const float p = position * (float) juce::jmax (0, frames - 1);
        const int f0 = (int) p;
        const int f1 = juce::jmin (f0 + 1, frames - 1);
        const float mix = p - (float) f0;
        const float* a = table->getRawFrame (f0);
        const float* b = table->getRawFrame (f1);

        juce::Path path;
        const int N = wf::Wavetable::frameSize;
        const int step = juce::jmax (1, N / juce::jmax (64, (int) r.getWidth()));
        for (int i = 0; i < N; i += step)
        {
            const float v = a[i] + (b[i] - a[i]) * mix;
            const float x = r.getX() + r.getWidth() * (float) i / (float) N;
            const float y = r.getCentreY() - v * r.getHeight() * 0.45f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff4fc3f7));
        g.strokePath (path, juce::PathStrokeType (1.6f));

        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (12.0f);
        g.drawText (table->getName() + "  (" + juce::String (frames) + " frames, pos " + juce::String (f0 + 1) + ")",
                    getLocalBounds().reduced (6), juce::Justification::topLeft);
    }

private:
    const wf::Wavetable* table = nullptr;
    float position = 0.0f;
};
