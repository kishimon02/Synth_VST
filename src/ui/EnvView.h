#pragma once

#include "Controls.h"
#include "../dsp/Envelope.h"
#include "../dsp/SynthEngine.h"

namespace ui
{

// ADSR curve display, Serum style: the exact curve the voices use (the real
// wf::Envelope is simulated at 1 kHz to draw it), three draggable handles
// (attack end, decay end / sustain level, release end), and a moving dot
// that follows the newest voice while it plays.
class EnvView final : public juce::Component, private juce::Timer
{
public:
    EnvView (Apvts& apvts, int envIndex, const wf::EnvDisplay& liveIn, juce::Colour accentIn)
        : index (envIndex), live (liveIn), accent (accentIn)
    {
        const auto ids = ParamID::env (envIndex);
        attack  = apvts.getParameter (ids.attack);
        decay   = apvts.getParameter (ids.decay);
        sustain = apvts.getParameter (ids.sustain);
        release = apvts.getParameter (ids.release);
        setOpaque (true);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        g.fillAll (colours::widget);
        const auto r = plotArea();

        // grid
        g.setColour (colours::panelEdge);
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * 0.5f), r.getX(), r.getRight());
        g.drawHorizontalLine ((int) r.getBottom(), r.getX(), r.getRight());

        const Layout L = layout();
        const float a = value (attack), d = value (decay), s = value (sustain), rel = value (release);

        // simulate the real envelope: 1 kHz is plenty for a picture
        constexpr double simRate = 1000.0;
        wf::Envelope env;
        env.setSampleRate (simRate);
        env.setParameters (a, d, s, rel);
        env.noteOn();
        const float offMs = a + d + L.holdMs;
        juce::Path p;
        p.startNewSubPath (r.getX(), r.getBottom());
        const int steps = juce::jmax (2, (int) (L.totalMs));   // one per ms
        bool released = false;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i * L.totalMs / (float) steps;
            if (! released && t >= offMs) { env.noteOff(); released = true; }
            const float lvl = env.process();
            p.lineTo (xFor (t, L, r), yFor (lvl, r));
        }
        auto fill = p;
        fill.lineTo (r.getRight(), r.getBottom());
        fill.closeSubPath();
        g.setColour (accent.withAlpha (0.18f));
        g.fillPath (fill);
        g.setColour (accent);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // stage boundaries + handles
        g.setColour (colours::textDim.withAlpha (0.5f));
        for (float t : { a, a + d, offMs })
            g.drawVerticalLine ((int) xFor (t, L, r), r.getY(), r.getBottom());

        const juce::Point<float> handles[3] = { { xFor (a, L, r), yFor (1.0f, r) },
                                                { xFor (a + d, L, r), yFor (s, r) },
                                                { xFor (offMs + rel, L, r), yFor (0.0f, r) } };
        for (int i = 0; i < 3; ++i)
        {
            const bool hot = i == dragHandle || i == hoverHandle;
            g.setColour (hot ? juce::Colours::white : accent);
            g.fillEllipse (juce::Rectangle<float> (hot ? 9.0f : 7.0f, hot ? 9.0f : 7.0f).withCentre (handles[i]));
        }

        // live position of the newest voice
        if (live.active.load (std::memory_order_relaxed))
        {
            const float held = live.heldMs.load (std::memory_order_relaxed);
            const float relT = live.releaseMs.load (std::memory_order_relaxed);
            const float lvl = live.level[(size_t) index].load (std::memory_order_relaxed);
            const float t = relT < 0.0f ? juce::jmin (held, offMs) : offMs + juce::jmin (relT, rel * 1.2f);
            const auto pt = juce::Point<float> (xFor (t, L, r), yFor (lvl, r));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pt));
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawVerticalLine ((int) pt.x, pt.y, r.getBottom());
        }

        // labels
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("A " + ms (a) + "  D " + ms (d) + "  S " + juce::String (s, 2) + "  R " + ms (rel),
                    getLocalBounds().reduced (6, 2), juce::Justification::topLeft);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (full.reduced (0.5f), 6.0f, 1.0f);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = handleAt (e.position);
        if (h != hoverHandle) { hoverHandle = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { hoverHandle = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragHandle = handleAt (e.position);
        if (dragHandle < 0)
            return;
        dragLayout = layout();                       // keep the time scale fixed while dragging
        dragStart = e.position;
        startA = value (attack); startD = value (decay); startS = value (sustain); startR = value (release);
        for (auto* p : paramsForHandle (dragHandle)) p->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragHandle < 0)
            return;
        const auto r = plotArea();
        const float dMs = (e.position.x - dragStart.x) * dragLayout.totalMs / r.getWidth();
        switch (dragHandle)
        {
            case 0: setValue (attack, startA + dMs); break;
            case 1: setValue (decay, startD + dMs);
                    setValue (sustain, startS + (dragStart.y - e.position.y) / r.getHeight()); break;
            case 2: setValue (release, startR + dMs); break;
            default: break;
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragHandle < 0)
            return;
        for (auto* p : paramsForHandle (dragHandle)) p->endChangeGesture();
        dragHandle = -1;
        repaint();
    }

