#include "ParamCatalog.h"

namespace ai
{

namespace
{
    juce::String valueText (const juce::RangedAudioParameter& p, float value)
    {
        return p.getText (p.convertTo0to1 (value), 0);
    }

    float currentValue (const juce::RangedAudioParameter& p)
    {
        return p.convertFrom0to1 (p.getValue());
    }

    float defaultValue (const juce::RangedAudioParameter& p)
    {
        return p.convertFrom0to1 (p.getDefaultValue());
    }

    juce::String describeOne (const juce::RangedAudioParameter& p)
    {
        juce::String line = p.paramID + "  \"" + p.getName (64) + "\"  ";
        if (auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (&p))
        {
            line += "choice index 0.." + juce::String (choice->choices.size() - 1) + " [";
            for (int i = 0; i < choice->choices.size(); ++i)
                line += (i > 0 ? ", " : "") + juce::String (i) + "=" + choice->choices[i];
            line += "] default " + juce::String (choice->getIndex());
            return line;
        }
        if (dynamic_cast<const juce::AudioParameterBool*> (&p) != nullptr)
            return line + "bool 0/1 default " + juce::String ((int) defaultValue (p));
        const auto& range = p.getNormalisableRange();
        const bool integer = dynamic_cast<const juce::AudioParameterInt*> (&p) != nullptr;
        line += (integer ? "int " : "float ") + juce::String (range.start, integer ? 0 : 3) + ".."
              + juce::String (range.end, integer ? 0 : 3);
        const auto label = p.getLabel();
        if (label.isNotEmpty()) line += " " + label;
        line += " default " + juce::String (defaultValue (p), integer ? 0 : 3);
        return line;
    }
}

juce::String ParamCatalog::describe (juce::AudioProcessorValueTreeState& apvts)
{
    juce::String out;
    for (auto* param : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            out += describeOne (*ranged) + "\n";
    return out;
}

juce::String ParamCatalog::currentValues (juce::AudioProcessorValueTreeState& apvts, bool onlyNonDefault)
{
    auto* obj = new juce::DynamicObject();
    for (auto* param : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const float v = currentValue (*ranged);
            if (onlyNonDefault && std::abs (v - defaultValue (*ranged)) < 1.0e-4f)
                continue;
            obj->setProperty (ranged->paramID, (double) juce::roundToInt (v * 1000.0f) / 1000.0);
        }
    return juce::JSON::toString (juce::var (obj), true);
}

std::vector<ParamCatalog::Change> ParamCatalog::collect (juce::AudioProcessorValueTreeState& apvts, const juce::var& changes,
                                                         juce::StringArray& unknown, bool apply)
{
    std::vector<Change> result;
    const auto* arr = changes.getArray();
    if (arr == nullptr)
        return result;

    for (const auto& item : *arr)
    {
        const auto id = item.getProperty ("id", "").toString().trim();
        if (id.isEmpty())
            continue;
        auto* param = apvts.getParameter (id);
        if (param == nullptr)
        {
            unknown.addIfNotAlreadyThere (id);
            continue;
        }
        const auto rawValue = item.getProperty ("value", juce::var());
        if (! rawValue.isDouble() && ! rawValue.isInt() && ! rawValue.isInt64() && ! rawValue.isBool())
            continue;

        Change c;
        c.id = id;
        c.name = param->getName (64);
        c.oldValue = currentValue (*param);
        c.newValue = param->getNormalisableRange().snapToLegalValue ((float) (double) rawValue);
        c.oldText = valueText (*param, c.oldValue);
        c.newText = valueText (*param, c.newValue);
        if (std::abs (c.newValue - c.oldValue) < 1.0e-5f)
            continue;
        if (apply)
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (c.newValue));
            param->endChangeGesture();
        }
        result.push_back (c);
    }
    return result;
}

std::vector<ParamCatalog::Change> ParamCatalog::applyChanges (juce::AudioProcessorValueTreeState& apvts, const juce::var& changes,
                                                              juce::StringArray& unknown)
{
    return collect (apvts, changes, unknown, true);
}

std::vector<ParamCatalog::Change> ParamCatalog::previewChanges (juce::AudioProcessorValueTreeState& apvts, const juce::var& changes,
                                                                juce::StringArray& unknown)
{
    return collect (apvts, changes, unknown, false);
}

} // namespace ai
