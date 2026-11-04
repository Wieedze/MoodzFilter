#include "plugin/Parameters.h"

#include "dsp/FilterModels.h"
#include "dsp/Oversampler.h"
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

    juce::String slopeToText (float db)
    {
        // Whole values without a decimal, so the default reads "24 dB/oct"
        const auto rounded = std::round (db * 10.0f) / 10.0f;
        const auto number = juce::approximatelyEqual (rounded, std::round (rounded)) ? juce::String (juce::roundToInt (rounded)) : juce::String (rounded, 1);
        return number + " dB/oct";
    }

    namespace
    {
        // A choice index from its raw float value, within [0, count - 1]: NaN, infinity and
        // out-of-range values are bounded in float before the conversion to int
        int toChoiceIndex (float rawValue, std::size_t count) noexcept
        {
            if (!std::isfinite (rawValue))
                return 0;
            const auto last = static_cast<float> (count - 1);
            return static_cast<int> (std::clamp (rawValue, 0.0f, last) + 0.5f);
        }
    }

    int toModelIndex (float rawValue) noexcept
    {
        return toChoiceIndex (rawValue, dsp::filterSetupCount);
    }

    int toVintageVariantIndex (float rawValue) noexcept
    {
        return toChoiceIndex (rawValue, dsp::vintageVariantLabels.size());
    }

    int toOversamplingIndex (float rawValue) noexcept
    {
        return toChoiceIndex (rawValue, dsp::Oversampler::factorCount);
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
            juce::ParameterID { id::gainOut, version::task0003 },
            "Gain out",
            gainOutRange,
            0.0f,
            juce::AudioParameterFloatAttributes {}
                .withStringFromValueFunction ([] (float db, int) { return decibelsToText (db); })
                .withValueFromStringFunction ([] (const juce::String& text) { return textToDecibels (text); })));

        // ADRs 0006, 0007: every model with each passband it has, in the frozen order,
        // default Cytomic SVF Low-pass
        juce::StringArray modelNames;
        for (const auto& setup : dsp::filterSetups())
            modelNames.add (setup.label);
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id::filterModel, version::task0008 }, "Model", modelNames, 0));

        // SPEC: Cutoff 20 Hz to 20 kHz, logarithmic, default 1 kHz. Skew centred on
        // sqrt(20 * 20000) = 632 Hz, so equal control travel gives equal frequency ratios.
        juce::NormalisableRange<float> cutoffRange { dsp::MultimodeFilter::minCutoffHz, dsp::MultimodeFilter::maxCutoffHz };
        cutoffRange.setSkewForCentre (std::sqrt (dsp::MultimodeFilter::minCutoffHz * dsp::MultimodeFilter::maxCutoffHz));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::cutoff, version::task0008 },
            "Cutoff",
            cutoffRange,
            1000.0f,
            juce::AudioParameterFloatAttributes {}
                .withStringFromValueFunction ([] (float hz, int) { return hertzToText (hz); })
                .withValueFromStringFunction ([] (const juce::String& text) { return textToHertz (text); })));

        // SPEC: Resonance 0 to 100 %, default 20 %
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::resonance, version::task0008 },
            "Resonance",
            juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
            20.0f,
            juce::AudioParameterFloatAttributes {}.withLabel ("%")));

        // ADR 0008: per-model settings, ignored by a model without the setting. Defaults give
        // back the task 0009 sound. Slope: OB-Xd 4-pole continuously (Morph), Diode Ladder at the
        // nearest 6 dB step.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::slope, version::task0010 },
            "Slope",
            juce::NormalisableRange<float> { dsp::minSlopeDb, dsp::maxSlopeDb, 0.1f },
            dsp::maxSlopeDb,
            juce::AudioParameterFloatAttributes {}
                .withStringFromValueFunction ([] (float db, int) { return slopeToText (db); })
                .withValueFromStringFunction ([] (const juce::String& text) { return text.trim().getFloatValue(); })));

        // K35 saturation 0 to 100 %, sst-filters 0 to 4; 25 % is Mild
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::k35Saturation, version::task0010 },
            "K35 saturation",
            juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
            dsp::defaultK35Saturation * 100.0f,
            juce::AudioParameterFloatAttributes {}.withLabel ("%")));

        juce::StringArray variantNames;
        for (const auto* label : dsp::vintageVariantLabels)
            variantNames.add (label);
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id::vintageVariant, version::task0010 }, "Vintage variant", variantNames, dsp::defaultVintageVariant));

        // SPEC, ADR 0009: Off, 2x, 4x around the filter, default 2x. One latency for the three.
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id::oversampling, version::task0012 }, "Oversampling", juce::StringArray { "Off", "2x", "4x" }, 1));

        return layout;
    }

    Snapshotter::Snapshotter (const juce::AudioProcessorValueTreeState& state)
        : gainOut (state.getRawParameterValue (id::gainOut)),
          filterModel (state.getRawParameterValue (id::filterModel)),
          cutoff (state.getRawParameterValue (id::cutoff)),
          resonance (state.getRawParameterValue (id::resonance)),
          slope (state.getRawParameterValue (id::slope)),
          k35Saturation (state.getRawParameterValue (id::k35Saturation)),
          vintageVariant (state.getRawParameterValue (id::vintageVariant)),
          oversampling (state.getRawParameterValue (id::oversampling))
    {
        jassert (gainOut != nullptr && filterModel != nullptr && cutoff != nullptr && resonance != nullptr);
        jassert (slope != nullptr && k35Saturation != nullptr && vintageVariant != nullptr && oversampling != nullptr);
    }

    Params Snapshotter::snapshot() const noexcept
    {
        constexpr auto relaxed = std::memory_order_relaxed;
        return Params {
            gainOut->load (relaxed),
            toModelIndex (filterModel->load (relaxed)),
            cutoff->load (relaxed),
            resonance->load (relaxed) / 100.0f,
            slope->load (relaxed),
            k35Saturation->load (relaxed) / 100.0f,
            toVintageVariantIndex (vintageVariant->load (relaxed)),
            toOversamplingIndex (oversampling->load (relaxed)),
        };
    }
}