private:
    struct Layout { float holdMs = 0.0f, totalMs = 1.0f; };

    Layout layout() const
    {
        const float a = value (attack), d = value (decay), rel = value (release);
        Layout L;
        L.holdMs = juce::jmax (60.0f, 0.2f * (a + d + rel));   // visible sustain plateau
        L.totalMs = juce::jmax (1.0f, a + d + L.holdMs + rel);
        return L;
    }

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (8.0f, 16.0f).withTrimmedBottom (-6.0f); }
    static float xFor (float ms, const Layout& L, juce::Rectangle<float> r) { return r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, ms / L.totalMs); }
    static float yFor (float level, juce::Rectangle<float> r) { return r.getBottom() - juce::jlimit (0.0f, 1.0f, level) * r.getHeight(); }

    int handleAt (juce::Point<float> pt) const
    {
        const auto r = plotArea();
        const Layout L = layout();
        const float a = value (attack), d = value (decay), s = value (sustain), rel = value (release);
        const juce::Point<float> handles[3] = { { xFor (a, L, r), yFor (1.0f, r) },
                                                { xFor (a + d, L, r), yFor (s, r) },
                                                { xFor (a + d + L.holdMs + rel, L, r), yFor (0.0f, r) } };
        int best = -1; float bestDist = 12.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float dist = handles[i].getDistanceFrom (pt);
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
        return best;
    }

    std::vector<juce::RangedAudioParameter*> paramsForHandle (int h) const
    {
        switch (h)
        {
            case 0:  return { attack };
            case 1:  return { decay, sustain };
            case 2:  return { release };
            default: return {};
        }
    }

    static float value (const juce::RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); }
    static void setValue (juce::RangedAudioParameter* p, float v)
    {
        p->setValueNotifyingHost (p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (v)));
    }
    static juce::String ms (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + "s" : juce::String ((int) v) + "ms"; }

    void timerCallback() override
    {
        if (! isShowing())
            return;
        const float a = value (attack), d = value (decay), s = value (sustain), rel = value (release);
        const bool liveNow = live.active.load (std::memory_order_relaxed);
        if (a != lastA || d != lastD || s != lastS || rel != lastR || liveNow || wasLive)
        {
            lastA = a; lastD = d; lastS = s; lastR = rel;
            wasLive = liveNow;
            repaint();
        }
    }

    const int index;
    const wf::EnvDisplay& live;
    juce::Colour accent;
    juce::RangedAudioParameter *attack = nullptr, *decay = nullptr, *sustain = nullptr, *release = nullptr;
    int dragHandle = -1, hoverHandle = -1;
    Layout dragLayout;
    juce::Point<float> dragStart;
    float startA = 0.0f, startD = 0.0f, startS = 0.0f, startR = 0.0f;
    float lastA = -1.0f, lastD = -1.0f, lastS = -1.0f, lastR = -1.0f;
    bool wasLive = false;
};

} // namespace ui
