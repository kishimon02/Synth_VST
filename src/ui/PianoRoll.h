#pragma once

#include "LookAndFeel.h"
#include "../ai/MusicContext.h"

// Piano-roll drawing shared by the AI result cards and the chord page.
namespace ui
{

inline const char* gmDrumName (int note)
{
    switch (note)
    {
        case 35: case 36: return "Kick";
        case 37: return "Rim";  case 38: case 40: return "Snare"; case 39: return "Clap";
        case 42: return "CHH";  case 44: return "PHH"; case 46: return "OHH";
        case 41: case 43: case 45: case 47: case 48: case 50: return "Tom";
        case 49: case 57: return "Crash"; case 51: case 59: return "Ride"; case 53: return "Bell";
        default: return nullptr;
    }
}

inline juce::String midiNoteName (int note)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[note % 12]) + juce::String (note / 12 - 1);
}

// Background, grid, pitch labels and the notes. Returns the number of bars drawn.
inline int drawPianoRoll (juce::Graphics& g, juce::Rectangle<float> r, const std::vector<ai::Note>& notes,
                          int beatsPerBar, bool drums, juce::Colour colour)
{
    g.setColour (colours::widget);
    g.fillRoundedRectangle (r, 6.0f);
    auto plot = r.reduced (6.0f, 4.0f).withTrimmedLeft (36.0f);

    int lo = 127, hi = 0;
    float endBeat = 1.0f;
    for (const auto& n : notes)
    {
        lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch);
        endBeat = juce::jmax (endBeat, n.startBeat + n.durationBeats);
    }
    if (notes.empty())
    {
        lo = 60; hi = 72;
    }
    beatsPerBar = juce::jmax (1, beatsPerBar);
    const float bars = std::ceil (endBeat / (float) beatsPerBar);
    const float totalBeats = juce::jmax (1.0f, bars * (float) beatsPerBar);
    lo = juce::jmax (0, lo - 1); hi = juce::jmin (127, hi + 1);
    const float rowH = plot.getHeight() / (float) (hi - lo + 1);

    for (int b = 0; b <= (int) totalBeats; ++b)
    {
        const float x = plot.getX() + plot.getWidth() * (float) b / totalBeats;
        g.setColour (b % beatsPerBar == 0 ? colours::textDim : colours::panelEdge);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
    g.setFont (juce::FontOptions (9.5f));
    for (int pitch = lo; pitch <= hi; ++pitch)
    {
        const float y = plot.getBottom() - rowH * (float) (pitch - lo + 1);
        if (drums)
        {
            if (auto* name = gmDrumName (pitch))
            {
                g.setColour (colours::textDim);
                g.drawText (name, (int) r.getX() + 4, (int) y, 34, (int) rowH + 1, juce::Justification::centredLeft);
            }
        }
        else if (pitch % 12 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (midiNoteName (pitch), (int) r.getX() + 4, (int) y, 34, (int) rowH + 1, juce::Justification::centredLeft);
            g.setColour (colours::panelEdge);
            g.drawHorizontalLine ((int) (y + rowH), plot.getX(), plot.getRight());
        }
    }
    for (const auto& n : notes)
    {
        const float x = plot.getX() + plot.getWidth() * n.startBeat / totalBeats;
        const float w = juce::jmax (2.0f, plot.getWidth() * n.durationBeats / totalBeats - 1.0f);
        const float y = plot.getBottom() - rowH * (float) (n.pitch - lo + 1);
        g.setColour (colour.withAlpha (0.45f + 0.55f * (float) n.velocity / 127.0f));
        g.fillRoundedRectangle (x, y + 1.0f, w, juce::jmax (2.0f, rowH - 2.0f), 2.0f);
    }
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    return (int) bars;
}

} // namespace ui
