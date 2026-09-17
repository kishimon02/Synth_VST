#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{

// Shared palette so panels, views and the look-and-feel agree.
namespace colours
{
    inline const juce::Colour background { 0xff1b1c22 };
    inline const juce::Colour panel      { 0xff24262f };
    inline const juce::Colour panelEdge  { 0xff30333f };
    inline const juce::Colour widget     { 0xff15161b };
    inline const juce::Colour text       { 0xffd8dae3 };
    inline const juce::Colour textDim    { 0xff8a8d9a };
    inline const juce::Colour accent     { 0xff4fc3f7 };   // OSC A / general
    inline const juce::Colour accentB    { 0xffffa726 };   // OSC B
    inline const juce::Colour accentMod  { 0xffab7bff };   // LFO / mod
    inline const juce::Colour accentFx   { 0xff66e0a3 };   // FX
}

// Dark, flat look: rotary knobs drawn as an arc with a pointer, compact
// combo boxes and toggles. Applied once by the editor.
class WaveForgeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    WaveForgeLookAndFeel()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Slider::rotarySliderFillColourId, accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, panelEdge);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::Slider::trackColourId, accent.withAlpha (0.7f));
        setColour (juce::Slider::backgroundColourId, widget);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, widget);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ComboBox::backgroundColourId, widget);
        setColour (juce::ComboBox::outlineColourId, panelEdge);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::ToggleButton::tickDisabledColourId, textDim);
        setColour (juce::TextButton::buttonColourId, widget);
        setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.5f));
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::TabbedButtonBar::tabOutlineColourId, panelEdge);
        setColour (juce::TabbedButtonBar::tabTextColourId, textDim);
        setColour (juce::TabbedButtonBar::frontTextColourId, text);
        setColour (juce::TabbedComponent::backgroundColourId, background);
        setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, panelEdge);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const float arcW = juce::jmax (2.0f, radius * 0.14f);
        const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // knob body
        g.setColour (colours::widget);
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        // track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - arcW * 0.5f, radius - arcW * 0.5f, 0.0f,
                             startAngle, endAngle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc. Bipolar sliders fill from the centre.
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float zeroPos = bipolar ? (float) ((0.0 - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum())) : 0.0f;
        const float fromAngle = startAngle + zeroPos * (endAngle - startAngle);
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius - arcW * 0.5f, radius - arcW * 0.5f, 0.0f,
                             juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
        g.setColour (slider.isEnabled() ? fill : fill.withAlpha (0.3f));
        g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // pointer
        const float inner = radius * 0.45f, outer = radius - arcW * 1.6f;
        juce::Line<float> pointer (centre.getPointOnCircumference (inner, angle),
                                   centre.getPointOnCircumference (outer, angle));
        g.setColour (colours::text);
        g.drawLine (pointer, 2.0f);
    }

    // Toggles are drawn as pill buttons: the whole cell lights up when on,
    // so the label never fights a checkbox for space in a narrow column.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down) override
    {
        juce::ignoreUnused (down);
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        const auto tick = b.findColour (juce::ToggleButton::tickColourId);

        g.setColour (on ? tick.withAlpha (0.85f) : colours::widget);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? tick : (highlighted ? colours::textDim : colours::panelEdge));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);

        g.setColour ((on ? colours::background : colours::text).withAlpha (b.isEnabled() ? 1.0f : 0.5f));
        g.setFont (juce::FontOptions (12.0f, on ? juce::Font::bold : juce::Font::plain));
        g.drawFittedText (b.getButtonText(), r.reduced (3.0f, 0.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.75f);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.5f)); }
    juce::Font getLabelFont (juce::Label&) override       { return juce::Font (juce::FontOptions (12.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.5f)); }

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown) override
    {
        juce::ignoreUnused (isMouseDown);
        const auto area = button.getActiveArea().toFloat();
        const bool front = button.isFrontTab();
        g.setColour (front ? colours::panel : (isMouseOver ? colours::panel.withAlpha (0.5f) : juce::Colours::transparentBlack));
        g.fillRoundedRectangle (area.reduced (1.0f), 6.0f);
        if (front)
        {
            g.setColour (colours::accent);
            g.fillRect (area.withHeight (3.0f).withY (area.getBottom() - 3.0f).reduced (8.0f, 0.0f));
        }
        g.setColour (front ? colours::text : colours::textDim);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (button.getButtonText(), area.toNearestInt(), juce::Justification::centred);
    }

    int getTabButtonBestWidth (juce::TabBarButton&, int) override { return 110; }
};

} // namespace ui
