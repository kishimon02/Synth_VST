#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

// Small parameter-bound widgets and a panel base, shared by every page of the
// editor. Each widget owns its APVTS attachment, so a panel is just a list of
// these plus a layout.
namespace ui
{

using Apvts = juce::AudioProcessorValueTreeState;

class ParamKnob final : public juce::Component
{
public:
    ParamKnob (Apvts& apvts, const juce::String& paramId, const juce::String& text,
               juce::Colour accent = colours::accent)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 15);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, colours::textDim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attachment = std::make_unique<Apvts::SliderAttachment> (apvts, paramId, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        slider.setBounds (r);
    }

    juce::Slider slider;
    juce::Label  label;

private:
    std::unique_ptr<Apvts::SliderAttachment> attachment;
};

class ParamToggle final : public juce::ToggleButton
{
public:
    ParamToggle (Apvts& apvts, const juce::String& paramId, const juce::String& text)
        : juce::ToggleButton (text)
    {
        attachment = std::make_unique<Apvts::ButtonAttachment> (apvts, paramId, *this);
    }

private:
    std::unique_ptr<Apvts::ButtonAttachment> attachment;
};

class ParamCombo final : public juce::Component
{
public:
    ParamCombo (Apvts& apvts, const juce::String& paramId, const juce::String& text)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId)))
            box.addItemList (choice->choices, 1);
        addAndMakeVisible (box);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (label);

        attachment = std::make_unique<Apvts::ComboBoxAttachment> (apvts, paramId, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        box.setBounds (r.withHeight (22).withY (r.getY() + (r.getHeight() - 22) / 2).reduced (2, 0));
    }

    juce::ComboBox box;
    juce::Label    label;

private:
    std::unique_ptr<Apvts::ComboBoxAttachment> attachment;
};

// Rounded box with a title strip. Subclasses add controls and lay them out.
class Panel : public juce::Component
{
public:
    explicit Panel (const juce::String& titleText, juce::Colour accentIn = colours::accent)
        : accent (accentIn)
    {
        title.setText (titleText, juce::dontSendNotification);
        title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, accent);
        addAndMakeVisible (title);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    }

    static constexpr int titleHeight = 22;

    // Body area below the title, with the panel padding applied.
    juce::Rectangle<int> body()
    {
        auto r = getLocalBounds().reduced (8, 4);
        title.setBounds (r.removeFromTop (titleHeight));
        return r;
    }

    // Lays `items` out left to right, widths proportional to `weights`
    // (equal when empty). Knobs fill the cell; combos and toggles are centred
    // vertically at their natural height.
    static void row (juce::Rectangle<int> area, const std::vector<juce::Component*>& items,
                     const std::vector<float>& weights = {}, int gap = 4)
    {
        if (items.empty())
            return;
        float total = 0.0f;
        for (size_t i = 0; i < items.size(); ++i)
            total += i < weights.size() ? weights[i] : 1.0f;
        const int usable = area.getWidth() - gap * ((int) items.size() - 1);
        for (size_t i = 0; i < items.size(); ++i)
        {
            auto* c = items[i];
            const float wt = i < weights.size() ? weights[i] : 1.0f;
            auto cell = area.removeFromLeft (juce::roundToInt ((float) usable * wt / total));
            area.removeFromLeft (gap);
            if (dynamic_cast<ParamKnob*> (c) != nullptr)
                c->setBounds (cell);
            else if (dynamic_cast<ParamCombo*> (c) != nullptr)
                c->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 40));
            else
                c->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 24));
        }
    }

protected:
    juce::Label title;
    juce::Colour accent;
};

} // namespace ui
