#pragma once

#include "LineStack3DView.h"
#include "../Params.h"
#include "../dsp/Filter.h"

namespace ui
{

// Filter response display.
//   3D  a sheet of magnitude responses for cutoffs from 20 Hz to 20 kHz
//       (front = lowest), the response at the current cutoff on top in white
//   2D  the current response over a log frequency axis
// The curves are the analog 2-pole prototype behind the TPT SVF (12 dB) or
// its square (24 dB); this matches the DSP closely below ~Nyquist/4 and is
// only a display, so no per-sample filter is run here.
class FilterView final : public LineStack3DView
{
public:
    enum Mode { mode3D = 0, mode2D };
    static constexpr int points = 256, cutoffLines = 64;
    static constexpr float minHz = 20.0f, maxHz = 20000.0f, minDb = -60.0f, maxDb = 24.0f;

    explicit FilterView (juce::AudioProcessorValueTreeState& apvts)
        : LineStack3DView (colours::accent)
    {
        typeParam = apvts.getRawParameterValue (ParamID::filterType);
        cutoffParam = apvts.getRawParameterValue (ParamID::filterCutoff);
        resoParam = apvts.getRawParameterValue (ParamID::filterResonance);
        enabledParam = apvts.getRawParameterValue (ParamID::filterEnabled);
        resetCamera (0.45f, -0.7f, 1.0f);
        timerTick();
    }

    void setMode (Mode m)
    {
        mode = m;
        setGLRenderingEnabled (m == mode3D);
        repaint();
    }

private:
    // dB magnitude of the response at frequency f for the given settings
    static float responseDb (int type, float cutoffHz, float reso01, float f) noexcept
    {
        const float q = 0.5f + reso01 * reso01 * 9.5f;
        const float w = f / juce::jmax (1.0f, cutoffHz);
        const float w2 = w * w;
        const float dRe = 1.0f - w2, dIm = w / q;
        const float dMag2 = dRe * dRe + dIm * dIm;
        float mag2;
        switch (type)
        {
            case wf::VoiceFilter::hp12: case wf::VoiceFilter::hp24: mag2 = (w2 * w2) / dMag2; break;
            case wf::VoiceFilter::bp12:                              mag2 = (dIm * dIm) / dMag2; break;
            default:                                                 mag2 = 1.0f / dMag2; break;
        }
        float db = 10.0f * std::log10 (juce::jmax (1.0e-12f, mag2));
        if (type == wf::VoiceFilter::lp24 || type == wf::VoiceFilter::hp24)
            db *= 2.0f;
        return juce::jlimit (minDb, maxDb, db);
    }

    static float freqAt (int i) noexcept
    {
        return minHz * std::pow (maxHz / minHz, (float) i / (float) (points - 1));
    }
    static float dbToY (float db) noexcept { return -0.5f + 1.0f * (db - minDb) / (maxDb - minDb); }

    void timerTick() override
    {
        const int t = (int) std::lround (typeParam->load());
        const float c = cutoffParam->load(), r = resoParam->load();
        const bool on = enabledParam->load() >= 0.5f;
        if (t != type || std::abs (r - reso) > 1.0e-4f || on != enabled)
        {
            type = t; reso = r; enabled = on;
            requestVertexRefill();
            repaint();
        }
        if (std::abs (c - cutoff) > 0.01f)
        {
            cutoff = c;
            if (mode == mode2D) repaint();
        }
    }

    bool buildVertices (std::vector<float>& verts, int& outLines, int& outPoints) override
    {
        outLines = cutoffLines;
        outPoints = points;
        verts.resize ((size_t) outLines * points * 3);
        size_t k = 0;
        for (int line = 0; line < outLines; ++line)
        {
            const float pos = (float) line / (float) (outLines - 1);
            const float fc = minHz * std::pow (maxHz / minHz, pos);
            for (int i = 0; i < points; ++i)
            {
                verts[k++] = -1.0f + 2.0f * (float) i / (float) (points - 1);
                verts[k++] = enabled ? dbToY (responseDb (type, fc, reso, freqAt (i))) : dbToY (0.0f);
                verts[k++] = -1.0f + 2.0f * pos;
            }
        }
        return true;
    }

    bool buildOverlayLine (std::vector<float>& verts, int& pts) override
    {
        pts = points;
        verts.resize ((size_t) points * 3);
        const float pos = std::log (juce::jlimit (minHz, maxHz, cutoff) / minHz) / std::log (maxHz / minHz);
        size_t k = 0;
        for (int i = 0; i < points; ++i)
        {
            verts[k++] = -1.0f + 2.0f * (float) i / (float) (points - 1);
            verts[k++] = enabled ? dbToY (responseDb (type, cutoff, reso, freqAt (i))) : dbToY (0.0f);
            verts[k++] = -1.0f + 2.0f * pos;
        }
        return true;
    }

    juce::Colour lineColour (int line, int lines) const override
    {
        const float t = (float) line / (float) juce::jmax (1, lines - 1);
        return accent.interpolatedWith (colours::accentMod, t).withAlpha (0.45f);
    }

    void paintFallback (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (6.0f, 18.0f);
        g.setColour (colours::panelEdge);
        for (float db = 0.0f; db > minDb; db -= 12.0f)
        {
            const float y = r.getBottom() - r.getHeight() * (db - minDb) / (maxDb - minDb);
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }
        const float labels[] = { 100.0f, 1000.0f, 10000.0f };
        g.setFont (juce::FontOptions (10.0f));
        for (float f : labels)
        {
            const float x = r.getX() + r.getWidth() * std::log (f / minHz) / std::log (maxHz / minHz);
            g.setColour (colours::panelEdge);
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            g.setColour (colours::textDim);
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        (int) x - 14, (int) r.getBottom() + 2, 28, 12, juce::Justification::centred);
        }

        juce::Path p;
        for (int i = 0; i < points; ++i)
        {
            const float db = enabled ? responseDb (type, cutoff, reso, freqAt (i)) : 0.0f;
            const float x = r.getX() + r.getWidth() * (float) i / (float) (points - 1);
            const float y = r.getBottom() - r.getHeight() * (db - minDb) / (maxDb - minDb);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        auto fill = p;
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        g.setColour (accent.withAlpha (0.18f));
        g.fillPath (fill);
        g.setColour (accent);
        g.strokePath (p, juce::PathStrokeType (1.8f));
    }

    void paintOverlay (juce::Graphics& g) override
    {
        g.setColour (colours::text.withAlpha (0.75f));
        g.setFont (juce::FontOptions (11.0f));
        const juce::String names[] = { "LP 12", "LP 24", "HP 12", "HP 24", "BP 12" };
        g.drawText ((enabled ? names[juce::jlimit (0, 4, type)] : juce::String ("bypassed"))
                        + "   " + (cutoff >= 1000.0f ? juce::String (cutoff / 1000.0f, 2) + " kHz" : juce::String ((int) cutoff) + " Hz")
                        + (mode == mode3D ? "   (depth = cutoff)" : ""),
                    getLocalBounds().reduced (8, 5), juce::Justification::topLeft);
    }

    std::atomic<float>* typeParam = nullptr;
    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* resoParam = nullptr;
    std::atomic<float>* enabledParam = nullptr;
    int type = 1;
    float cutoff = 20000.0f, reso = 0.0f;
    bool enabled = true;
    Mode mode = mode3D;
};

} // namespace ui
