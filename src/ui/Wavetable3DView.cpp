#include "Wavetable3DView.h"

namespace ui
{

Wavetable3DView::Wavetable3DView (juce::Colour accentIn) : LineStack3DView (accentIn) {}

void Wavetable3DView::setTable (const wf::Wavetable* table)
{
    if (table == displayedTable)
        return;
    displayedTable = table;
    pendingTable.store (table, std::memory_order_release);
    requestVertexRefill();
    repaint();
}

void Wavetable3DView::setPosition (float pos01)
{
    position.store (pos01, std::memory_order_relaxed);   // the overlay line follows it
}

void Wavetable3DView::setMode (Mode m)
{
    mode = m;
    setGLRenderingEnabled (m == mode3D);
    repaint();
}

void Wavetable3DView::timerTick()
{
    // 2D / spectrum modes are painted with juce: repaint only when the
    // position moved so an idle view costs nothing.
    if (mode != mode3D && std::abs (position.load() - lastPaintedPosition) > 1.0e-4f)
        repaint();
}

//==============================================================================
bool Wavetable3DView::buildVertices (std::vector<float>& verts, int& outLines, int& outPoints)
{
    const auto* table = pendingTable.load (std::memory_order_acquire);
    if (table == nullptr)
        return false;

    const int frames = table->getNumFrames();
    outLines = frames > 1 ? juce::jmax (frames, minLines) : 1;
    outPoints = pointsPerFrame;
    verts.resize ((size_t) outLines * pointsPerFrame * 3);

    // x: sample position, y: amplitude, z: depth (front = frame 0). Lines
    // between frames are interpolated exactly like the oscillator does.
    std::vector<float> frame;
    size_t k = 0;
    for (int line = 0; line < outLines; ++line)
    {
        const float pos = outLines > 1 ? (float) line / (float) (outLines - 1) : 0.0f;
        interpolatedFrame (*table, pos, frame);
        const float z = -1.0f + 2.0f * pos;
        for (int i = 0; i < pointsPerFrame; ++i)
        {
            verts[k++] = -1.0f + 2.0f * (float) i / (float) (pointsPerFrame - 1);
            verts[k++] = frame[(size_t) i] * 0.4f;
            verts[k++] = z;
        }
    }
    return true;
}

bool Wavetable3DView::buildOverlayLine (std::vector<float>& verts, int& points)
{
    const auto* table = pendingTable.load (std::memory_order_acquire);
    if (table == nullptr)
        return false;
    const float pos = position.load();
    std::vector<float> frame;
    interpolatedFrame (*table, pos, frame);
    points = pointsPerFrame;
    verts.resize ((size_t) points * 3);
    const float z = table->getNumFrames() > 1 ? -1.0f + 2.0f * pos : 0.0f;
    size_t k = 0;
    for (int i = 0; i < points; ++i)
    {
        verts[k++] = -1.0f + 2.0f * (float) i / (float) (points - 1);
        verts[k++] = frame[(size_t) i] * 0.4f;
        verts[k++] = z;
    }
    return true;
}

// `out` gets pointsPerFrame samples (clamped to +-1) of the frame at pos01.
void Wavetable3DView::interpolatedFrame (const wf::Wavetable& t, float pos01, std::vector<float>& out)
{
    const int frames = t.getNumFrames();
    const float p = juce::jlimit (0.0f, 1.0f, pos01) * (float) juce::jmax (0, frames - 1);
    const int f0 = juce::jlimit (0, frames - 1, (int) p);
    const int f1 = juce::jmin (f0 + 1, frames - 1);
    const float mix = p - (float) f0;
    const float* a = t.getRawFrame (f0);
    const float* b = t.getRawFrame (f1);
    const int step = wf::Wavetable::frameSize / pointsPerFrame;
    out.resize ((size_t) pointsPerFrame);
    for (int i = 0; i < pointsPerFrame; ++i)
    {
        const int s = i * step;
        out[(size_t) i] = juce::jlimit (-1.0f, 1.0f, a[s] + (b[s] - a[s]) * mix);
    }
}

void Wavetable3DView::currentFrameSamples (std::vector<float>& out) const
{
    constexpr int N = wf::Wavetable::frameSize;
    out.assign ((size_t) N, 0.0f);
    if (displayedTable == nullptr)
        return;
    const int frames = displayedTable->getNumFrames();
    const float p = position.load() * (float) juce::jmax (0, frames - 1);
    const int f0 = juce::jlimit (0, frames - 1, (int) p);
    const int f1 = juce::jmin (f0 + 1, frames - 1);
    const float mix = p - (float) f0;
    const float* a = displayedTable->getRawFrame (f0);
    const float* b = displayedTable->getRawFrame (f1);
    for (int i = 0; i < N; ++i)
        out[(size_t) i] = a[i] + (b[i] - a[i]) * mix;
}

void Wavetable3DView::refreshSpectrumIfNeeded()
{
    const float pos = position.load();
    if (displayedTable == spectrumTable && std::abs (pos - spectrumPosition) < 1.0e-4f)
        return;
    spectrumTable = displayedTable;
    spectrumPosition = pos;
    std::vector<float> frame;
    currentFrameSamples (frame);
    wf::WaveEditOps::analyse (frame.data(), spectrum);
}

//==============================================================================
void Wavetable3DView::paintFallback (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (6.0f, 18.0f);
    lastPaintedPosition = position.load();
    if (displayedTable == nullptr)
        return;

    if (mode == modeSpectrum)
    {
        refreshSpectrumIfNeeded();
        constexpr int shown = 64;
        const float bw = r.getWidth() / (float) shown;
        g.setColour (colours::panelEdge);
        g.drawHorizontalLine ((int) r.getBottom(), r.getX(), r.getRight());
        for (int k = 1; k <= shown; ++k)
        {
            const float m = juce::jlimit (0.0f, 1.0f, spectrum.magnitude[(size_t) k]);
            const float hgt = m * r.getHeight();
            g.setColour (accent.withAlpha (0.35f + 0.65f * m));
            g.fillRect (r.getX() + bw * (float) (k - 1) + 1.0f, r.getBottom() - hgt, bw - 2.0f, hgt);
        }
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (10.0f));
        for (int k = 8; k <= shown; k += 8)
            g.drawText (juce::String (k), (int) (r.getX() + bw * (float) (k - 1) - 10.0f), (int) r.getBottom() + 2, 22, 12,
                        juce::Justification::centred);
        return;
    }

