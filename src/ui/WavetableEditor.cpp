#include "WavetableEditor.h"

namespace ui
{

namespace Ops = wf::WaveEditOps;

WavetableEditor::WavetableEditor (WaveForgeProcessor& p, int oscIndex)
    : processor (p), osc (oscIndex),
      applyButton (oscIndex == 0 ? "Apply to OSC A" : "Apply to OSC B"),
      wave (*this), bars (*this), strip (*this)
{
    // Start from the oscillator's current table.
    if (const auto* t = processor.getOscTable (osc))
    {
        numFrames = t->getNumFrames();
        frames.assign ((size_t) numFrames * N, 0.0f);
        for (int f = 0; f < numFrames; ++f)
            std::copy_n (t->getRawFrame (f), N, frame (f));
        nameEditor.setText (t->getSourceId().startsWith ("builtin:") ? t->getName() + " Edit" : t->getName(),
                            juce::dontSendNotification);
    }
    else
    {
        frames.assign ((size_t) N, 0.0f);
        wf::WavetableLoader::basicShape ("Sine", frame (0));
        nameEditor.setText ("User Table", juce::dontSendNotification);
    }

    title.setText ("WAVETABLE EDITOR  -  " + juce::String (osc == 0 ? "OSC A" : "OSC B"), juce::dontSendNotification);
    title.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, osc == 0 ? colours::accent : colours::accentB);
    addAndMakeVisible (title);

    hint.setText ("Drag in the wave to draw (Shift = straight line). Drag the bars to set harmonics. "
                  "Click the strip to pick a frame.", juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, colours::textDim);
    hint.setFont (juce::FontOptions (11.5f));
    addAndMakeVisible (hint);

    status.setColour (juce::Label::textColourId, colours::accentFx);
    status.setFont (juce::FontOptions (11.5f));
    status.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (status);

    nameEditor.setFont (juce::FontOptions (13.0f));
    nameEditor.setColour (juce::TextEditor::backgroundColourId, colours::widget);
    nameEditor.setColour (juce::TextEditor::outlineColourId, colours::panelEdge);
    nameEditor.setColour (juce::TextEditor::textColourId, colours::text);
    addAndMakeVisible (nameEditor);

    frameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (frameLabel);

    auto button = [this] (juce::TextButton& b, std::function<void()> fn)
    {
        b.onClick = std::move (fn);
        addAndMakeVisible (b);
    };

    button (prevButton, [this] { selectFrame (current - 1); });
    button (nextButton, [this] { selectFrame (current + 1); });
    button (addButton, [this]
    {
        if (numFrames >= wf::Wavetable::maxFrames) return;
        frames.insert (frames.begin() + (size_t) (current + 1) * N, (size_t) N, 0.0f);
        ++numFrames;
        selectFrame (current + 1);
        frameEdited();
    });
    button (dupButton, [this]
    {
        if (numFrames >= wf::Wavetable::maxFrames) return;
        std::vector<float> copy (frame (current), frame (current) + N);
        frames.insert (frames.begin() + (size_t) (current + 1) * N, copy.begin(), copy.end());
        ++numFrames;
        selectFrame (current + 1);
        frameEdited();
    });
    button (delButton, [this]
    {
        if (numFrames <= 1) return;
        frames.erase (frames.begin() + (size_t) current * N, frames.begin() + (size_t) (current + 1) * N);
        --numFrames;
        selectFrame (juce::jmin (current, numFrames - 1));
        frameEdited();
    });
    button (morphButton, [this]
    {
        Ops::morph (frames.data(), numFrames, 0, numFrames - 1);
        frameEdited();
        status.setText ("Morphed frames 1 -> " + juce::String (numFrames), juce::dontSendNotification);
    });

    auto shape = [this] (juce::TextButton& b, const char* name)
    {
        b.onClick = [this, name] { wf::WavetableLoader::basicShape (name, frame (current)); frameEdited(); };
        addAndMakeVisible (b);
    };
    shape (sineButton, "Sine"); shape (triButton, "Triangle"); shape (sawButton, "Saw");
    shape (squareButton, "Square"); shape (pulseButton, "Pulse");

    button (normButton,    [this] { Ops::normalise (frame (current)); frameEdited(); });
    button (invertButton,  [this] { Ops::invert (frame (current)); frameEdited(); });
    button (reverseButton, [this] { Ops::reverse (frame (current)); frameEdited(); });
    button (dcButton,      [this] { Ops::removeDc (frame (current)); frameEdited(); });
    button (smoothButton,  [this] { Ops::smooth (frame (current)); frameEdited(); });
    button (clearButton,   [this] { std::fill_n (frame (current), N, 0.0f); frameEdited(); });

    button (applyButton, [this] { applyToOsc(); });
    button (saveButton,  [this] { saveAs(); });
    button (closeButton, [this] { if (onClose) onClose(); });
    applyButton.setColour (juce::TextButton::buttonColourId, colours::accentFx.withAlpha (0.35f));

    addAndMakeVisible (wave);
    addAndMakeVisible (bars);
    addAndMakeVisible (strip);

