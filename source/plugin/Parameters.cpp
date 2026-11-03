#include "plugin/Parameters.h"

#include "dsp/FilterModels.h"
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

    juce::String hertzToText (float hz)
    {
        // Rounded first, so that 999.99 (1 kHz after a trip through the control) reads 1.00 kHz
        if (juce::roundToInt (hz) < 1000)
            return juce::String (juce::roundToInt (hz)) + " Hz";
        return juce::String (hz / 1000.0f, 2) + " kHz";
    }

    float textToHertz (const juce::String& text)
    {
        const auto trimmed = text.trim();
        const auto value = trimmed.getFloatValue();
        return trimmed.containsIgnoreCase ("k") ? value * 1000.0f : value;
    }

    namespace
    {
        int toChoiceIndex (float rawValue, std::size_t count) noexcept
        {
            if (!std::isfinite (rawValue))
                return 0;
            const auto last = static_cast<float> (count - 1);
            return static_cast<int> (std::clamp (rawValue, 0.0f, last) + 0.5f);
        }
    }

    int toModelIndex (float rawValue) noexcept { return toChoiceIndex (rawValue, dsp::filterModelCount); }
    int toModeIndex (float rawValue) noexcept { return toChoiceIndex (rawValue, dsp::filterModeCount); }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // SPEC v1 amended by ADR 0004: Gain out from silence (-inf) to +6 dB, default 0 dB,
        // smoothed in the dsp layer. Skew centred on -12 dB so that the useful range
        // (about -24 to +6 dB) takes most of the control's travel.
        juce::NormalisableRange<float> gainOutRange { dsp::SmoothedGain::silenceFloorDb, dsp::SmoothedGain::maxDb, 0.1f };
        gainOutRange.setSkewForCentre (-12.0f);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::gainOut, 1 },
            "Gain out",
            gainOutRange,
            0.0f,
            juce::AudioParameterFloatAttributes {}
                .withStringFromValueFunction ([] (float db, int) { return decibelsToText (db); })
                .withValueFromStringFunction ([] (const juce::String& text) { return textToDecibels (text); })));

        // ADR 0006: the five models in their frozen order, default Model 1 (Cytomic SVF)
        juce::StringArray modelNames;
        for (const auto* label : dsp::filterModelLabels)
            modelNames.add (label);
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id::filterModel, 2 }, "Model", modelNames, 0));

        // SPEC: Cutoff 20 Hz to 20 kHz, logarithmic, default 1 kHz. Skew centred on
        // sqrt(20 * 20000) = 632 Hz, so equal control travel gives equal frequency ratios.
        juce::NormalisableRange<float> cutoffRange { dsp::MultimodeFilter::minCutoffHz, dsp::MultimodeFilter::maxCutoffHz };
        cutoffRange.setSkewForCentre (std::sqrt (dsp::MultimodeFilter::minCutoffHz * dsp::MultimodeFilter::maxCutoffHz));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::cutoff, 2 },
            "Cutoff",
            cutoffRange,
            1000.0f,
            juce::AudioParameterFloatAttributes {}
                .withStringFromValueFunction ([] (float hz, int) { return hertzToText (hz); })
                .withValueFromStringFunction ([] (const juce::String& text) { return textToHertz (text); })));

        // SPEC: Resonance 0 to 100 %, default 20 %
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::resonance, 2 },
            "Resonance",
            juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
            20.0f,
            juce::AudioParameterFloatAttributes {}.withLabel ("%")));

        // SPEC Mode, ADR 0006: the same four passbands for every model, default low pass; a
        // passband the model lacks plays low pass (dsp::filterSetupIndex)
        juce::StringArray modeNames;
        for (const auto* label : dsp::filterModeLabels)
            modeNames.add (label);
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id::filterMode, version }, "Mode", modeNames, 0));

        return layout;
    }

    Snapshotter::Snapshotter (const juce::AudioProcessorValueTreeState& state)
        : gainOut (state.getRawParameterValue (id::gainOut)),
          filterModel (state.getRawParameterValue (id::filterModel)),
          cutoff (state.getRawParameterValue (id::cutoff)),
          resonance (state.getRawParameterValue (id::resonance)),
          filterMode (state.getRawParameterValue (id::filterMode))
    {
        jassert (gainOut != nullptr && filterModel != nullptr && cutoff != nullptr && resonance != nullptr && filterMode != nullptr);
    }

    Params Snapshotter::snapshot() const noexcept
    {
        constexpr auto relaxed = std::memory_order_relaxed;
        return Params {
            gainOut->load (relaxed),
            toModelIndex (filterModel->load (relaxed)),
            cutoff->load (relaxed),
            resonance->load (relaxed) / 100.0f,
            toModeIndex (filterMode->load (relaxed)),
        };
    }
}