    // 2D: the interpolated current frame, full width
    std::vector<float> frame;
    currentFrameSamples (frame);
    g.setColour (colours::panelEdge);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    for (int q = 1; q < 4; ++q)
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) q / 4.0f), r.getY(), r.getBottom());

    juce::Path p;
    const int N = wf::Wavetable::frameSize;
    const int step = juce::jmax (1, N / juce::jmax (64, (int) r.getWidth() * 2));
    for (int i = 0; i < N; i += step)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) N;
        const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, frame[(size_t) i]) * r.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    p.lineTo (r.getRight(), r.getCentreY() - juce::jlimit (-1.0f, 1.0f, frame[0]) * r.getHeight() * 0.48f);
    g.setColour (accent);
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Wavetable3DView::paintOverlay (juce::Graphics& g)
{
    if (displayedTable == nullptr)
        return;
    const int frames = displayedTable->getNumFrames();
    const int current = juce::roundToInt (position.load() * (float) (frames - 1)) + 1;
    g.setColour (colours::text.withAlpha (0.75f));
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (displayedTable->getName() + "   " + juce::String (current) + " / " + juce::String (frames)
                    + (mode == modeSpectrum ? "   harmonics" : ""),
                getLocalBounds().reduced (8, 5), juce::Justification::topLeft);
}

} // namespace ui