    selectFrame (0);
}

//==============================================================================
void WavetableEditor::selectFrame (int index)
{
    current = juce::jlimit (0, numFrames - 1, index);
    Ops::analyse (frame (current), harmonics);
    refreshLabels();
    wave.repaint(); bars.repaint(); strip.repaint();
}

void WavetableEditor::frameEdited()
{
    Ops::analyse (frame (current), harmonics);
    refreshLabels();
    wave.repaint(); bars.repaint(); strip.repaint();
}

void WavetableEditor::harmonicsEdited()
{
    Ops::synthesise (harmonics, frame (current));
    if (Ops::peak (frame (current)) > 1.0f)   // keep the wave inside +-1
        Ops::normalise (frame (current));
    Ops::analyse (frame (current), harmonics);
    refreshLabels();
    wave.repaint(); bars.repaint(); strip.repaint();
}

void WavetableEditor::refreshLabels()
{
    frameLabel.setText ("Frame " + juce::String (current + 1) + " / " + juce::String (numFrames), juce::dontSendNotification);
    delButton.setEnabled (numFrames > 1);
    addButton.setEnabled (numFrames < wf::Wavetable::maxFrames);
    dupButton.setEnabled (numFrames < wf::Wavetable::maxFrames);
    morphButton.setEnabled (numFrames > 2);
}

void WavetableEditor::applyToOsc()
{
    auto name = juce::File::createLegalFileName (nameEditor.getText().trim());
    if (name.isEmpty())
        name = "User Table";
    const auto file = WaveForgeProcessor::userWavetableDirectory().getChildFile (name + ".wav");

    juce::String error;
    if (! wf::WavetableLoader::saveFile (frames.data(), numFrames, file, error)
        || ! processor.loadWavetableFile (osc, file, error, true))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Apply wavetable", error);
        return;
    }
    status.setText ("Applied to " + juce::String (osc == 0 ? "OSC A" : "OSC B") + "  (" + file.getFileName() + ")",
                    juce::dontSendNotification);
}

void WavetableEditor::saveAs()
{
    const auto dir = WaveForgeProcessor::userWavetableDirectory();
    dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save wavetable (.wav, Serum compatible)",
                                                   dir.getChildFile (nameEditor.getText().trim() + ".wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File()) return;
                              if (file.getFileExtension().isEmpty()) file = file.withFileExtension (".wav");
                              juce::String error;
                              if (! wf::WavetableLoader::saveFile (frames.data(), numFrames, file, error))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save wavetable", error);
                              else
                                  status.setText ("Saved " + file.getFullPathName(), juce::dontSendNotification);
                          });
}

//==============================================================================
void WavetableEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

void WavetableEditor::resized()
{
    auto r = getLocalBounds().reduced (10, 8);

    auto top = r.removeFromTop (26);
    title.setBounds (top.removeFromLeft (230));
    closeButton.setBounds (top.removeFromRight (70).reduced (2, 1));
    saveButton.setBounds (top.removeFromRight (100).reduced (2, 1));
    applyButton.setBounds (top.removeFromRight (130).reduced (2, 1));
    status.setBounds (top);

    r.removeFromTop (4);
    auto row1 = r.removeFromTop (26);
    nameEditor.setBounds (row1.removeFromLeft (200).reduced (0, 2));
    row1.removeFromLeft (10);
    prevButton.setBounds (row1.removeFromLeft (28).reduced (2, 1));
    frameLabel.setBounds (row1.removeFromLeft (110));
    nextButton.setBounds (row1.removeFromLeft (28).reduced (2, 1));
    row1.removeFromLeft (8);
    for (auto* b : { &addButton, &dupButton, &delButton })
        b->setBounds (row1.removeFromLeft (52).reduced (2, 1));
    morphButton.setBounds (row1.removeFromLeft (100).reduced (2, 1));
    row1.removeFromLeft (10);
    hint.setBounds (row1);

    r.removeFromTop (6);
    auto row2 = r.removeFromTop (26);
    for (auto* b : { &sineButton, &triButton, &sawButton, &squareButton, &pulseButton })
        b->setBounds (row2.removeFromLeft (62).reduced (2, 1));
    row2.removeFromLeft (14);
    for (auto* b : { &normButton, &invertButton, &reverseButton, &dcButton, &smoothButton, &clearButton })
        b->setBounds (row2.removeFromLeft (84).reduced (2, 1));

    r.removeFromTop (6);
    strip.setBounds (r.removeFromBottom (64));
    r.removeFromBottom (6);
    bars.setBounds (r.removeFromBottom (juce::jmax (90, r.getHeight() / 3)));
    r.removeFromBottom (6);
    wave.setBounds (r);
}

//==============================================================================
void WavetableEditor::WaveCanvas::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::widget);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::panelEdge);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    for (int q = 1; q < 8; ++q)
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) q / 8.0f), r.getY(), r.getBottom());

    const float* f = editor.frame (editor.current);
    juce::Path p;
    const int step = juce::jmax (1, N / juce::jmax (64, (int) r.getWidth() * 2));
    for (int i = 0; i < N; i += step)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) N;
        const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, f[i]) * r.getHeight() * 0.46f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (editor.osc == 0 ? colours::accent : colours::accentB);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

