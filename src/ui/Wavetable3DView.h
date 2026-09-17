#pragma once

#include "LineStack3DView.h"
#include "../dsp/Wavetable.h"
#include "../dsp/WaveEditOps.h"

namespace ui
{

// Serum-style wavetable view. Three modes:
//   3D       the table as a dense sheet of polylines (frames interpolated up
//            to minLines so a 16-frame table still reads as a surface), the
//            interpolated frame under the WT position drawn on top in white
//   2D       the frame under the WT position, interpolated, full width
//   Spectrum harmonic magnitudes of that frame as bars
class Wavetable3DView final : public LineStack3DView
{
public:
    enum Mode { mode3D = 0, mode2D, modeSpectrum };

    explicit Wavetable3DView (juce::Colour accent = colours::accent);

    // Message thread. The table must outlive the view (bank is append-only).
    void setTable (const wf::Wavetable* table);
    void setPosition (float pos01);
    void setMode (Mode m);
    Mode getMode() const noexcept { return mode; }

    static constexpr int pointsPerFrame = 512;
    static constexpr int minLines = 128;

private:
    bool buildVertices (std::vector<float>& verts, int& numLines, int& pointsPerLine) override;
    bool buildOverlayLine (std::vector<float>& verts, int& points) override;
    static void interpolatedFrame (const wf::Wavetable& t, float pos01, std::vector<float>& out);
    void paintFallback (juce::Graphics&) override;
    void paintOverlay (juce::Graphics&) override;
    void timerTick() override;

    void currentFrameSamples (std::vector<float>& out) const;   // interpolated frame at `position`
    void refreshSpectrumIfNeeded();

    std::atomic<const wf::Wavetable*> pendingTable { nullptr };
    const wf::Wavetable* displayedTable = nullptr;
    std::atomic<float> position { 0.0f };
    Mode mode = mode3D;

    // spectrum cache (message thread)
    wf::WaveEditOps::Harmonics spectrum;
    const wf::Wavetable* spectrumTable = nullptr;
    float spectrumPosition = -1.0f;
    float lastPaintedPosition = -1.0f;
};

} // namespace ui
