#include "plugin/Parameters.h"

#include "dsp/SmoothedGain.h"

namespace moodz::parameters
{
    juce::String decibelsToText (float db)
    {
        if (db <= dsp::SmoothedGain::silenceFloorDb)
            return "-inf";
        return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
    }

    float textToDecibels (const juce::String& text)
    {
        const auto trimmed = text.trim();
        if (trimmed.startsWithIgnoreCase ("-inf"))
            return dsp::SmoothedGain::silenceFloorDb;
        return trimmed.getFloatValue();
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // SPEC v1 amended by ADR 0004: Gain out from silence (-inf) to +6 dB, default 0 dB,
        // smoothed in the dsp layer. Skew centred on -12 dB so that the useful range
        // (about -24 to +6 dB) takes most of the control's travel.
        juce::NormalisableRange<float> gainOutRange { dsp::SmoothedGain::silenceFloorDb, dsp::SmoothedGain::maxDb, 0.1f };
        gainOutRange.setSkewForCentre (-12.0f);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::gainOut, version },
            "Gain out",
            gainOutRange,
            0.0f,
            juce::AudioParameterFloatAttributes {}
                .withStringFromValueFunction ([] (float db, int) { return decibelsToText (db); })
                .withValueFromStringFunction ([] (const juce::String& text) { return textToDecibels (text); })));

        return layout;
    }

    Snapshotter::Snapshotter (const juce::AudioProcessorValueTreeState& state)
        : gainOut (state.getRawParameterValue (id::gainOut))
    {
        jassert (gainOut != nullptr);
    }

    Params Snapshotter::snapshot() const noexcept
    {
        return Params { gainOut->load (std::memory_order_relaxed) };
    }
}