void WavetableEditor::WaveCanvas::drawTo (juce::Point<float> pt, bool first)
{
    const auto r = getLocalBounds().toFloat();
    const int index = juce::jlimit (0, N - 1, (int) ((pt.x - r.getX()) / r.getWidth() * (float) N));
    const float value = juce::jlimit (-1.0f, 1.0f, (r.getCentreY() - pt.y) / (r.getHeight() * 0.46f));
    float* f = editor.frame (editor.current);

    if (first || lastIndex < 0)
    {
        f[index] = value;
    }
    else
    {
        // fill every sample between the previous and the current mouse position
        const int a = lastIndex, b = index;
        const int lo = juce::jmin (a, b), hi = juce::jmax (a, b);
        for (int i = lo; i <= hi; ++i)
        {
            const float t = hi == lo ? 1.0f : (float) (i - a) / (float) (b - a);
            f[i] = lastValue + (value - lastValue) * t;
        }
    }
    lastIndex = index;
    lastValue = value;
}

void WavetableEditor::WaveCanvas::mouseDown (const juce::MouseEvent& e)
{
    lastIndex = -1;
    lineStart = e.position;
    drawTo (e.position, true);
    editor.frameEdited();
}

void WavetableEditor::WaveCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isShiftDown())
    {
        // straight line from the press point: redraw from the start each time
        lastIndex = -1;
        drawTo (lineStart, true);
    }
    drawTo (e.position, false);
    editor.frameEdited();
}

//==============================================================================
void WavetableEditor::HarmonicCanvas::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::widget);
    g.fillRoundedRectangle (r, 6.0f);
    auto area = r.reduced (4.0f, 14.0f).withTrimmedBottom (2.0f);
    const float bw = area.getWidth() / (float) shown;
    for (int k = 1; k <= shown; ++k)
    {
        const float m = juce::jlimit (0.0f, 1.0f, editor.harmonics.magnitude[(size_t) k]);
        const float hgt = m * area.getHeight();
        g.setColour (colours::accentMod.withAlpha (0.35f + 0.65f * m));
        g.fillRect (area.getX() + bw * (float) (k - 1) + 1.0f, area.getBottom() - hgt, bw - 2.0f, hgt);
    }
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("HARMONICS 1 - " + juce::String (shown), getLocalBounds().reduced (8, 2), juce::Justification::topLeft);
    for (int k = 8; k <= shown; k += 8)
        g.drawText (juce::String (k), (int) (area.getX() + bw * (float) (k - 1) - 10.0f), (int) area.getBottom() + 1, 22, 12,
                    juce::Justification::centred);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

void WavetableEditor::HarmonicCanvas::mouseDrag (const juce::MouseEvent& e)
{
    auto area = getLocalBounds().toFloat().reduced (4.0f, 14.0f).withTrimmedBottom (2.0f);
    const float bw = area.getWidth() / (float) shown;
    const int k = juce::jlimit (1, shown, 1 + (int) ((e.position.x - area.getX()) / bw));
    const float m = juce::jlimit (0.0f, 1.0f, (area.getBottom() - e.position.y) / area.getHeight());
    auto& h = editor.harmonics;
    if (h.magnitude[(size_t) k] < 1.0e-5f)
        h.phase[(size_t) k] = -juce::MathConstants<float>::halfPi;   // new partial starts as a sine
    h.magnitude[(size_t) k] = m;
    editor.harmonicsEdited();
}

//==============================================================================
void WavetableEditor::FrameStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::widget);
    g.fillRoundedRectangle (r, 6.0f);
    const int n = editor.numFrames;
    const float cw = r.getWidth() / (float) n;
    const int stride = juce::jmax (1, N / 32);
    for (int f = 0; f < n; ++f)
    {
        auto cell = juce::Rectangle<float> (r.getX() + cw * (float) f, r.getY(), cw, r.getHeight()).reduced (1.0f, 6.0f);
        if (f == editor.current)
        {
            g.setColour (colours::accent.withAlpha (0.25f));
            g.fillRoundedRectangle (cell, 3.0f);
        }
        if (cw < 6.0f)
            continue;   // too dense to draw waves: the selection highlight is enough
        const float* s = editor.frame (f);
        juce::Path p;
        for (int i = 0; i < N; i += stride)
        {
            const float x = cell.getX() + cell.getWidth() * (float) i / (float) N;
            const float y = cell.getCentreY() - juce::jlimit (-1.0f, 1.0f, s[i]) * cell.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (f == editor.current ? juce::Colours::white : colours::accent.withAlpha (0.6f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

void WavetableEditor::FrameStrip::mouseDrag (const juce::MouseEvent& e)
{
    const float cw = (float) getWidth() / (float) editor.numFrames;
    editor.selectFrame ((int) (e.position.x / cw));
}

} // namespace ui
